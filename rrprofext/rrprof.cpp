
#include <ruby/ruby.h>
// #include <vm_core.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "platforms.h"
#include "rrprof.h"
#include "call_tree.h"
#include "server.h"
#include <iostream>
#include <sstream>
using namespace std;


#ifndef _WIN32
time_val_t GetNowTime_clock_gettime_monotonic()
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
    
	return ((time_val_t)tp.tv_sec)*1000*1000*1000 + ((time_val_t)tp.tv_nsec);
}
#else
time_val_t GetNowTime_clock_gettime_monotonic()
{
    FILETIME ft;
    ULARGE_INTEGER u64Time;
    GetSystemTimeAsFileTime(&ft);
    u64Time.u.LowPart = ft.dwLowDateTime;
    u64Time.u.HighPart = ft.dwHighDateTime;
    return u64Time.QuadPart * 100;
}

#endif

#ifdef TIMERMODE_CLOCK_TIMERTHREAD
time_val_t gTimerCounter = 0;
#endif

void rrprof_exit (void);



#ifdef TIMERMODE_TIMERTHREAD
pthread_t gTimerThread;
volatile time_val_t gTimerCounter;
void timer_thread_main()
{
    for(;;)
    {
        ++gTimerCounter;
        usleep(TIMERTHREAD_INTERVAL);
    }    
}
void start_timer_thread()
{
    pthread_create(&gTimerThread, NULL, timer_thread_main, NULL);
    pthread_detach(gTimerThread); 
}
#endif


struct ruby_name_info_t{
    VALUE klass;
    ID id;
};

const char *rrprof_name_func(void *name_info_ptr)
{
    if(!name_info_ptr)
        return "(null)";

    stringstream s;
    s << rb_class2name(((ruby_name_info_t*)name_info_ptr)->klass);
    s << "::";
    s << rb_id2name(((ruby_name_info_t*)name_info_ptr)->id);
    return s.str().c_str();
}

inline nameid_t nameinfo_to_nameid(const ruby_name_info_t &nameinfo)
{
    return (unsigned long long)nameinfo.klass * (unsigned long long)nameinfo.id;
}

void rrprof_calltree_call_hook(rb_event_flag_t event, VALUE data, VALUE self, ID p_id, VALUE p_klass)
{
    ruby_name_info_t ruby_name_info;
    if(event == RUBY_EVENT_C_CALL)
    {
        ruby_name_info.id = p_id;
        ruby_name_info.klass = p_klass;
    }
    else
        rb_frame_method_id_and_class(&ruby_name_info.id, &ruby_name_info.klass);
    nameid_t nameid = nameinfo_to_nameid(ruby_name_info);
    llprof_call_handler(nameid, (void *)&ruby_name_info);
}




void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID p_id, VALUE p_klass)
{
    llprof_return_handler();
}







extern "C"
void Init_rrprof(void)
{

    #ifdef PRINT_FLAGS
        print_option_flags();
    #endif

    llprof_set_time_func(GetNowTime_clock_gettime_monotonic);
    llprof_set_name_func(rrprof_name_func);
    llprof_init();
    

    #ifdef TIMERMODE_TIMERTHREAD
        start_timer_thread();
    #endif
    start_server();
	VALUE rrprof_mod = rb_define_module("RRProf");

	rb_add_event_hook(&rrprof_calltree_call_hook, RUBY_EVENT_CALL | RUBY_EVENT_C_CALL, Qnil);
	rb_add_event_hook(&rrprof_calltree_ret_hook, RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN, Qnil);



    atexit (rrprof_exit);

}



void rrprof_exit (void)
{

    char *ep_mode = getenv("RRPROF_ENDPRINT");
    if(ep_mode && !strcmp(ep_mode, "1"))
    {
        printf("[rrprof exit]\n");
    }
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



