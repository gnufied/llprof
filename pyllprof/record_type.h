#ifndef RECORD_TYPE_H
#define RECORD_TYPE_H


const int NUM_RECORDS = 2;


inline void llprof_rtype_init(profile_value_t *value)
{
    value[0] = 0;
    value[1] = 1;
}

inline void llprof_rtype_metainfo(rtype_metainfo_t *metainfo)
{
    metainfo->add(0, "time", "ns", "A");
    metainfo->add(1, "start_timw", "ns", "S");
}

inline void llprof_rtype_start_node(profile_value_t *value)
{
    value[1] = get_time_now_nsec();
}

inline void llprof_rtype_end_node(const profile_value_t *start_value, profile_value_t *value)
{
    value[0] += get_time_now_nsec() - start_value[1];
}

inline void llprof_rtype_stackinfo_nowval(profile_value_t *value)
{
    llprof_rtype_start_node(value);
}


#endif
