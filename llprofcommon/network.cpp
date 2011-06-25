
#include "network.h"
#include <assert.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace std;

int SendFull(int sock, const char *pbuf, int sz)
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

int RecvFull(int sock, char *pbuf, int sz)
{
    int offset = 0;
    while(1)
    {
        int read_sz = recv(sock, pbuf + offset, sz - offset, 0);
        if(read_sz <= 0)
        {
            if(read_sz != 0)
            {
                //perror("read");
            }
            return 1;
        }
        offset += read_sz;
        if(offset == sz)
            return 0;
        assert(offset < sz);
    }
}


int SocketConnectTo(string host, string service)
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



bool SendProfMessage(int sock, int msg_id, const void *buf, int buf_sz)
{
    // printf("Send: type:%d  size:%d\n", msg_id, buf_sz);
    unsigned int msg[] = {msg_id, buf_sz};
    if(SendFull(sock, (const char *)msg, 8) != 0)
        return false;
    if(SendFull(sock, (const char *)buf, buf_sz) != 0)
        return false;
    return true;
}




void HexDump(char *buf, int size)
{
    int i;
    for(i = 0; i < size; i++)
    {
        if(i % 32 == 0)
        {
            if(i != 0)
                printf("\n");
            printf("%.8X: ", i);
        }
        printf("%.2X ", ((unsigned char *)buf)[i]);
    }
    printf("\n");
}


