
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

void rp_assert_failure(char *expr)
{
    printf("rp_assert_failure: %s\n", expr);
    abort();
}
/*
time_val_t gTimeCounter;
time_val_t GetNowTime()
{
	return 0;
}
*/
#ifndef _WIN32
time_val_t GetNowTime_clock_gettime_monotonic()
{
	struct timespec tp;
	//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	clock_gettime(CLOCK_MONOTONIC, &tp);
    
    // return (time_val_t)tp.tv_nsec;
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
/*
time_val_t GetNowTime()
{
    unsigned long long ret;
    __asm__ volatile ("rdtsc" : "=A" (ret));
    return ret;
}

time_val_t GetNowTime()
{
     unsigned a, d;
     asm("cpuid");
     asm volatile("rdtsc" : "=a" (a), "=d" (d));

     return (((time_val_t)a) | (((time_val_t)d) << 32));
}
*/

#ifdef TIMERMODE_CLOCK_TIMERTHREAD
time_val_t gTimerCounter = 0;
#endif

/*static VALUE
print_stat(VALUE self, VALUE obj)
{
    CallTree_PrintStat(self, obj);
    return NULL;
}
*/

void
print_test(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
	ID id;
	VALUE klass;
    rb_frame_method_id_and_class( &id, &klass);
    const char *name = rb_id2name(id);
    int line = rb_sourceline();
    printf("print_test: Current method: %s [%d]\n", name, line);
}


void rrprof_exit (void);


void print_option_flags()
{
    printf("Option builtin Flags: ");
    #ifdef ENABLE_TIME_GETTING
        printf("ENABLE_TIME_GETTING ");
    #endif
    #ifdef ENABLE_RUBY_RUNTIME_INFO  
        printf("ENABLE_RUBY_RUNTIME_INFO ");
    #endif
    #ifdef ENABLE_SEARCH_NODES       
        printf("ENABLE_SEARCH_NODES ");
    #endif
    #ifdef ENABLE_CALC_TIMES         
        printf("ENABLE_CALC_TIMES ");
    #endif
    #ifdef ENABLE_STACK
        printf("ENABLE_STACK ");
    #endif
    
    #ifdef DO_NOTHING_IN_HOOK
        printf("DO_NOTHING_IN_HOOK ");
    #endif
    printf(".\n");
}


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

inline nameid_t RubyNameInfoToNameID(VALUE klass, ID id)
{
    return (unsigned long long)klass * (unsigned long long)id;
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
    {
        rb_frame_method_id_and_class(&ruby_name_info.id, &ruby_name_info.klass);
    }
    
    nameid_t nameid = RubyNameInfoToNameID(ruby_name_info.klass, ruby_name_info.id);

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
    // rb_define_module_function(rrprof_mod, "print_stat", (VALUE (*)(...))print_stat, 0);

    
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
        //print_stat(Qnil, Qnil);
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



/*
VALUE CallTree_PrintStat(VALUE self, VALUE obj)
{
	
	printf("[All threads]\n");
    ThreadIterator iter;
    BufferIteration_Initialize(&iter);
    unsigned long long int node_counter = 0;
    while(BufferIteration_NextBuffer(&iter))
    {
        if(iter.phase != 1)
            continue;
        printf("Thread %lld:\n", iter.current_thread->ThreadID);
        printf("  Call Nodes: %d\n", (int)iter.current_thread->NodeInfoArray.size());
        node_counter += iter.current_thread->NodeInfoArray.size();
        
        
        unsigned long long int tbl_sz = 0;
        int i;
        for(i = 1; i < iter.current_thread->NodeInfoArray.size(); i++)
        {
            tbl_sz += 0; //st_memsize(iter.current_thread->NodeInfoArray[i].children->tbl);
        }
        printf("  Table Size: %lld\n", tbl_sz);
    }

    
	printf("[St]\n");
    printf("  All Call Nodes: %lld\n", node_counter);
    
    //print_table();
    return 0;
}

*/




