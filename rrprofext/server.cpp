
#include <iostream>
#include <ruby/ruby.h>
#include "server.h"
#include "rrprof_const.h"

#include <assert.h>
#include <string.h>
#include "platforms.h"
#include "rrprof.h"
#include "call_tree.h"
#include "class_table.h"

using namespace std;

#ifdef _WIN32
#   include <winsock2.h>
#endif

pthread_t gServerThread;

int send_full(int sock, char *pbuf, int sz)
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




void SendMessage(int sock, int msg_id, void *buf, int buf_sz)
{
    SendMessage2(sock, msg_id, buf, buf_sz, NULL, 0);
}

void SendMessage2(int sock, int msg_id, void *buf1, int buf_sz1, void *buf2, int buf_sz2)
{
    // printf("Send: type:%d  size:%d\n", msg_id, buf_sz);
    unsigned int msg[] = {msg_id, buf_sz1+buf_sz2};
    send_full(sock, (char *)msg, 8);
    if(buf_sz1 != 0)
        send_full(sock, (char *)buf1, buf_sz1);
    if(buf_sz2 != 0)
        send_full(sock, (char *)buf2, buf_sz2);
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
    // それぞれのメッセージの意味は > rrprof_const.h
    switch(msg_id)
    {
    case MSG_START_PROFILE:
        // SetProfileMode(StartProfile_GetProfileMode(msgbuf));
        gServerStartedFlag = 1;
        SendMessage(sock, MSG_COMMAND_SUCCEED, 0, 0);
        while(!IsProgramRunning())
        {
            sleep(1);
        }
        break;
    
    case MSG_QUERY_INFO:{
        int info_type = QueryInfo_GetInfoType(msgbuf);
        if(info_type == INFO_DATA_SLIDE)
        {
            char buf[1024];
            slide_info_to_str(buf, sizeof(buf));
            SendMessage(sock, MSG_SLIDE_INFO, buf, strlen(buf));
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
            if(subsec_type == QUERY_NAMES_SYMBOL)
                name = GetMethodNameTable()->Get(*(NameTable::Key *)(msgbuf+(8*i)));
            else if(subsec_type == QUERY_NAMES_CLASS)
            {
                name = GetClassNameTable()->Get(*(NameTable::Key *)(msgbuf+(8*i)));

                // cout << "   Res:" << name << endl;
            }
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


void *rrprof_server_thread(void *p)
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
		int msg[2];
		struct sockaddr_in client_addr;
		int len = sizeof(client_addr);
        #ifdef PRINT_DEBUG
		printf("listening...\n");
        #endif
		int sock = accept(listening_sock,  (struct sockaddr *)&client_addr, (socklen_t *)&len);
        #ifdef PRINT_DEBUG
		printf("Connected\n");
        #endif
		while(1)
		{
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
                    break;
                }
            msg_handler(sock, msg_id, msg_sz, msg_buf);
		}
        #ifdef PRINT_DEBUG
        printf("Disconnected\n");
        #endif
	}
    
#   ifdef _WIN32
        WSACleanup();
#   endif

}


void start_server()
{
    pthread_create(&gServerThread, NULL, rrprof_server_thread, NULL);
    pthread_detach(gServerThread);
}

