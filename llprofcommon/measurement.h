#ifndef LLPROF_MEASUREMENTS_H
#define LLPROF_MEASUREMENTS_H

#include "call_tree.h"


    
time_val_t get_time_now_nsec();

void start_timer_thread();
time_val_t get_timer_thread_counter();



#endif // LLPROF_MEASUREMENTS_H
