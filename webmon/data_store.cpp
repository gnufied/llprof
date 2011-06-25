
#include "http.h"
#include "data_store.h"
#include "../llprofcommon/network.h"
#include "../llprofcommon/llprof_const.h"
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace std;

DataStore::DataStore(int id)
{
    id_ = id;
    socket_ = 0;
    closed_ = false;
}

DataStore::~DataStore()
{
    Close();
}

void DataStore::SetConnectedSocket(int sock)
{
    socket_ = sock;
}

void DataStore::SetConnectTo(string host, string port)
{
    host_ = host;
    port_ = port;
}

void* DataStore_thread_main(void *data)
{
    ((DataStore*)data)->ThreadMain();
}

void DataStore::StartThread()
{
    pthread_create(&conn_thread_, NULL, DataStore_thread_main, this);
    pthread_detach(conn_thread_);
}

void DataStore::Close()
{
    if(!closed_)
    {
        closed_ = true;
        if(socket_)
            close(socket_);
    }
}


MessageBuf *DataStore::SendRequest(unsigned int msg_type, void *msg, int msg_size)
{
    SendProfMessage(socket_, msg_type, msg, msg_size);
    MessageBuf *buf = new MessageBuf();
    buf->Resize(8);
    buf->ReadFrom(socket_, 0, 8);
    buf->FitSize();
    buf->ReadFrom(socket_, 8, buf->GetMessageSize());
    return buf;
}

int MessageBuf::ReadFrom(int sock, int offset, int sz)
{
    return RecvFull(sock, (buf_ + offset), sz);
}

void DataStore::ThreadMain()
{
    if(socket_ == 0)
    {
        socket_ = SocketConnectTo(host_, port_);
        if(socket_ <= 0)
        {
            socket_ = 0;
            closed_ = true;
            cout << "Failed to connect:" << host_ << ":" << port_ << endl;
            return;
        }
    }

    
    int request_val;
    MessageBuf *res;
    
    request_val = INFO_DATA_SLIDE;
    res = SendRequest(MSG_QUERY_INFO, &request_val, 4);
    cout << "Response SLIDE: (" << res->GetMessageSize() << ")" << res->AsString() << endl;
    delete res;

    // プロファイルターゲット名の取得
    request_val = INFO_PROFILE_TARGET;
    res = SendRequest(MSG_QUERY_INFO, &request_val, 4);
    cout << "Response TARGET: " << res->AsString() << endl;
    target_name_ = res->AsString();
    delete res;

    // レコード情報
    request_val = INFO_RECORD_METAINFO;
    res = SendRequest(MSG_QUERY_INFO, &request_val, 4);
    cout << "Response RECORD_INFO: " << res->AsString() << endl;
    delete res;


}

map<int, DataStore*> *gAllDataStore;
int gDataStoreIDSeq;
void InitDataStore()
{
    gAllDataStore = new map<int, DataStore*>();
    gDataStoreIDSeq = 0;
}

bool DataStoreRequest(std::stringstream &out, const std::vector<std::string> &path,
        const std::map<std::string,std::string> &data)
{
    if(path.size() > 0 && path[0] == "new")
    {
        vector<string> connect_to;
        string hostval = getm(data, "host", "");
        algorithm::split(connect_to, hostval, is_any_of(":"));
        if(connect_to.size() == 1)
            connect_to.push_back("12321");
        
        gDataStoreIDSeq++;
        DataStore *ds = new DataStore(gDataStoreIDSeq);



        (*gAllDataStore)[gDataStoreIDSeq] = ds;
        ds->SetConnectTo(connect_to[0], connect_to[1]);
        ds->StartThread();
        write_json(out, "OK");
        return true;
    }
    else if(path.size() > 0 && path[0] == "list")
    {
        JsonList lw(out);
        for(map<int,DataStore*>::iterator iter = gAllDataStore->begin(); iter != gAllDataStore->end(); iter++)
        {
            stringstream substrm;
            JsonDict d(substrm);
            d.add("id", (*iter).second->GetID());
            d.add("name", (*iter).second->GetTargetName());
            d.add("host", (*iter).second->GetTargetHost());
            d.close();
            lw.add<stringstream&>(substrm);
        }
        return true;
    }

    return false;
}


