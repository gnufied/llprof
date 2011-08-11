/* call_tree.h - コールツリーのデータ構造、管理
 * 
 */

#ifndef CALL_TREE_H
#define CALL_TREE_H

#include "llprof_const.h"
#include "measurement.h"
#include <string>




typedef struct
{
    const char *field_name;
    int slide;
} slide_record_t;


void CallTree_GetSlide(slide_record_t **ret, int *nfield);

struct ThreadIterator{
    struct ThreadInfo* current_thread;
    int phase;
    int got_flag;
};


void BufferIteration_GetBuffer(ThreadIterator *iter, void **buf, unsigned int *size);
unsigned long long BufferIteration_GetThreadID(ThreadIterator *iter);
int BufferIteration_GetBufferType(ThreadIterator *iter);
void BufferIteration_Initialize(ThreadIterator *iter);
int BufferIteration_NextBuffer(ThreadIterator *iter);


typedef unsigned long long nameid_t;

// Public API
void llprof_set_name_func(const char * cb(nameid_t, void *data_ptr));
void llprof_set_time_func(profile_value_t (*cb)());




const char *llprof_call_name_func(nameid_t nameid, void *p);

void llprof_call_handler(nameid_t id, void *name_info);
void llprof_return_handler();

void llprof_calltree_init();

profile_value_t* llprof_get_profile_value_ptr();

std::string llprof_get_record_info();


//

#endif
