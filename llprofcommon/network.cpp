
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
    if(buf_sz != 0)
    {
        if(SendFull(sock, (const char *)buf, buf_sz) != 0)
            return false;
    }
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

void InitSocketSubSystem()
{
#   ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#   endif

}

void FinSocketSubSystem()
{
#   ifdef _WIN32
        WSACleanup();
#   endif
}


int MakeServerSocket(int port)
{
	int listening_sock;
	struct sockaddr_in listening_addr;
	int ret;

	listening_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listening_sock == -1)
		perror("socket");

	memset(&listening_addr, 0, sizeof(listening_addr));
	listening_addr.sin_family = AF_INET;
	listening_addr.sin_addr.s_addr = INADDR_ANY;
	listening_addr.sin_port = htons(port);

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
    return listening_sock;
}



unsigned long long int HashStr(const string &s)
{
    return HashStr(s.c_str());
}

unsigned long long int HashStr(const char *s)
{
    unsigned long long int result = 0;
    int len = strlen(s);
    int p = 0;
    result += len;
    while(p < len)
    {
        if(len - p >= sizeof(unsigned long long int))
        {
            result += *(unsigned long long int *)(s+p) << ((p/7)%7);
            p += 7;
        }else
        {
            result += (int)*(char *)(s+p) << ((p*8) % 7);
            p += 1;
        }
    }
    return result;
}


