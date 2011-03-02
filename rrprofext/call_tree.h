#ifndef CALL_TREE_H
#define CALL_TREE_H

#include <ruby/ruby.h>

void CallTree_Init();
void CallTree_RegisterModeFunction();
VALUE CallTree_PrintStat(VALUE self, VALUE obj);
void CallTree_GetSlide(slide_record_t **ret, int *nfield);

typedef struct {
    struct _tag_thread_info* current_thread;
    int phase;
    int got_flag;
} thread_iterator;


void BufferIteration_GetBuffer(thread_iterator *iter, void **buf, unsigned int *size);
unsigned long long BufferIteration_GetThreadID(thread_iterator *iter);
int BufferIteration_GetBufferType(thread_iterator *iter);
void BufferIteration_Initialize(thread_iterator *iter);
int BufferIteration_NextBuffer(thread_iterator *iter);


#endif
