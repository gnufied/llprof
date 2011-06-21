
#include <iostream>
#include "server.h"
#include "llprof_const.h"

#include <assert.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "platforms.h"
#include "call_tree.h"
#include "class_table.h"

using namespace std;

#ifdef _WIN32
#   include <winsock2.h>
#endif



string g_profile_target_name;
int gServerStartedFlag;
pthread_t gServerThread;

int g_aggressive_thread_started = 0;
pthread_t g_aggressive_thread;

int send_full(int sock, const char *pbuf, int sz)
{
    int offset = 0;
    while(1)
    {
        int wrote_sz = send(sock, pbuf + offset, sz - offset, 0);
        if(wrote_sz <= 0)
            return 1;
        offset += wrote_sz;
        if(offset == sz)
            return 0;
        assert(offset < sz);
    }    
}

int recv_full(int sock, char *pbuf, int sz)
{
    int offset = 0;
    while(1)
    {
        int read_sz = recv(sock, pbuf + offset, sz - offset, 0);
        if(read_sz <= 0)
        {
            if(read_sz != 0)
            {
                perror("read");
            }
            return 1;
        }
        offset += read_sz;
        if(offset == sz)
            return 0;
        assert(offset < sz);
    }
}




void SendMessage(int sock, int msg_id, const void *buf, int buf_sz)
{
    SendMessage2(sock, msg_id, buf, buf_sz, NULL, 0);
}

void SendMessage2(int sock, int msg_id, const void *buf1, int buf_sz1, const void *buf2, int buf_sz2)
{
    // printf("Send: type:%d  size:%d\n", msg_id, buf_sz);
    unsigned int msg[] = {msg_id, buf_sz1+buf_sz2};
    send_full(sock, (const char *)msg, 8);
    if(buf_sz1 != 0)
        send_full(sock, (const char *)buf1, buf_sz1);
    if(buf_sz2 != 0)
        send_full(sock, (const char *)buf2, buf_sz2);
}


int StartProfile_GetProfileMode(char *buf)
{
    return *(int *)buf+0;
}

int QueryInfo_GetInfoType(char *buf)
{
    return *(int *)buf+0;
}


extern slide_record_t g_slides[16];

void slide_info_to_str(char *str, int maxsz)
{
    int sz;
    slide_record_t *records = 0;
    CallTree_GetSlide(&records, &sz);
            
    int offset = 0;
    int i;
    for(i = 0; i < sz; i++)
    {
        int wrote_sz = snprintf(str+offset, maxsz-offset-1, "%s %d\n", records[i].field_name, records[i].slide);
        if(wrote_sz < 0)
            return;
        offset += wrote_sz;
    }
}

void msg_handler(int sock, int msg_id, int size, char *msgbuf)
{
    // それぞれのメッセージの意味は llprof_const.h
    switch(msg_id)
    {
    case MSG_START_PROFILE:
        // SetProfileMode(StartProfile_GetProfileMode(msgbuf));
        gServerStartedFlag = 1;
        SendMessage(sock, MSG_COMMAND_SUCCEED, 0, 0);

        break;
    
    case MSG_QUERY_INFO:{
        int info_type = QueryInfo_GetInfoType(msgbuf);
        if(info_type == INFO_DATA_SLIDE)
        {
            char buf[1024];
            slide_info_to_str(buf, sizeof(buf));
            SendMessage(sock, MSG_SLIDE_INFO, buf, strlen(buf));
        }
        else if(info_type == INFO_PROFILE_TARGET)
        {
            string name = llprof_get_target_name();
            SendMessage(sock, MSG_PROFILE_TARGET, name.c_str(), name.length());
        }
        else if(info_type == INFO_RECORD_METAINFO)
        {
            string value = llprof_get_record_info();
            SendMessage(sock, MSG_RECORD_METAINFO, value.c_str(), value.length());
        }
        break;
    }
    case MSG_REQ_PROFILE_DATA:{
        unsigned int sz;
        char *buf;
        // printf("Now Time: %lld\n", GetNowTime());
        
        ThreadIterator iter;
        BufferIteration_Initialize(&iter);
        while(BufferIteration_NextBuffer(&iter))
        {
            BufferIteration_GetBuffer(&iter, (void**)&buf, &sz);
            int msg_type = 0;
            if(BufferIteration_GetBufferType(&iter) == BT_PERFORMANCE)
                msg_type = MSG_PROFILE_DATA;
            else if(BufferIteration_GetBufferType(&iter) == BT_STACK)
                msg_type = MSG_STACK_DATA;
            unsigned long long thread_id = BufferIteration_GetThreadID(&iter);
            SendMessage2(
                sock, msg_type,
                &thread_id , 8,
                buf, sz
            );
        }

        SendMessage(sock, MSG_COMMAND_SUCCEED, 0, 0);
        break;
    }

    case MSG_QUERY_NAMES:{
        int i = 0;
        char name_buf[1000*256];
        int offset = 0;
        int subsec_rem = 0;
        int subsec_type = 0;
        int str_idx = 0;
        
        for(; i < size/8; i++)
        {
            if(subsec_rem == 0)
            {
                subsec_type = *(int *)(msgbuf+(8*i) + 0);
                subsec_rem = *(int *)(msgbuf+(8*i) + 4);
                continue;
            }
            const char *name = 0;
            if(subsec_type == QUERY_NAMES)
                name = GetNameIDTable()->Get(*(nameid_t *)(msgbuf+(8*i)));
                //name = rb_class2name(*(VALUE *)(msgbuf+(8*i)));
//            printf("%d, %d: %lld => %s\n", subsec_type, i, *(unsigned long long *)(msgbuf+(8*i)), name);
            if(name != 0)
            {
                int len_name = strlen(name);
                if(len_name+offset >= sizeof(name_buf))
                {
                    printf("too many names\n");
                    SendMessage(sock, MSG_ERROR, 0, 0);
                    return;
                }
                memcpy(name_buf+offset, name, len_name);
                offset += len_name;
            }
            name_buf[offset] = '\n';
            offset += 1;
            str_idx++;
            subsec_rem--;
        }
        SendMessage(sock, MSG_NAMES, name_buf, offset);
        break;
    }
    }

}


void llprof_message_dispatch(int sock)
{
    while(1)
    {
		int msg[2];
        char msg_buf[65536];
        memset(msg_buf, 255, 65536);
        if(recv_full(sock, (char *)msg, 8))
            break;
        int msg_id = msg[0];
        int msg_sz = msg[1];
        if(msg_sz >= sizeof(msg_buf))
        {
            printf("\nReceive: Message too large\n");
            abort();
        }
        if(msg_sz > 0)
            if(recv_full(sock, msg_buf, msg_sz))
            {
                return;
            }
        msg_handler(sock, msg_id, msg_sz, msg_buf);
    }
}

void *llprof_server_thread(void *p)
{
	cout << "Server Thread Started." << endl;
	int listening_sock;
	struct sockaddr_in listening_addr;
	int ret;

#   ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#   endif

	listening_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listening_sock == -1)
		perror("socket");

	memset(&listening_addr, 0, sizeof(listening_addr));
	listening_addr.sin_family = AF_INET;
	listening_addr.sin_addr.s_addr = INADDR_ANY;
	listening_addr.sin_port = htons(12321);
	

	int yes_val = 1;
	setsockopt(
		listening_sock,
		SOL_SOCKET,	
		SO_REUSEADDR,
		(const char *)&yes_val,
		sizeof(yes_val)
	);

	ret = bind(listening_sock, (struct sockaddr *)&listening_addr, sizeof(listening_addr));
	if(ret == -1)
		perror("bind");
	ret = listen(listening_sock, 10);
	if(ret == -1)
	{
		printf("listen error: %s", strerror(errno));

		perror("listen");
	}

	while(1)
	{
		struct sockaddr_in client_addr;
		int len = sizeof(client_addr);
        #ifdef LLPROF_DEBUG
		printf("listening...\n");
        #endif
		int sock = accept(listening_sock,  (struct sockaddr *)&client_addr, (socklen_t *)&len);
        #ifdef LLPROF_DEBUG
		printf("Connected\n");
        #endif
        
        llprof_message_dispatch(sock);

        #ifdef LLPROF_DEBUG
        printf("Disconnected\n");
        #endif
	}
#   ifdef _WIN32
        WSACleanup();
#   endif
}




int socket_connect_to(string host, string service)
{
	struct addrinfo hints, *res0;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	int err;
	if((err = getaddrinfo(host.c_str(), service.c_str(), &hints, &res0)) != 0)
	{
		return -1;
	}

    int sock = -1;
	for(addrinfo *res = res0; res != NULL; res = res->ai_next)
	{
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(sock < 0)
			continue;
		if(connect(sock, res->ai_addr, res->ai_addrlen) != 0)
		{
			close(sock);
            sock = -1;
			continue;
		}
	}
	// if(sock == -1) not connected

	freeaddrinfo(res0);
    return sock;
}


void *llprof_aggressive_thread(void *p)
{
    cout << "Aggressive Thread: Start" << endl;
#   ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#   endif

    string host, port;
    int interval = 0;
    if(!getenv("LLPROF_AGG_HOST") || strlen(getenv("LLPROF_AGG_HOST")) == 0)
    {
        cout << "Aggressive Thread: Exit (No LLPROF_AGG_HOST)" << endl;
        return NULL;
    }
    host = getenv("LLPROF_AGG_HOST");

    if(getenv("LLPROF_AGG_PORT"))
        port = getenv("LLPROF_AGG_PORT");
    else
        port = "12300";
    
    if(getenv("LLPROF_AGG_INTERVAL"))
    {
        interval = atoi(getenv("LLPROF_AGG_INTERVAL"));
        if(interval < 4)
            interval = 4;
    } else
    {
        interval = 60;
    }
    
    while(true)
    {
        cout << "Aggressive Thread: try" << endl;
        int sock = socket_connect_to(host, port);
        if(sock != -1)
        {
            cout << "Aggressive Thread: connected:" << sock << endl;
            llprof_message_dispatch(sock);
            cout << "Aggressive Thread: diconnected" << endl;
        }
        sleep(interval);
    }

#   ifdef _WIN32
        WSACleanup();
#   endif
}

void llprof_start_server()
{
    pthread_create(&gServerThread, NULL, llprof_server_thread, NULL);
    pthread_detach(gServerThread);

}

void llprof_start_aggressive_thread()
{
    pthread_create(&g_aggressive_thread, NULL, llprof_aggressive_thread, NULL);
    pthread_detach(g_aggressive_thread);
}

void llprof_set_target_name(const string &name)
{
    g_profile_target_name = name;
}

string llprof_get_target_name()
{
    return g_profile_target_name;
}
