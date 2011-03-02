#ifndef RRPROF_H
#define RRPROF_H

#include "rrprof_const.h"

typedef long long int time_val_t;
int IsProgramRunning();

extern int gServerStartedFlag;



#ifdef ENABLE_TIME_GETTING
#   ifdef TIMERMODE_CLOCK_GETTIME_MONOTONIC
        time_val_t GetNowTime_clock_gettime_monotonic();
#       define NOW_TIME    (GetNowTime_clock_gettime_monotonic())
#   endif
#   ifdef TIMERMODE_TIMERTHREAD
        volatile extern time_val_t gTimerCounter;

#       define NOW_TIME     (gTimerCounter)
#   endif
#else
#   define NOW_TIME     (0)
#endif

/*
inline time_val_t GetNowTime()
{
    unsigned long long ret;
    __asm__ volatile ("rdtsc" : "=A" (ret));
    return ret;
}
*/
void rp_assert_failure(char *expr);

#define rp_assert(expr)     if(!(expr)){rp_assert_failure(#expr);};


typedef struct
{
    const char *field_name;
    int slide;
} slide_record_t;



#define ADD_SLIDE(struct_field, field_name_str)     { \
    slides[slide_idx].field_name = field_name_str; \
    slides[slide_idx].slide = (int)((unsigned long long int)&test.struct_field - (unsigned long long int)&test); \
    slide_idx++; \
}

void HexDump(char *buf, int size);


#endif

