#ifndef RRPROF_H
#define RRPROF_H

#include "llprof_const.h"
#include "call_tree.h"

int IsProgramRunning();

extern int gServerStartedFlag;




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



void HexDump(char *buf, int size);


#endif

