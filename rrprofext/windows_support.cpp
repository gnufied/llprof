#ifdef _WIN32

#include "windows_support.h"


struct no_return_func_param_t{
    void * (*start_routine)(void *);
    void *arg;
};

void __cdecl no_return_func(void *p)
{
    no_return_func_param_t *start_param = (no_return_func_param_t *)p;
    (*start_param->start_routine)(start_param->arg);
    delete start_param;
    return;
}

int pthread_create(pthread_t *thread, pthread_attr_t *attr, void * (*start_routine)(void *), void *arg)
{
	cout << "pthread_create." << endl;
    no_return_func_param_t *param = new no_return_func_param_t();
    param->start_routine = start_routine;
    param->arg = arg;
    _beginthread(no_return_func, 0, param);
    return 0;
}

int pthread_detach(pthread_t th)
{
    return 0;
}
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    InitializeCriticalSection(&mutex->cs_);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    DeleteCriticalSection(&mutex->cs_);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    EnterCriticalSection(&mutex->cs_);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    LeaveCriticalSection(&mutex->cs_);
    return 0;
}

int pthread_key_create(pthread_key_t *key, void (*destr_function) (void*))
{
    key->tls_index_ = TlsAlloc();
    return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
    return TlsGetValue(key.tls_index_);
}

int pthread_setspecific(pthread_key_t key, void *pointer)
{
    TlsSetValue(key.tls_index_, (void *)pointer);
    return 0;
}


unsigned int sleep(unsigned int seconds)
{
    Sleep(seconds*1000);
    return 0;
}


#endif
