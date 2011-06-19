
#include <ruby/ruby.h>
// #include <vm_core.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "llprof.h"
#include <iostream>
#include <sstream>
using namespace std;


void rrprof_exit (void);


struct ruby_name_info_t{
    VALUE klass;
    ID id;
};

const char *rrprof_name_func(nameid_t nameid, void *name_info_ptr)
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


    llprof_set_time_func(get_time_now_nsec);
    llprof_set_name_func(rrprof_name_func);
    llprof_init();
    

    
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



