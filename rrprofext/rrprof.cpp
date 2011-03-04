
#include <ruby/ruby.h>
// #include <vm_core.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "rrprof.h"
#include "call_tree.h"
#include "server.h"



#define POLLFD_MAX	256


int gProgramStartedFlag;
int gServerStartedFlag;

int IsProgramRunning()
{
    return gProgramStartedFlag;
}

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
time_val_t GetNowTime_clock_gettime_monotonic()
{
	struct timespec tp;
	//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	clock_gettime(CLOCK_MONOTONIC, &tp);
    
    // return (time_val_t)tp.tv_nsec;
	return ((time_val_t)tp.tv_sec)*1000*1000*1000 + ((time_val_t)tp.tv_nsec);
}
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

static VALUE
print_stat(VALUE self, VALUE obj)
{
    CallTree_PrintStat(self, obj);
    return NULL;
}


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

extern "C"
void Init_rrprof(void)
{
    gProgramStartedFlag = 0;

    #ifdef PRINT_FLAGS
        print_option_flags();
    #endif

    #ifdef TIMERMODE_TIMERTHREAD
    start_timer_thread();
    #endif
    start_server();
    char *susp_mode = getenv("RRPROF_SUSPEND");
    if(susp_mode && !strcmp(susp_mode, "1"))
    {
        #ifdef PRINT_DEBUG
        printf("Suspend\n");
        #endif
        for(;;)
        {
            sleep(1);
            if(gServerStartedFlag != 0)
            {
                sleep(1);
                break;
            }
        }
    }
    #ifdef PRINT_DEBUG
	printf("Initializing rrprof...\n");
    #endif

	VALUE rrprof_mod = rb_define_module("RRProf");
    rb_define_module_function(rrprof_mod, "print_stat", (VALUE (*)(...))print_stat, 0);


    CallTree_Init();
    CallTree_RegisterModeFunction();
    #ifdef PRINT_DEBUG
	printf("Start program\n");
    #endif

    atexit (rrprof_exit);
    gProgramStartedFlag = 1;

}



void rrprof_exit (void)
{

    char *ep_mode = getenv("RRPROF_ENDPRINT");
    if(ep_mode && !strcmp(ep_mode, "1"))
    {
        printf("[rrprof exit]\n");
        print_stat(Qnil, Qnil);
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





