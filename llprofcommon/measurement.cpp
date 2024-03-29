
#include "measurement.h"

#include "platforms.h"


#ifndef _WIN32
profile_value_t get_time_now_nsec()
{
	struct timespec tp;
	//clock_gettime(CLOCK_MONOTONIC, &tp);
	clock_gettime(CLOCK_REALTIME, &tp);
    
	return ((profile_value_t)tp.tv_sec)*1000*1000*1000 + ((profile_value_t)tp.tv_nsec);
}
#else
profile_value_t get_time_now_nsec()
{
    FILETIME ft;
    ULARGE_INTEGER u64Time;
    GetSystemTimeAsFileTime(&ft);
    u64Time.u.LowPart = ft.dwLowDateTime;
    u64Time.u.HighPart = ft.dwHighDateTime;
    return u64Time.QuadPart * 100;
}

#endif

volatile profile_value_t gTimerCounter = 0;
profile_value_t gTimerCounterInterval = 1000;
pthread_t gTimerThread;

static void* timer_thread_main(void *p)
{
    for(;;)
    {
        ++gTimerCounter;
        usleep(gTimerCounterInterval);
    }    
}
void start_timer_thread()
{
    pthread_create(&gTimerThread, NULL, timer_thread_main, NULL);
    pthread_detach(gTimerThread);
}

profile_value_t get_timer_thread_counter()
{
    return gTimerCounter;
}
