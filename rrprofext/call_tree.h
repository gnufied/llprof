/* call_tree.h - コールツリーのデータ構造、管理
 * 
 */

#ifndef CALL_TREE_H
#define CALL_TREE_H

#include <ruby/ruby.h>

// モジュールの初期化時
void CallTree_InitModule();
// コールツリーデータの初期化
void CallTree_Init();

void CallTree_RegisterModeFunction();
VALUE CallTree_PrintStat(VALUE self, VALUE obj);
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


#endif
