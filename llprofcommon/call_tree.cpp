

#include <assert.h>
#include <time.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>

#include "platforms.h"
#include "server.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "call_tree.h"
#include "class_table.h"
#include "record_type.h"

using namespace std;

// #define LLPROF_DEBUG

//int gDefaultNodeInfoTableSize = 65536;
//int gDefaultSerializedNodeInfoTableSize = 65536;
int gDefaultNodeInfoTableSize = 1024;
int gDefaultSerializedNodeInfoTableSize = 1024;
int gDefaultSerializedStackInfoSize = 512;



static const char * (*g_name_func)(nameid_t, void *);
void llprof_set_name_func(const char * cb(nameid_t, void *))
{
    g_name_func = cb;
}

#define NOW_TIME     ((*g_gettime_func)())

const char *llprof_call_name_func(nameid_t id, void *p)
{
    return (*g_name_func)(id, p);
}


static time_val_t (*g_gettime_func)();
void llprof_set_time_func(time_val_t (*cb)())
{
    g_gettime_func = cb;
}


typedef map<nameid_t, unsigned int> children_t;
struct MethodNodeInfo
{
    unsigned int serialized_node_index;
    unsigned int generation_number;
    
    nameid_t nameid;

	children_t *children;
    time_val_t start_time;
	unsigned int parent_node_id;
};

struct MethodNodeSerializedInfo
{
    nameid_t nameid;
    unsigned int call_node_id;
	unsigned int parent_node_id;
	time_val_t all_time;
	unsigned long long int call_count;

};

struct StackInfo
{
    int node_id;
    time_val_t start_time;
};

#define ADD_SLIDE(struct_field, field_name_str)     { \
    slides[slide_idx].field_name = field_name_str; \
    slides[slide_idx].slide = (int)((unsigned long long int)&test.struct_field - (unsigned long long int)&test); \
    slide_idx++; \
}

void CallTree_GetSlide(slide_record_t **ret, int *nfield)
{
    static slide_record_t slides[16];
    int slide_idx = 0;
    {
        MethodNodeSerializedInfo test;
        
        ADD_SLIDE(nameid, "pdata.nameid")
        ADD_SLIDE(all_time, "pdata.all_time")
        ADD_SLIDE(call_count, "pdata.call_count")
        ADD_SLIDE(call_node_id, "pdata.call_node_id")
        ADD_SLIDE(parent_node_id, "pdata.parent_node_id")
        slides[slide_idx].field_name = "pdata.record_size";
        slides[slide_idx].slide = sizeof(test);
        slide_idx++;
    }
    {
        StackInfo test;
        
        ADD_SLIDE(node_id, "sdata.node_id")
        ADD_SLIDE(start_time, "sdata.start_time")
        slides[slide_idx].field_name = "sdata.record_size";
        slides[slide_idx].slide = sizeof(test);
        slide_idx++;
    }

    *nfield = slide_idx;
    *ret = slides;
}



struct ThreadInfo
{
    unsigned long long int ThreadID;

    vector<MethodNodeInfo> NodeInfoArray;


    vector<MethodNodeSerializedInfo>* SerializedNodeInfoArray;

    vector<StackInfo> *SerializedStackArray;

    unsigned int CurrentCallNodeID;
    unsigned int GenerationNumber;

    pthread_mutex_t DataMutex;
    pthread_mutex_t StackMutex;
    pthread_mutex_t GlobalMutex;

    int stop;
    
    ThreadInfo *next;


    ThreadInfo(unsigned long long thread_id)
    {
        // memset(info, 0, sizeof(ThreadInfo));
        ThreadID = thread_id;
        pthread_mutex_init(&GlobalMutex, NULL);
        pthread_mutex_init(&DataMutex, NULL);
        pthread_mutex_init(&StackMutex, NULL);
        stop = 0;
        
        SerializedNodeInfoArray = new vector<MethodNodeSerializedInfo>();
        SerializedStackArray = new vector<StackInfo>();

#ifdef LLPROF_DEBUG
        cout << "[NewThread]"  << endl << "  pNodes = " << SerializedNodeInfoArray << endl;
        cout << "  pStack = " << SerializedStackArray << endl;
#endif

        NodeInfoArray.reserve(gDefaultNodeInfoTableSize);
        SerializedNodeInfoArray->reserve(gDefaultSerializedNodeInfoTableSize);
        
        // info->SerializedNodeInfoTable = malloc(sizeof(MethodNodeSerializedInfo) * gDefaultSerializedNodeInfoTableSize);
        // info->SerializedNodeInfoTable_Size = gDefaultSerializedNodeInfoTableSize;

        SerializedStackArray->reserve(gDefaultSerializedStackInfoSize);
        SerializedStackArray->resize(1);
        
        memset(&(*SerializedStackArray)[0], 0, sizeof(StackInfo) * SerializedStackArray->size());

        NodeInfoArray.resize(1);
        // CallNodeSeq = 1;
        // ActualCallNodeSeq = 0;
        GenerationNumber = 1;
        CurrentCallNodeID = AddCallNode(0, 0, NULL);

        next = NULL;

    }

    unsigned int AddCallNode(unsigned int parent_node_id, nameid_t nameid, void *name_data_ptr);


};




ThreadInfo *gFirstThread;
ThreadInfo *gLastThread;

pthread_key_t gCurrentThreadKey;

pthread_mutex_t gThreadDataMutex;
unsigned long long int gThreadMaxID = 0;

ThreadInfo *get_current_thread()
{
    ThreadInfo *tdata = (ThreadInfo *)pthread_getspecific(gCurrentThreadKey);
    if(tdata)
        return tdata;

    pthread_mutex_lock(&gThreadDataMutex);
    tdata = new ThreadInfo(gThreadMaxID);
    pthread_setspecific(gCurrentThreadKey, tdata);
    
    gThreadMaxID++;
    if(!gLastThread)
    {
        gFirstThread = gLastThread = tdata;
    }
    else
    {
        gLastThread->next = tdata;
        gLastThread = tdata;
    }
    pthread_mutex_unlock(&gThreadDataMutex);
    assert(tdata);
    return tdata;
}
#define CURRENT_THREAD      ((get_current_thread()))


// 送信用バッファ
vector<MethodNodeSerializedInfo>* gBackBuffer_SerializedNodeInfoArray;
vector<StackInfo>* gBackBuffer_SerializedStackArray;




static MethodNodeSerializedInfo *GetSerializedNodeInfo(unsigned int call_node_id);
static MethodNodeInfo *GetNodeInfo(unsigned int call_node_id);



template <class T>
T *new_tail_elem(vector<T> &arr)
{
    arr.resize(arr.size() + 1);
    return &arr[arr.size() - 1];
}


inline unsigned int GetChildNodeID(ThreadInfo *ti, nameid_t nameid, void *name_data_ptr)
{
    unsigned int parent_id = ti->CurrentCallNodeID;
	MethodNodeInfo *node_info = &ti->NodeInfoArray[parent_id];
    children_t::iterator iter = node_info->children->find(nameid);
	if(iter == node_info->children->end())
		return ti->AddCallNode(parent_id, nameid, name_data_ptr);
	return (*iter).second;
}


void AddActualRecord(unsigned int cnid)
{
    ThreadInfo *ti = CURRENT_THREAD;

    MethodNodeSerializedInfo *sinfo = new_tail_elem(*ti->SerializedNodeInfoArray);

    MethodNodeInfo *call_node = &ti->NodeInfoArray[cnid];
    call_node->serialized_node_index = ti->SerializedNodeInfoArray->size() - 1;
    call_node->generation_number = ti->GenerationNumber;
    
	sinfo->call_node_id = cnid;
	sinfo->parent_node_id = call_node->parent_node_id;
	sinfo->nameid = call_node->nameid;
	sinfo->all_time = 0;
    sinfo->call_count = 0;


}

static MethodNodeSerializedInfo *GetSerializedNodeInfo(unsigned int call_node_id)
{
    ThreadInfo *ti = CURRENT_THREAD;

    pthread_mutex_lock(&ti->DataMutex);
    MethodNodeInfo &call_node = ti->NodeInfoArray[call_node_id];
    if(call_node.generation_number != ti->GenerationNumber)
        AddActualRecord(call_node_id);
    MethodNodeSerializedInfo *ret = &(*ti->SerializedNodeInfoArray)[call_node.serialized_node_index];
    pthread_mutex_unlock(&ti->DataMutex);
    return ret;
}

static MethodNodeSerializedInfo *GetSerializedNodeInfoNoAdd(unsigned int call_node_id)
{
    ThreadInfo *ti = CURRENT_THREAD;
    pthread_mutex_lock(&ti->DataMutex);
    MethodNodeInfo &call_node = ti->NodeInfoArray[call_node_id];
    if(call_node.generation_number != ti->GenerationNumber)
        return NULL;
    MethodNodeSerializedInfo *ret = &(*ti->SerializedNodeInfoArray)[call_node.serialized_node_index];
    pthread_mutex_unlock(&ti->DataMutex);
    return ret;
}

static MethodNodeInfo *GetNodeInfo(unsigned int call_node_id)
{
    ThreadInfo* ti = CURRENT_THREAD;
    MethodNodeInfo* call_node = &ti->NodeInfoArray[call_node_id];
    return call_node;
}

/*
inline void get_current_node_info_pair(ThreadInfo *ti, MethodNodeInfo *&ninfo, MethodNodeSerializedInfo *&sinfo)
{
    sinfo = GetSerializedNodeInfo(ti->CurrentCallNodeID);
    ninfo = GetNodeInfo(ti->CurrentCallNodeID);
}

*/

inline void get_current_node_info_pair(ThreadInfo *ti, MethodNodeInfo *&ninfo, MethodNodeSerializedInfo *&sinfo)
{
    pthread_mutex_lock(&ti->DataMutex);
    ninfo = &ti->NodeInfoArray[ti->CurrentCallNodeID];
    if(ninfo->generation_number != ti->GenerationNumber)
        AddActualRecord(ti->CurrentCallNodeID);
    sinfo = &(*ti->SerializedNodeInfoArray)[ninfo->serialized_node_index];
    pthread_mutex_unlock(&ti->DataMutex);
}


    

unsigned int ThreadInfo::AddCallNode(unsigned int parent_node_id, nameid_t nameid, void *name_info_ptr)
{
    MethodNodeInfo *call_node = new_tail_elem(NodeInfoArray);
    call_node->serialized_node_index = 0;
    call_node->generation_number = 0;
    call_node->nameid = nameid;
    call_node->children = new children_t();
    
    GetNameIDTable()->AddCB(nameid, name_info_ptr);
    
    call_node->parent_node_id = parent_node_id;
    if(parent_node_id != 0)
    {
        GetNodeInfo(parent_node_id)->children->insert(make_pair(call_node->nameid, NodeInfoArray.size() - 1));
    }

    return NodeInfoArray.size() - 1;
}




void llprof_call_handler(nameid_t nameid, void *name_info)
{
    ThreadInfo* ti = CURRENT_THREAD;

	assert(ti->stop == 0);
	ti->stop = 1;

    unsigned int before = ti->CurrentCallNodeID;
    ti->CurrentCallNodeID = GetChildNodeID(ti, nameid, name_info);
    

    //MethodNodeSerializedInfo sinfo_data;
    //MethodNodeSerializedInfo *sinfo = &sinfo_data;

    MethodNodeSerializedInfo *sinfo;
    MethodNodeInfo *ninfo;
    get_current_node_info_pair(ti, ninfo, sinfo);

    sinfo->call_count++;
    ninfo->start_time = NOW_TIME;


    pthread_mutex_lock(&ti->StackMutex);
    StackInfo *stack_info = new_tail_elem(*ti->SerializedStackArray);
    stack_info->start_time = ninfo->start_time;
    stack_info->node_id = ti->CurrentCallNodeID;
    pthread_mutex_unlock(&ti->StackMutex);
 
    
	ti->stop = 0;
}


void llprof_return_handler()
{
    ThreadInfo* ti = CURRENT_THREAD;
    // Rubyのバグ？
    // rrprofのrequire終了時にも呼ばれる(バージョンがある)ようなのでスキップ

    if(ti->CurrentCallNodeID == 1)
    {
        return;
    }
    
	assert(ti->stop == 0);
	ti->stop = 1;
    MethodNodeSerializedInfo *sinfo;
    MethodNodeInfo *ninfo;

    get_current_node_info_pair(ti, ninfo, sinfo);
    
    time_val_t diff = NOW_TIME - ninfo->start_time;
    sinfo->all_time += diff;

    ti->CurrentCallNodeID = sinfo->parent_node_id;
#ifdef LLPROF_DEBUG
    assert(ti->CurrentCallNodeID != 0);
#endif

    pthread_mutex_lock(&ti->StackMutex);
    ti->SerializedStackArray->resize(ti->SerializedStackArray->size()-1);
    pthread_mutex_unlock(&ti->StackMutex);


	ti->stop = 0;	
}


void print_tree(unsigned int id, int indent);


typedef struct
{
	int indent;
} print_tree_prm_t;


/*
void print_tree_st(MethodNodeInfo *key, unsigned int id, st_data_t data)
{
	print_tree_prm_t *prm = (print_tree_prm_t *)data;
	print_tree(id, prm->indent);
}
void print_tree(unsigned int id, int indent)
{
    assert(0);
    ThreadInfo* ti = CURRENT_THREAD;
    MethodNodeInfo *info = &ti->NodeInfoArray[id];
	MethodNodeSerializedInfo *sinfo = &(*ti->SerializedNodeInfoArray)[id];

	int i = 0;
	for(; i < indent; i++)
	{
		cout << " ";
	}
	cout << info->mid_str << "(" << sinfo->call_count << ")"<< endl;
	
	print_tree_prm_t prm;
	prm.indent = indent + 2;
	st_foreach(info->children->tbl, (int (*)(...))print_tree_st, (st_data_t)&prm);
}


void print_table()
{
    assert(0);
    ThreadInfo *ti = CURRENT_THREAD;
    printf("%8s %16s %24s %8s %12s %16s %16s\n", "id", "class", "name", "p-id", "count", "all-t", "self-t");
	int i = 1;
	for(i = 1; i < ti->NodeInfoArray.size(); i++)
	{
		MethodNodeInfo *info = GetNodeInfo(i);
		MethodNodeSerializedInfo *sinfo = GetSerializedNodeInfoNoAdd(i);
        if(!sinfo)
            continue;
		printf(
            "%8d %16lld %24s %8d %12lld %16lld %16lld\n",
            i,
            (unsigned long long int)info->klass,
            info->mid_str,
            sinfo->parent_node_id,
            sinfo->call_count,
            sinfo->all_time,
            sinfo->all_time - sinfo->children_time
        );

	}
}

*/




void BufferIteration_Initialize(ThreadIterator *iter)
{
    iter->current_thread = gFirstThread;
    iter->phase = 0;
    iter->got_flag = 0;
}

int BufferIteration_NextBuffer(ThreadIterator *iter)
{
    iter->got_flag = 0;
    if(!iter->current_thread)
        return 0;
    if(iter->phase == 0)
    {
        iter->phase = 1;
        return 1;
    }
    if(iter->phase == 1)
    {
        iter->phase = 2;
        return 1;
    }
    if(iter->phase == 2)
    {
        iter->phase = 1;
        iter->current_thread = iter->current_thread->next;
        if(!iter->current_thread)
            return 0;
        return 1;
    }
    assert(0);
    return 0;
}


void CallTree_GetSerializedBuffer(ThreadInfo *thread, void **buf, unsigned int *size)
{
    pthread_mutex_lock(&thread->DataMutex);

    swap(thread->SerializedNodeInfoArray, gBackBuffer_SerializedNodeInfoArray);
    thread->GenerationNumber++;
    thread->SerializedNodeInfoArray->resize(0);

	*buf = &(*gBackBuffer_SerializedNodeInfoArray)[0];
    *size = gBackBuffer_SerializedNodeInfoArray->size() * sizeof(MethodNodeSerializedInfo);
    
    pthread_mutex_unlock(&thread->DataMutex);
}


void CallTree_GetSerializedStackBuffer(ThreadInfo *thread, void **buf, unsigned int *size)
{
    pthread_mutex_lock(&thread->StackMutex);
    int orig_sz = thread->SerializedStackArray->size();
    swap(thread->SerializedStackArray, gBackBuffer_SerializedStackArray);

 	*size = sizeof(StackInfo) * gBackBuffer_SerializedStackArray->size();
	*buf = &(*gBackBuffer_SerializedStackArray)[0];

    thread->SerializedStackArray->resize(orig_sz);    
    // protocol: 0番目レコードには現在の時刻情報をいれる
    (*gBackBuffer_SerializedStackArray)[0].start_time = NOW_TIME;
    

    memset(&(*thread->SerializedStackArray)[0], 0, sizeof(StackInfo) * thread->SerializedStackArray->size());
    pthread_mutex_unlock(&thread->StackMutex);

}

void BufferIteration_GetBuffer(ThreadIterator *iter, void **buf, unsigned int *size)
{
    assert(iter->got_flag == 0);
    iter->got_flag = 1;
    if(iter->phase == 1)
    {
        CallTree_GetSerializedBuffer(iter->current_thread, buf, size);
        return;
    }
    if(iter->phase == 2)
    {
        CallTree_GetSerializedStackBuffer(iter->current_thread, buf, size);
        return;
    }
    assert(0);
}

unsigned long long BufferIteration_GetThreadID(ThreadIterator *iter)
{
    return iter->current_thread->ThreadID;
}

int BufferIteration_GetBufferType(ThreadIterator *iter)
{
    if(iter->phase == 1)
    {
        return BT_PERFORMANCE;
    }
    if(iter->phase == 2)
    {
        return BT_STACK;
    }
    assert(0);
    return 0;
}


void llprof_calltree_init()
{
    pthread_mutex_init(&gThreadDataMutex, NULL);
    pthread_key_create(&gCurrentThreadKey, NULL);

    gBackBuffer_SerializedNodeInfoArray = new vector<MethodNodeSerializedInfo>();
    gBackBuffer_SerializedStackArray = new vector<StackInfo>();
    get_current_thread();
}


