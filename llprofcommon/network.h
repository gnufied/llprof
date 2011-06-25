#ifndef LLPROF_NETWORK_H
#define LLPROF_NETWORK_H

#include "platforms.h"
#include <string>

int SendFull(int sock, const char *pbuf, int sz);
int RecvFull(int sock, char *pbuf, int sz);

int SocketConnectTo(std::string host, std::string service);

bool SendProfMessage(int sock, int msg_id, const void *buf, int buf_sz);

void HexDump(char *buf, int size);

#endif // LLPROF_NETWORK_H
