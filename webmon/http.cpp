
#include "http.h"
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <sstream>
#include <fstream>

#include <vector>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cstdlib>
#include <cstdio>

#include "boost_wrap.h"
#include "data_store.h"
using namespace std;

using namespace llprof;

string url_decode(string src)
{
    string result;
    while(true)
    {
        string::size_type pp = src.find("%");
        if(pp == string::npos)
        {
            return result + src;
        }
        result += src.substr(0, pp);
        string byte = src.substr(pp+1, pp+3);
        int val;
        scanf(byte.c_str(), "%x", &val);
        result += (char)val;
        src = src.substr(pp+3);
    }
}

bool parse_query_string(map<string, string> &result, string query_string)
{
    string target;
    while(!query_string.empty())
    {
        string::size_type amp_pos = query_string.find("&");
        if(amp_pos == string::npos)
        {
            target = query_string;
            query_string = "";
        }
        else
        {
            target = query_string.substr(0, amp_pos);
            query_string = query_string.substr(amp_pos+1);
        }
        string::size_type eq_pos = target.find("=");
        if(eq_pos == string::npos)
            return false;
        string key = url_decode(target.substr(0, eq_pos));
        string value = url_decode(target.substr(eq_pos+1));
        result[key] = value;
    }
    return true;
}

bool recv_string(int sock, string &result, int maxlen)
{
    char *buf = new char[maxlen];
    int n = recv(sock, buf, maxlen, MSG_NOSIGNAL);
    if(n <= 0)
    {
        delete []buf;
        return false;
    }
    result = string(buf, n);
    delete []buf;
    return true;
}

bool send_string(int sock, const string &msg)
{
    size_t sent = 0;
    if(msg.length() == 0)
        return true;
    while(msg.length() > sent) 
    {
        int n = send(sock, msg.c_str() + sent, msg.length() - sent, 0);
        if(n <= 0)
        {
            return false;
        }
        sent += n;
    }
    return true;
}


bool parse_http_request_header(map<string, string> &result, string header)
{
    stringstream strm(header);
    string line;
    
    getline(strm, line);
	vector<string> v;
	algorithm::split( v, line, algorithm::is_space() );
    if(v.size() < 3)
        return false;
    
    result["!method"] = v[0];
    string::size_type qpos = v[1].find("?");
    if(qpos != string::npos)
    {
        result["!path"] = v[1].substr(0, qpos);
        result["!qs"] = v[1].substr(qpos+1);
    } else {
        result["!path"] = v[1];
        result["!qs"] = "";
    }
    result["!version"] = v[2];
    
    while(getline(strm, line))
    {
        if(line.empty())
            continue;
        if(line[line.length()-1] == '\r')
        {
            line = line.substr(0, line.length()-1);
        }
        string::size_type dpos = line.find(": ");
        if(dpos == string::npos)
            return false;
        
        string key = line.substr(0, dpos);
        string value = line.substr(dpos+2);
        result[key] = value;
    }
    return true;
}

string build_repsonce(string first_line, const map<string, string> &header, string body)
{
    stringstream resp;
    resp << first_line << "\r\n";
    
    for(map<string,string>::const_iterator iter = header.begin(); iter != header.end(); iter++)
    {
        resp << (*iter).first << ": " << (*iter).second << "\r\n";
    }
    resp << "Content-Length: " << lexical_cast<string>(body.length()) << "\r\n";
    resp << "\r\n";
    resp << body;
    return resp.str();
}

void set_default_resp_header(map<string,string> &header)
{
    header["Server"] = "llprof web server";
    header["Connection"] = "close";
    header["Content-Type"] = "text/plain";
}

string build_resp_200(string content_type, string body)
{
    map<string, string> header;
    set_default_resp_header(header);
    header["Content-Type"] = content_type;
    return build_repsonce("HTTP/1.1 200 OK", header, body);
}


string build_resp_404()
{
    map<string, string> header;
    set_default_resp_header(header);
    return build_repsonce("HTTP/1.1 404 Not Found", header, "404 Error");
}

string build_resp_500()
{
    map<string, string> header;
    set_default_resp_header(header);
    return build_repsonce("HTTP/1.1 500 Internal Server Error", header, "");
}

int get_fstream_size(ifstream &strm)
{
    ifstream::streampos cur = strm.tellg();
    strm.seekg(0, ios_base::end);
    int sz = strm.tellg();
    strm.seekg(cur, ios_base::beg);
    return sz;
}

string build_resp_file(string file_name)
{
    ifstream file(file_name.c_str(), ifstream::in);
    // cout << "Filename:" << file_name << endl;
    if(file.bad() || file.fail())
    {
        cout << "File is bad:" << endl;
        return build_resp_404();
    }
    
    string ext = file_name.substr(file_name.length()-3);
    string mime = "application/octet-stream";
    if(ext == "txt")
        mime = "text/plain";
    else if(ext == "tml")
        mime = "text/html";
    else if(ext == "htm")
        mime = "text/html";
    else if(ext == "css")
        mime = "text/css";
    else if(ext == ".js")
        mime = "text/javascript";

    int sz = get_fstream_size(file);
    int ssz = sz;
    vector<char> buf;
    buf.resize(sz);
    file.read(&buf[0], sz);
    string buf_str(&buf[0], ssz);
    return build_resp_200(mime, buf_str);
}

string build_resp_json(const stringstream &strm)
{
    return build_resp_200("text/javascript", strm.str());
}


int client_handler(int sock)
{
    static unsigned long long request_counter;
    request_counter++;

    string req_header_str, buf;
    string req_body_str;
    while(true)
    {
        if(!recv_string(sock, buf))
            return -1;
        int nrskip = 2;
        string::size_type rrpos = buf.find("\n\n");
        if(rrpos == string::npos)
            rrpos = buf.find("\r\n\r\n"), nrskip = 4;
        if(rrpos != string::npos) {
            req_header_str += buf.substr(0, rrpos);
            req_body_str = buf.substr(rrpos + nrskip);
            break;
        }
        req_header_str += buf;
    }
    map<string,string> req_header;
    if(!parse_http_request_header(req_header, req_header_str))
        return -1;
    
    if(req_header["!method"] == "POST")
    {
        int len_rem = lexical_cast<int>(getm(req_header, "Content-Length", "0"));
        len_rem -= req_body_str.length();
        while(len_rem > 0)
        {
            if(!recv_string(sock, buf, len_rem))
                return -1;
            len_rem -= buf.length();
            req_body_str += buf;
        }
    }
    
    //cout << "Header::" << endl << req_header << endl;
    //cout << "Body::" << endl << req_body_str << endl;
    //cout << endl;
    cout << "Request " << request_counter << ": " << req_header["!method"] << " " << req_header["!path"] << endl;

    string resp = "";
    vector<string> path;
	algorithm::split(path, req_header["!path"], is_any_of("/"));
    path.erase(path.begin());
    
    if(path.size() == 0 || path[0] == "" || path[0] == "index.html")
    {
        // cout << "Handle top index" << endl;
        resp = build_resp_file("./files/index.html");
    }
    else if(path.size() > 1 && path[0] == "files")
    {
        // cout << "Handle /files/" << endl;
        path.erase(path.begin());
        string target_file_path = algorithm::join(path, "/");
        if(target_file_path.find("..") != string::npos)
            resp = build_resp_404();
        else
            resp = build_resp_file("./files/" + target_file_path);
    }
    else if(path.size() > 0 && path[0] == "test_time")
    {
        static int counter = 0;
        stringstream strm;
        {
            JsonDict list(strm);
            list.add("va", "abasb");
            list.add("counter", counter++);
            list.add("vc", false);
        }
        cout << "Wrote:" << strm.str() << endl;
        resp = build_resp_json(strm);
    }
    else if(path.size() > 0 && path[0] == "ds")
    {
        path.erase(path.begin());
        stringstream strm;
        map<string, string> data;
        parse_query_string(data, req_body_str);
        string mime;
        if(!DataStoreRequest(strm, mime, path, data))
            resp = build_resp_404();
        else
            resp = build_resp_200(mime, strm.str());
    }
    else
    {
        resp = build_resp_404();
    }
    send_string(sock, resp);
    close(sock);
    //cout << "Responced." << endl;
    return 0;
}

int g_thread_counter = 0;

void *start_client_handler(void *sock_p)
{
    int sock = reinterpret_cast<unsigned long long int>(sock_p);
    g_thread_counter++;
    client_handler(sock);
    g_thread_counter--;
    return NULL;
}

void* http_server_main(void *p)
{
    int server_sock;
	struct sockaddr_in svr_addr;
	

    memset(&svr_addr, 0, sizeof(svr_addr));
	svr_addr.sin_port = htons(8020);
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	int yes_val = 1;
	setsockopt(
		server_sock,
		SOL_SOCKET,	
		SO_REUSEADDR,
		(const char *)&yes_val,
		sizeof(yes_val)
	);
	bind(server_sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr));
	listen(server_sock, 1);

 	while(1)
    {
        // cout << "Waiting accept" << endl;
        struct sockaddr_in client_addr;
        int sz_client_addr = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *) &client_addr, (socklen_t *)&sz_client_addr);
		// cout << "Accept from " << inet_ntoa(client_addr.sin_addr) << endl;

        pthread_t client_handler_thread;
        pthread_create(&client_handler_thread, NULL, start_client_handler, (void *)client_sock);
        pthread_detach(client_handler_thread);
        
        cout << "# of threads: " << g_thread_counter << endl;
	}
    return NULL;
}

pthread_t g_http_server_thread;
void start_http_server()
{
    pthread_create(&g_http_server_thread, NULL, http_server_main, NULL);
    pthread_detach(g_http_server_thread);
}
