/* windows_support.h - WindowsとUNIXの差分を埋めるためのサポート
 */

#ifndef RRPROF_WINDOWS_SUPPORT
#define RRPROF_WINDOWS_SUPPORT
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>

typedef HANDLE pthread_t;
struct pthread_attr_t{
    int dummy_;
};
struct pthread_mutex_t{
    CRITICAL_SECTION cs_;
};
struct pthread_mutexattr_t{
    int dummy_;
};

struct pthread_key_t{
    DWORD tls_index_;
};

int pthread_create(pthread_t *thread, pthread_attr_t *attr, void * (*start_routine)(void *), void *arg);
int pthread_detach(pthread_t th);


int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_key_create(pthread_key_t *key, void (*destr_function) (void*));
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, void *pointer);


unsigned int sleep(unsigned int seconds);

#endif
#endif
