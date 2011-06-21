#ifndef LLPROF_MEASUREMENTS_H
#define LLPROF_MEASUREMENTS_H

#include <string>
#include <vector>
#include "call_tree.h"

typedef unsigned long long profile_value_t;

time_val_t get_time_now_nsec();

void start_timer_thread();
time_val_t get_timer_thread_counter();


class rtype_metainfo_t
{
public:
    std::vector<std::string> records;

    rtype_metainfo_t(int maxnum)
    {
        records.resize(maxnum);
    }

    void add(int n, const std::string &name, const std::string &unit, const std::string &flag)
    {
        records[n] = name + " " + unit + " f" + flag;
    }
    
    
};


#endif // LLPROF_MEASUREMENTS_H
