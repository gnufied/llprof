#ifndef PLATFORMS_H
#define PLATFORMS_H



#include "windows_support.h"

#include <sys/types.h>

#ifndef _WIN32
#   include <unistd.h>
#   include <pthread.h>
#   include <sys/socket.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#else
#endif




#endif
