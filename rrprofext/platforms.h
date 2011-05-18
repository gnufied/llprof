/* platforms.h - プラットホームごとの差異を吸収するためのヘッダ
 * 
 */
#ifndef PLATFORMS_H
#define PLATFORMS_H



#include "windows_support.h"


#ifndef _WIN32
#   include <sys/types.h>
#   include <unistd.h>
#   include <pthread.h>
#   include <sys/socket.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   define TLS_DECLARE  __declspec( thread )
#else
#   define TLS_DECLARE  __thread
#endif





#endif
