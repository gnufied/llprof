
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "rrprof.h"
#include "call_tree.h"
#include "class_table.h"

using namespace std;


//int gDefaultNodeInfoTableSize = 65536;
//int gDefaultSerializedNodeInfoTableSize = 65536;
int gDefaultNodeInfoTableSize = 16;
int gDefaultSerializedNodeInfoTableSize = 16;
int gDefaultSerializedStackInfoSize = 512;



struct MethodTable
{
	st_table *tbl;
};


struct MethodNodeInfo
{
    unsigned int serialized_node_index;
    unsigned int generation_number;
    
	ID mid;
	VALUE klass;
	const char *mid_str;
	const char *klass_str;

    struct CallTreeNodeKey *st_table_key;
	MethodTable *children;
    time_val_t start_time;
	unsigned int parent_node_id;
};

#pragma pack(4)
struct MethodNodeSerializedInfo
{
	ID mid;
	VALUE klass;
    unsigned int call_node_id;
	unsigned int parent_node_id;
	time_val_t all_time;
	time_val_t children_time;
	unsigned long long int call_count;

};
#pragma pack()


#pragma pack(4)
struct StackInfo
{
    int node_id;
    time_val_t start_time;
};
#pragma pack()



void CallTree_GetSlide(slide_record_t **ret, int *nfield)
{
    static slide_record_t slides[16];
    int slide_idx = 0;
    {
        MethodNodeSerializedInfo test;
        
        ADD_SLIDE(mid, "pdata.mid")
        ADD_SLIDE(klass, "pdata.class")
        ADD_SLIDE(all_time, "pdata.all_time")
        ADD_SLIDE(children_time, "pdata.children_time")
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


unsigned int AddCallNode(ThreadInfo *ti, unsigned int parent_node_id, ID mid, VALUE klass);


struct ThreadInfo
{
    unsigned long long int ThreadID;

    MethodNodeInfo *NodeInfoTable;
    unsigned int NodeInfoTable_Size;


    MethodNodeSerializedInfo *SerializedNodeInfoTable;
    unsigned int SerializedNodeInfoTable_Size;
    // MethodNodeSerializedInfo *SerializedNodeInfoTable_back;

    StackInfo *SerializedStackInfo;
    unsigned int SerializedStackInfo_Size;
    // StackInfo *SerializedStackInfo_back;


    unsigned int ActualCallNodeSeq;
    unsigned int CurrentCallNodeID;
    unsigned int CallNodeSeq;
    unsigned int GenerationNumber;

    unsigned int StackSize;
    unsigned int StackSize_back;

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

        AllocNodeTable(gDefaultNodeInfoTableSize);
        AllocSerializedInfoTable(gDefaultSerializedNodeInfoTableSize);
        
        // info->SerializedNodeInfoTable = malloc(sizeof(MethodNodeSerializedInfo) * gDefaultSerializedNodeInfoTableSize);
        // info->SerializedNodeInfoTable_Size = gDefaultSerializedNodeInfoTableSize;

        SerializedStackInfo = (StackInfo *)malloc(sizeof(StackInfo) * gDefaultSerializedStackInfoSize);
        SerializedStackInfo_Size = gDefaultSerializedStackInfoSize;

        StackSize = 1;
        
        memset(SerializedStackInfo, 0, sizeof(StackInfo) * SerializedStackInfo_Size);

        CallNodeSeq = 1;
        ActualCallNodeSeq = 0;
        GenerationNumber = 1;
        CurrentCallNodeID = AddCallNode(this, 0, (ID)0, (VALUE)0);

        next = NULL;

        #ifdef PRINT_DEBUG
            printf("New thread %lld\n", thread_id);
        #endif
    }


    void AllocNodeTable(int size)
    {
        pthread_mutex_lock(&GlobalMutex);
        NodeInfoTable = (MethodNodeInfo *)realloc(NodeInfoTable, sizeof(MethodNodeInfo) * size);
        if(NodeInfoTable)
            NodeInfoTable_Size = size;
        else
        {
            perror("Failed to allocate NodeInfoTable");
            abort();
        }
        pthread_mutex_unlock(&GlobalMutex);
        #ifdef PRINT_DEBUG
        cout << "*** Allocate NodeInfoTable: " << size << " records" << endl;
        #endif
    }


    void AllocSerializedInfoTable(int size)
    {
        pthread_mutex_lock(&GlobalMutex);
        SerializedNodeInfoTable = (MethodNodeSerializedInfo *)realloc(
            SerializedNodeInfoTable, sizeof(MethodNodeSerializedInfo) * size);
        if(SerializedNodeInfoTable)
            SerializedNodeInfoTable_Size = size;
        else
        {
            perror("Failed to allocate SerializedNodeInfoTable");
            abort();
        }
        pthread_mutex_unlock(&GlobalMutex);
        #ifdef PRINT_DEBUG
        printf("*** Allocate SerializedNodeInfoTable: %d records\n", size);
        #endif
    }


};


unsigned int AddCallNode(ThreadInfo *ti, unsigned int parent_node_id, ID mid, VALUE klass);


void thread_info_alloc_node_table(ThreadInfo *info, int size);





ThreadInfo *gFirstThread;
ThreadInfo *gLastThread;
__thread ThreadInfo *gCurrentThread = NULL;
pthread_mutex_t gThreadDataMutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long int gThreadMaxID = 0;

ThreadInfo *get_current_thread()
{
    if(gCurrentThread != NULL)
        return gCurrentThread;
    pthread_mutex_lock(&gThreadDataMutex);
    gCurrentThread = new ThreadInfo(gThreadMaxID);
    gThreadMaxID++;
    
    if(!gLastThread)
    {
        gFirstThread = gLastThread = gCurrentThread;
    }
    else
    {
        gLastThread->next = gCurrentThread;
        gLastThread = gCurrentThread;
    }
    pthread_mutex_unlock(&gThreadDataMutex);
    assert(gCurrentThread);
    return gCurrentThread;
}
#define CURRENT_THREAD      ((get_current_thread()))


// 送信用バッファ
MethodNodeSerializedInfo *gBackBuffer_SerializedNodeInfoTable;
StackInfo *gBackBuffer_SerializedStackInfo;

int gBackBuffer_SerializedNodeInfoTable_Size;
int gBackBuffer_SerializedStackInfo_Size;

void alloc_back_buffer()
{
    gBackBuffer_SerializedNodeInfoTable = (MethodNodeSerializedInfo *)malloc(sizeof(MethodNodeSerializedInfo) * gDefaultSerializedNodeInfoTableSize);
    gBackBuffer_SerializedNodeInfoTable_Size = gDefaultSerializedNodeInfoTableSize;

    gBackBuffer_SerializedStackInfo = (StackInfo *)malloc(sizeof(StackInfo) * gDefaultSerializedStackInfoSize);
    gBackBuffer_SerializedStackInfo_Size = gDefaultSerializedStackInfoSize;
}


struct CallTreeNodeKey{
    ID mid;
    VALUE klass;
    int hashval;
};

int mtbl_hash_type_cmp(CallTreeNodeKey *a, CallTreeNodeKey *b) 
{
    return (a->klass != b->klass) || (a->mid != b->mid);
}

int mtbl_hash_type_hash(CallTreeNodeKey *key) 
{
   return key->hashval;
}

struct st_hash_type mtbl_hash_type = {
    (int (*)(...)) mtbl_hash_type_cmp,
    (st_index_t (*)(...)) mtbl_hash_type_hash
};



MethodTable* MethodTableCreate();
void MethodKeyCalcHash(CallTreeNodeKey* info);
unsigned int MethodTableLookup(MethodTable *mtbl, VALUE klass, ID mid);


MethodTable* MethodTableCreate()
{
	MethodTable* mtbl = ALLOC(MethodTable);
	mtbl->tbl = st_init_table(&mtbl_hash_type);
	return mtbl;
}

unsigned int MethodTableLookup(MethodTable *mtbl, VALUE klass, ID mid)
{
	void *ret;
	CallTreeNodeKey key;
	key.klass = klass;
	key.mid = mid;
	MethodKeyCalcHash(&key);


	if(!st_lookup(mtbl->tbl, (st_data_t)&key, (st_data_t *)&ret))
	{
		return 0;
	}
    return 
        static_cast<unsigned int>(
            reinterpret_cast<unsigned long long int>(ret)
        );

}


static MethodNodeSerializedInfo *GetSerializedNodeInfo(unsigned int call_node_id);
static MethodNodeInfo *GetNodeInfo(unsigned int call_node_id);



void MethodKeyCalcHash(CallTreeNodeKey* info)
{
	info->hashval = info->mid + info->klass;
}
CallTreeNodeKey* MethodKeyCreate(MethodNodeInfo *info)
{
    // 作り捨て
    // ハッシュテーブルに保持するため。
    // 高々コールノード数しか作成されない。
    
    CallTreeNodeKey *key = (CallTreeNodeKey *)malloc(sizeof(CallTreeNodeKey));
    key->mid = info->mid;
    key->klass = info->klass;
	MethodKeyCalcHash(key);
    return key;
}

unsigned int AddCallNode(ThreadInfo *ti, unsigned int parent_node_id, ID mid, VALUE klass)
{
    
	if(ti->CallNodeSeq >= ti->NodeInfoTable_Size)
    {
        thread_info_alloc_node_table(ti, ti->NodeInfoTable_Size*2);
	}
    
    MethodNodeInfo *call_node = ti->NodeInfoTable + ti->CallNodeSeq;
    call_node->serialized_node_index = 0;
    call_node->generation_number = 0;

	call_node->mid = mid;
	call_node->klass = klass;
	call_node->children = MethodTableCreate();
	//call_node->mid_str = rb_id2name(mid);
	//call_node->klass_str = rb_class2name(klass);
    AddClassName(klass);
    //call_node->klass_str = NULL;
    call_node->parent_node_id = parent_node_id;
	call_node->st_table_key = MethodKeyCreate(call_node);


	if(parent_node_id != 0)
	{
		st_insert(
            GetNodeInfo(parent_node_id)->children->tbl,
            (st_data_t)call_node->st_table_key,
            (st_data_t)ti->CallNodeSeq
        );
	}



	return ti->CallNodeSeq++;
}






unsigned int GetChildNodeID(unsigned int parent_id, ID mid, VALUE klass)
{
    ThreadInfo *ti = CURRENT_THREAD;

	MethodNodeInfo *node_info = &ti->NodeInfoTable[parent_id];
    
    unsigned int id = MethodTableLookup(node_info->children, klass, mid);
	if(id == 0)
	{
		id = AddCallNode(ti, parent_id, mid, klass);
	}
    
	return id;
}




void AddActualRecord(unsigned int cnid)
{
    ThreadInfo *ti = CURRENT_THREAD;

	if(ti->ActualCallNodeSeq >= ti->SerializedNodeInfoTable_Size)
    {
        ti->AllocSerializedInfoTable(ti->SerializedNodeInfoTable_Size * 2);
    }

    MethodNodeInfo *call_node = ti->NodeInfoTable + cnid;
    call_node->serialized_node_index = ti->ActualCallNodeSeq;
    call_node->generation_number = ti->GenerationNumber;

	MethodNodeSerializedInfo *sinfo = &ti->SerializedNodeInfoTable[call_node->serialized_node_index];
	sinfo->call_node_id = cnid;
	sinfo->parent_node_id = call_node->parent_node_id;
	sinfo->mid = call_node->mid;
	sinfo->klass = call_node->klass;
	sinfo->all_time = 0;
    sinfo->children_time = 0;
    sinfo->call_count = 0;
	ti->ActualCallNodeSeq++;

    /*
	if(sinfo->parent_node_id != 0)
	{
		st_insert(
            GetNodeInfo(call_node->parent_node_id)->children->tbl,
            (st_data_t)call_node,
            (st_data_t)cnid
        );
	}
    */

}

static MethodNodeSerializedInfo *GetSerializedNodeInfo(unsigned int call_node_id)
{
    ThreadInfo *ti = CURRENT_THREAD;

    pthread_mutex_lock(&ti->DataMutex);
    MethodNodeInfo* call_node = ti->NodeInfoTable + call_node_id;
    if(call_node->generation_number != ti->GenerationNumber)
        AddActualRecord(call_node_id);
    MethodNodeSerializedInfo *ret = ti->SerializedNodeInfoTable + call_node->serialized_node_index;
    pthread_mutex_unlock(&ti->DataMutex);
    return ret;
}

static MethodNodeSerializedInfo *GetSerializedNodeInfoNoAdd(unsigned int call_node_id)
{
    ThreadInfo *ti = CURRENT_THREAD;
    pthread_mutex_lock(&ti->DataMutex);
    MethodNodeInfo* call_node = ti->NodeInfoTable + call_node_id;
    if(call_node->generation_number != ti->GenerationNumber)
        return NULL;
    MethodNodeSerializedInfo *ret = ti->SerializedNodeInfoTable + call_node->serialized_node_index;
    pthread_mutex_unlock(&ti->DataMutex);

    return ret;
}

static MethodNodeInfo *GetNodeInfo(unsigned int call_node_id)
{
    ThreadInfo* ti = CURRENT_THREAD;
    MethodNodeInfo* call_node = ti->NodeInfoTable + call_node_id;
    return call_node;
}

void rrprof_calltree_call_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    ThreadInfo* ti = CURRENT_THREAD;

	if(ti->stop)
	{
		return;
	}
	ti->stop = 1;
    
	ID id;
	VALUE klass;

    rb_frame_method_id_and_class(&id, &klass);

    unsigned int before = ti->CurrentCallNodeID;
    ti->CurrentCallNodeID = GetChildNodeID(ti->CurrentCallNodeID, id, klass);
    
    MethodNodeSerializedInfo *sinfo = GetSerializedNodeInfo(ti->CurrentCallNodeID);
    MethodNodeInfo *ninfo = GetNodeInfo(ti->CurrentCallNodeID);

    sinfo->call_count++;
    ninfo->start_time = NOW_TIME;

    pthread_mutex_lock(&ti->StackMutex);
    if(ti->SerializedStackInfo_Size <= ti->StackSize) {
        ti->SerializedStackInfo = (StackInfo *)realloc(
                ti->SerializedStackInfo,
                sizeof(StackInfo) * ti->SerializedStackInfo_Size * 2
                );
        ti->SerializedStackInfo_Size = ti->SerializedStackInfo_Size * 2;
    }
    StackInfo *stack_info = ti->SerializedStackInfo + ti->StackSize;
    ti->StackSize++;
    stack_info->start_time = ninfo->start_time;
    stack_info->node_id = ti->CurrentCallNodeID;
    pthread_mutex_unlock(&ti->StackMutex);

	ti->stop = 0;	
}


void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    

    ThreadInfo* ti = CURRENT_THREAD;
    // Rubyのバグ？
    // require終了時にも呼ばれる(バージョンがある)ようなのでスキップ

    if(ti->CurrentCallNodeID == 1)
    {
        return;
    }
    
	if(ti->stop) return;
	ti->stop = 1;

	ID id;
	VALUE klass;

    rb_frame_method_id_and_class( &id, &klass);
    MethodNodeSerializedInfo *sinfo = GetSerializedNodeInfo(ti->CurrentCallNodeID);
    MethodNodeInfo *ninfo = GetNodeInfo(ti->CurrentCallNodeID);
    MethodNodeSerializedInfo *parent_sinfo = GetSerializedNodeInfo(sinfo->parent_node_id);
    
    time_val_t diff = NOW_TIME - ninfo->start_time;
    //time_val_t diff = 0;
    sinfo->all_time += diff;
    //sinfo->children_time += ninfo->children_time;
    ti->CurrentCallNodeID = sinfo->parent_node_id;
    assert(ti->CurrentCallNodeID != 0);
    parent_sinfo->children_time += diff;

    pthread_mutex_lock(&ti->StackMutex);
    ti->StackSize--;
    pthread_mutex_unlock(&ti->StackMutex);

	ti->stop = 0;	
}


void CallTree_RegisterModeFunction()
{
	rb_add_event_hook(&rrprof_calltree_call_hook, RUBY_EVENT_CALL, Qnil);
	rb_add_event_hook(&rrprof_calltree_ret_hook, RUBY_EVENT_RETURN, Qnil);
}


void print_tree(unsigned int id, int indent);


typedef struct
{
	int indent;
} print_tree_prm_t;



void print_tree_st(MethodNodeInfo *key, unsigned int id, st_data_t data)
{
	print_tree_prm_t *prm = (print_tree_prm_t *)data;
	print_tree(id, prm->indent);
}

void print_tree(unsigned int id, int indent)
{
    ThreadInfo* ti = CURRENT_THREAD;
    MethodNodeInfo *info = &ti->NodeInfoTable[id];
	MethodNodeSerializedInfo *sinfo = &ti->SerializedNodeInfoTable[id];

	int i = 0;
	for(; i < indent; i++)
	{
		printf(" ");
	}
	cout << info->mid_str << "(" << sinfo->call_count << ")"<< endl;
	
	print_tree_prm_t prm;
	prm.indent = indent + 2;
	st_foreach(info->children->tbl, (int (*)(...))print_tree_st, (st_data_t)&prm);

}


void print_table()
{
    ThreadInfo *ti = CURRENT_THREAD;
    printf("%8s %16s %24s %8s %12s %16s %16s\n", "id", "class", "name", "p-id", "count", "all-t", "self-t");
	int i = 1;
	for(i = 1; i < ti->CallNodeSeq; i++)
	{
		MethodNodeInfo *info = GetNodeInfo(i);
		MethodNodeSerializedInfo *sinfo = GetSerializedNodeInfoNoAdd(i);
        if(!sinfo)
            continue;
		printf(
            "%8d %16lld %24s %8d %12d %16lld %16lld\n",
            i,
            info->klass,
            info->mid_str,
            sinfo->parent_node_id,
            sinfo->call_count,
            sinfo->all_time,
            sinfo->all_time - sinfo->children_time
        );

	}

}

VALUE CallTree_PrintStat(VALUE self, VALUE obj)
{
	
	printf("[All threads]\n");
    ThreadIterator iter;
    BufferIteration_Initialize(&iter);
    unsigned long long int node_counter = 0;
    while(BufferIteration_NextBuffer(&iter))
    {
        if(iter.phase != 1)
            continue;
        printf("Thread %lld:\n", iter.current_thread->ThreadID);
        printf("  Call Nodes: %d\n", iter.current_thread->CallNodeSeq);
        node_counter += iter.current_thread->CallNodeSeq;
        
        
        unsigned long long int tbl_sz = 0;
        int i;
        for(i = 1; i < iter.current_thread->CallNodeSeq; i++)
        {
            tbl_sz += st_memsize(iter.current_thread->NodeInfoTable[i].children->tbl);
        }
        printf("  Table Size: %lld\n", tbl_sz);
    }

    
	printf("[St]\n");
    printf("  All Call Nodes: %lld\n", node_counter);
    
    //print_table();
}


#define PRINTSIZEOF(tid)    {printf("Sizeof %s: %d\n", #tid, sizeof(tid));}

void CallTree_Init()
{
    InitClassTbl();
    get_current_thread();
	alloc_back_buffer();
    

    #ifdef PRINT_DEBUG
    PRINTSIZEOF(MethodNodeInfo);
    PRINTSIZEOF(MethodNodeSerializedInfo);
    #endif
}




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
    MethodNodeSerializedInfo *tmp;
    pthread_mutex_lock(&thread->DataMutex);
	*size = sizeof(MethodNodeSerializedInfo) * thread->ActualCallNodeSeq;
    tmp = thread->SerializedNodeInfoTable;
    thread->SerializedNodeInfoTable = gBackBuffer_SerializedNodeInfoTable;
    thread->GenerationNumber++;
    thread->ActualCallNodeSeq = 0;
    gBackBuffer_SerializedNodeInfoTable = tmp;
	*buf = gBackBuffer_SerializedNodeInfoTable;
    
    int tmp_sz;
    tmp_sz = gBackBuffer_SerializedNodeInfoTable_Size;
    gBackBuffer_SerializedNodeInfoTable_Size = thread->SerializedNodeInfoTable_Size;
    thread->SerializedNodeInfoTable_Size = tmp_sz;
    pthread_mutex_unlock(&thread->DataMutex);
}

void CallTree_GetSerializedStackBuffer(ThreadInfo *thread, void **buf, unsigned int *size)
{    
    StackInfo *tmp;
    if(gBackBuffer_SerializedStackInfo)
    {
        memset(gBackBuffer_SerializedStackInfo, 0, sizeof(StackInfo) * gBackBuffer_SerializedStackInfo_Size);
    }

    pthread_mutex_lock(&thread->StackMutex);
    tmp = thread->SerializedStackInfo;
    thread->SerializedStackInfo = gBackBuffer_SerializedStackInfo;
    gBackBuffer_SerializedStackInfo = tmp;

 	*size = sizeof(StackInfo) * thread->StackSize;
	*buf = gBackBuffer_SerializedStackInfo;
    
    int tmp_sz;
    tmp_sz = gBackBuffer_SerializedStackInfo_Size;
    gBackBuffer_SerializedStackInfo_Size = thread->SerializedStackInfo_Size;
    thread->SerializedStackInfo_Size = tmp_sz;
    pthread_mutex_unlock(&thread->StackMutex);
    
    // protocol: 0番目レコードには現在の時刻情報をいれる
    gBackBuffer_SerializedStackInfo[0].start_time = NOW_TIME;
    
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
}





