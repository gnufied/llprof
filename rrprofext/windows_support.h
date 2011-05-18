/* windows_support.h - WindowsとUNIXの差分を埋めるためのサポート
 */

#ifndef RRPROF_WINDOWS_SUPPORT
#define RRPROF_WINDOWS_SUPPORT
#ifdef _WIN32

#include <Windows.h>

typedef HANDLE pthread_t;
struct pthread_attr_t{
    int dummy_;
};
typedef CRITICAL_SECTION pthread_mutex_t;


int pthread_create(pthread_t *thread, pthread_attr_t *attr, void * (*start_routine)(void *), void *arg);
int pthread_detach(pthread_t th);



#endif
#endif
