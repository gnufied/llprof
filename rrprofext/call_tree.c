
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "rrprof.h"
#include "call_tree.h"

typedef struct _tag_method_table
{
	st_table *tbl;
} method_table_t;


typedef struct _tag_method_node_info
{
    unsigned int serialized_node_index;
    unsigned int generation_number;
    
	ID mid;
	VALUE klass;
	const char *mid_str;
	const char *klass_str;

    struct method_node_key_t *st_table_key;
	method_table_t *children;
    time_val_t start_time;
	unsigned int parent_node_id;
} method_node_info_t;

#pragma pack(4)
typedef struct _tag_method_serialized_node_info
{
	ID mid;
	VALUE klass;
    //unsigned int call_node_id;
	//unsigned int parent_node_id;
    unsigned int call_node_id;
	unsigned int parent_node_id;
	time_val_t all_time;
	time_val_t children_time;
	unsigned long long int call_count;

} method_serialized_node_info_t;
#pragma pack()


#pragma pack(4)
typedef struct _tag_serialized_stack_info
{
    int node_id;
    time_val_t start_time;
} serialized_stack_info_t;
#pragma pack()



void CallTree_GetSlide(slide_record_t **ret, int *nfield)
{
    static slide_record_t slides[16];
    int slide_idx = 0;
    {
        method_serialized_node_info_t test;
        
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
        serialized_stack_info_t test;
        
        ADD_SLIDE(node_id, "sdata.node_id")
        ADD_SLIDE(start_time, "sdata.start_time")
        slides[slide_idx].field_name = "sdata.record_size";
        slides[slide_idx].slide = sizeof(test);
        slide_idx++;
    }

    *nfield = slide_idx;
    *ret = slides;
}


typedef struct _tag_thread_info {
    unsigned long long int ThreadID;

    method_node_info_t *NodeInfoTable;
    unsigned int NodeInfoTable_Size;


    method_serialized_node_info_t *SerializedNodeInfoTable;
    unsigned int SerializedNodeInfoTable_Size;
    // method_serialized_node_info_t *SerializedNodeInfoTable_back;

    serialized_stack_info_t *SerializedStackInfo;
    unsigned int SerializedStackInfo_Size;
    // serialized_stack_info_t *SerializedStackInfo_back;


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
    
    struct _tag_thread_info *next;

} thread_info_t;


void thread_info_alloc_node_table(thread_info_t *info, int size);
void thread_info_alloc_serialized_node_table(thread_info_t *info, int size);


unsigned int AddCallNode(thread_info_t *ti, unsigned int parent_node_id, ID mid, VALUE klass);


void thread_info_alloc_node_table(thread_info_t *info, int size);

//int gDefaultNodeInfoTableSize = 65536;
//int gDefaultSerializedNodeInfoTableSize = 65536;
int gDefaultNodeInfoTableSize = 16;
int gDefaultSerializedNodeInfoTableSize = 16;
int gDefaultSerializedStackInfoSize = 512;

thread_info_t* thread_info_new(unsigned long long thread_id)
{
    
    thread_info_t *info = (thread_info_t *)malloc(sizeof(thread_info_t));
    memset(info, 0, sizeof(thread_info_t));

    info->ThreadID = thread_id;

            
    pthread_mutex_init(&info->GlobalMutex, NULL);
    pthread_mutex_init(&info->DataMutex, NULL);
    pthread_mutex_init(&info->StackMutex, NULL);
    info->stop = 0;

    thread_info_alloc_node_table(info, gDefaultNodeInfoTableSize);
    
    thread_info_alloc_serialized_node_table(info, gDefaultSerializedNodeInfoTableSize);
    
    // info->SerializedNodeInfoTable = malloc(sizeof(method_serialized_node_info_t) * gDefaultSerializedNodeInfoTableSize);
    // info->SerializedNodeInfoTable_Size = gDefaultSerializedNodeInfoTableSize;

    info->SerializedStackInfo = malloc(sizeof(serialized_stack_info_t) * gDefaultSerializedStackInfoSize);
    info->SerializedStackInfo_Size = gDefaultSerializedStackInfoSize;

    info->StackSize = 1;
    
    memset(info->SerializedStackInfo, 0, sizeof(serialized_stack_info_t) * info->SerializedStackInfo_Size);

	info->CallNodeSeq = 1;
    info->ActualCallNodeSeq = 0;
    info->GenerationNumber = 1;
	info->CurrentCallNodeID = AddCallNode((thread_info_t *)info, 0, (ID)0, (VALUE)0);

    info->next = NULL;

    #ifdef PRINT_DEBUG
        printf("New thread %lld\n", thread_id);
    #endif
    return info;
}


void thread_info_alloc_node_table(thread_info_t *info, int size)
{
    pthread_mutex_lock(&info->GlobalMutex);
	info->NodeInfoTable = realloc(info->NodeInfoTable, sizeof(method_node_info_t) * size);
	if(info->NodeInfoTable)
        info->NodeInfoTable_Size = size;
    else
    {
        perror("Failed to allocate NodeInfoTable");
        abort();
    }
    pthread_mutex_unlock(&info->GlobalMutex);
    #ifdef PRINT_DEBUG
    printf("*** Allocate NodeInfoTable: %d records\n", size);
    #endif
}

void thread_info_alloc_serialized_node_table(thread_info_t *info, int size)
{
    pthread_mutex_lock(&info->GlobalMutex);
	info->SerializedNodeInfoTable = realloc(
        info->SerializedNodeInfoTable, sizeof(method_serialized_node_info_t) * size);
	if(info->SerializedNodeInfoTable)
        info->SerializedNodeInfoTable_Size = size;
    else
    {
        perror("Failed to allocate SerializedNodeInfoTable");
        abort();
    }
    pthread_mutex_unlock(&info->GlobalMutex);
    #ifdef PRINT_DEBUG
    printf("*** Allocate SerializedNodeInfoTable: %d records\n", size);
    #endif
}


thread_info_t *gFirstThread;
thread_info_t *gLastThread;
__thread thread_info_t *gCurrentThread = NULL;
pthread_mutex_t gThreadDataMutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long int gThreadMaxID = 0;

thread_info_t *get_current_thread()
{
    if(gCurrentThread != NULL)
        return gCurrentThread;
    pthread_mutex_lock(&gThreadDataMutex);
    gCurrentThread = thread_info_new(gThreadMaxID);
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
method_serialized_node_info_t *gBackBuffer_SerializedNodeInfoTable;
serialized_stack_info_t *gBackBuffer_SerializedStackInfo;

int gBackBuffer_SerializedNodeInfoTable_Size;
int gBackBuffer_SerializedStackInfo_Size;

void alloc_back_buffer()
{
    gBackBuffer_SerializedNodeInfoTable = malloc(sizeof(method_serialized_node_info_t) * gDefaultSerializedNodeInfoTableSize);
    gBackBuffer_SerializedNodeInfoTable_Size = gDefaultSerializedNodeInfoTableSize;

    gBackBuffer_SerializedStackInfo = malloc(sizeof(serialized_stack_info_t) * gDefaultSerializedStackInfoSize);
    gBackBuffer_SerializedStackInfo_Size = gDefaultSerializedStackInfoSize;
}


typedef struct _tag_method_node_key_t{
    ID mid;
    VALUE klass;
    int hashval;
} method_node_key_t;

int mtbl_hash_type_cmp(method_node_key_t *a, method_node_key_t *b) 
{
    return (a->klass != b->klass) || (a->mid != b->mid);
}

int mtbl_hash_type_hash(method_node_key_t *key) 
{
   return key->hashval;
}

struct st_hash_type mtbl_hash_type = {
    (int (*)()) mtbl_hash_type_cmp,
    (int (*)()) mtbl_hash_type_hash
};



method_table_t* MethodTableCreate();
void MethodKeyCalcHash(method_node_key_t* info);
unsigned int MethodTableLookup(method_table_t *mtbl, VALUE klass, ID mid);


method_table_t* MethodTableCreate()
{
	method_table_t* mtbl = ALLOC(method_table_t);
	mtbl->tbl = st_init_table(&mtbl_hash_type);
	return mtbl;
}

unsigned int MethodTableLookup(method_table_t *mtbl, VALUE klass, ID mid)
{
	void *ret;
	method_node_key_t key;
	key.klass = klass;
	key.mid = mid;
	MethodKeyCalcHash(&key);


	if(!st_lookup(mtbl->tbl, (st_data_t)&key, (st_data_t *)&ret))
	{
		return 0;
	}
    return (unsigned int)ret;

}


static method_serialized_node_info_t *GetSerializedNodeInfo(unsigned int call_node_id);
static method_node_info_t *GetNodeInfo(unsigned int call_node_id);



void MethodKeyCalcHash(method_node_key_t* info)
{
	info->hashval = info->mid + info->klass;
}
method_node_key_t* MethodKeyCreate(method_node_info_t *info)
{
    // 作り捨て
    // ハッシュテーブルに保持するため。
    // 高々コールノード数しか作成されない。
    
    method_node_key_t *key = (method_node_key_t *)malloc(sizeof(method_node_key_t));
    key->mid = info->mid;
    key->klass = info->klass;
	MethodKeyCalcHash(key);
    return key;
}

unsigned int AddCallNode(thread_info_t *ti, unsigned int parent_node_id, ID mid, VALUE klass)
{
    
	if(ti->CallNodeSeq >= ti->NodeInfoTable_Size)
    {
        thread_info_alloc_node_table(ti, ti->NodeInfoTable_Size*2);
	}
    
    method_node_info_t *call_node = ti->NodeInfoTable + ti->CallNodeSeq;
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
    thread_info_t *ti = CURRENT_THREAD;

	method_node_info_t *node_info = &ti->NodeInfoTable[parent_id];
    
    unsigned int id = MethodTableLookup(node_info->children, klass, mid);
	if(id == 0)
	{
		id = AddCallNode(ti, parent_id, mid, klass);
	}
    
	return id;
}




void AddActualRecord(unsigned int cnid)
{
    thread_info_t *ti = CURRENT_THREAD;

	if(ti->ActualCallNodeSeq >= ti->SerializedNodeInfoTable_Size)
    {
        thread_info_alloc_serialized_node_table(
            ti,
            ti->SerializedNodeInfoTable_Size * 2
        );
    }

    method_node_info_t *call_node = ti->NodeInfoTable + cnid;
    call_node->serialized_node_index = ti->ActualCallNodeSeq;
    call_node->generation_number = ti->GenerationNumber;

	method_serialized_node_info_t *sinfo = &ti->SerializedNodeInfoTable[call_node->serialized_node_index];
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

static method_serialized_node_info_t *GetSerializedNodeInfo(unsigned int call_node_id)
{
    thread_info_t *ti = CURRENT_THREAD;

    pthread_mutex_lock(&ti->DataMutex);
    method_node_info_t* call_node = ti->NodeInfoTable + call_node_id;
    if(call_node->generation_number != ti->GenerationNumber)
        AddActualRecord(call_node_id);
    method_serialized_node_info_t *ret = ti->SerializedNodeInfoTable + call_node->serialized_node_index;
    pthread_mutex_unlock(&ti->DataMutex);
    return ret;
}

static method_serialized_node_info_t *GetSerializedNodeInfoNoAdd(unsigned int call_node_id)
{
    thread_info_t *ti = CURRENT_THREAD;
    pthread_mutex_lock(&ti->DataMutex);
    method_node_info_t* call_node = ti->NodeInfoTable + call_node_id;
    if(call_node->generation_number != ti->GenerationNumber)
        return NULL;
    method_serialized_node_info_t *ret = ti->SerializedNodeInfoTable + call_node->serialized_node_index;
    pthread_mutex_unlock(&ti->DataMutex);

    return ret;
}

static method_node_info_t *GetNodeInfo(unsigned int call_node_id)
{
    thread_info_t* ti = CURRENT_THREAD;
    method_node_info_t* call_node = ti->NodeInfoTable + call_node_id;
    return call_node;
}


//----------------------------------------------------------------------
__thread unsigned long long gTLSDepth = 0;

#ifdef DO_NOTHING_IN_HOOK

void rrprof_calltree_call_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    gTLSDepth++;
    printf("Cal D=%lld\n", gTLSDepth);
    return;
}

void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    gTLSDepth--;
    printf("Ret D=%lld\n", gTLSDepth);    
    return;
}

#else // DO_NOTHING_IN_HOOK
void rrprof_calltree_call_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{

    thread_info_t* ti = CURRENT_THREAD;

	if(ti->stop)
	{
		return;
	}
	ti->stop = 1;
    
	ID id;
	VALUE klass;
    #ifdef ENABLE_RUBY_RUNTIME_INFO
        rb_frame_method_id_and_class(&id, &klass);
    #else
        id = 0;
        klass = 0;
	#endif

    #ifdef ENABLE_SEARCH_NODES
        unsigned int before = ti->CurrentCallNodeID;
        ti->CurrentCallNodeID = GetChildNodeID(ti->CurrentCallNodeID, id, klass);
        
        method_serialized_node_info_t *sinfo = GetSerializedNodeInfo(ti->CurrentCallNodeID);
        method_node_info_t *ninfo = GetNodeInfo(ti->CurrentCallNodeID);
    #else
        method_serialized_node_info_t sinfo_buf;
        method_serialized_node_info_t *sinfo = &sinfo_buf;
        method_node_info_t ninfo_buf;
        method_node_info_t *ninfo = &ninfo_buf;
    #endif

    #ifdef ENABLE_CALC_TIMES
        sinfo->call_count++;
        ninfo->start_time = NOW_TIME;
    #endif

    #ifdef ENABLE_STACK
        pthread_mutex_lock(&ti->StackMutex);
        if(ti->SerializedStackInfo_Size <= ti->StackSize) {
            ti->SerializedStackInfo = realloc(
                    ti->SerializedStackInfo,
                    sizeof(serialized_stack_info_t) * ti->SerializedStackInfo_Size * 2
                    );
            ti->SerializedStackInfo_Size = ti->SerializedStackInfo_Size * 2;
        }
        serialized_stack_info_t *stack_info = ti->SerializedStackInfo + ti->StackSize;
        ti->StackSize++;
        stack_info->start_time = ninfo->start_time;
        stack_info->node_id = ti->CurrentCallNodeID;
        pthread_mutex_unlock(&ti->StackMutex);
    #endif

	ti->stop = 0;	
}


void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    

    thread_info_t* ti = CURRENT_THREAD;
    // Rubyのバグ？
    // require終了時にも呼ばれる(バージョンがある)ようなのでスキップ
    #ifdef ENABLE_SEARCH_NODES
        if(ti->CurrentCallNodeID == 1)
        {
            return;
        }
    #endif
    
	if(ti->stop) return;
	ti->stop = 1;

	ID id;
	VALUE klass;

    #ifdef ENABLE_RUBY_RUNTIME_INFO
        rb_frame_method_id_and_class( &id, &klass);
    #else
        id = 0;
        klass = 0;
    #endif

    #ifdef ENABLE_SEARCH_NODES
        method_serialized_node_info_t *sinfo = GetSerializedNodeInfo(ti->CurrentCallNodeID);
        method_node_info_t *ninfo = GetNodeInfo(ti->CurrentCallNodeID);
        method_serialized_node_info_t *parent_sinfo = GetSerializedNodeInfo(sinfo->parent_node_id);
    #else
        method_serialized_node_info_t sinfo_buf;
        method_serialized_node_info_t *sinfo = &sinfo_buf;
        method_serialized_node_info_t parent_sinfo_buf;
        method_serialized_node_info_t *parent_sinfo = &parent_sinfo_buf;
        method_node_info_t ninfo_buf;
        method_node_info_t *ninfo = &ninfo_buf;
    #endif
    
    #ifdef ENABLE_CALC_TIMES
        time_val_t diff = NOW_TIME - ninfo->start_time;
        //time_val_t diff = 0;
        sinfo->all_time += diff;
        //sinfo->children_time += ninfo->children_time;
        ti->CurrentCallNodeID = sinfo->parent_node_id;
        #ifdef ENABLE_SEARCH_NODES
            assert(ti->CurrentCallNodeID != 0);
        #endif
        parent_sinfo->children_time += diff;
    #endif

    #ifdef ENABLE_STACK
        pthread_mutex_lock(&ti->StackMutex);
        ti->StackSize--;
        pthread_mutex_unlock(&ti->StackMutex);
    #endif

	ti->stop = 0;	
}

#endif // DO_NOTHING_IN_HOOK


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



void print_tree_st(method_node_info_t *key, unsigned int id, st_data_t data)
{
	print_tree_prm_t *prm = (print_tree_prm_t *)data;
	print_tree(id, prm->indent);
}

void print_tree(unsigned int id, int indent)
{
    thread_info_t* ti = CURRENT_THREAD;
    method_node_info_t *info = &ti->NodeInfoTable[id];
	method_serialized_node_info_t *sinfo = &ti->SerializedNodeInfoTable[id];

	int i = 0;
	for(; i < indent; i++)
	{
		printf(" ");
	}
	printf("%s(%d)\n", info->mid_str, sinfo->call_count);
	
	print_tree_prm_t prm;
	prm.indent = indent + 2;
	st_foreach(info->children->tbl, print_tree_st, (st_data_t)&prm);

}


void print_table()
{
    thread_info_t *ti = CURRENT_THREAD;
    printf("%8s %16s %24s %8s %12s %16s %16s\n", "id", "class", "name", "p-id", "count", "all-t", "self-t");
	int i = 1;
	for(i = 1; i < ti->CallNodeSeq; i++)
	{
		method_node_info_t *info = GetNodeInfo(i);
		method_serialized_node_info_t *sinfo = GetSerializedNodeInfoNoAdd(i);
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
    thread_iterator iter;
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
    PRINTSIZEOF(method_node_info_t);
    PRINTSIZEOF(method_serialized_node_info_t);
    #endif
}




void BufferIteration_Initialize(thread_iterator *iter)
{
    iter->current_thread = gFirstThread;
    iter->phase = 0;
    iter->got_flag = 0;
}

int BufferIteration_NextBuffer(thread_iterator *iter)
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


void CallTree_GetSerializedBuffer(thread_info_t *thread, void **buf, unsigned int *size)
{
    method_serialized_node_info_t *tmp;
    pthread_mutex_lock(&thread->DataMutex);
	*size = sizeof(method_serialized_node_info_t) * thread->ActualCallNodeSeq;
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

void CallTree_GetSerializedStackBuffer(thread_info_t *thread, void **buf, unsigned int *size)
{    
    serialized_stack_info_t *tmp;
    if(gBackBuffer_SerializedStackInfo)
    {
        memset(gBackBuffer_SerializedStackInfo, 0, sizeof(serialized_stack_info_t) * gBackBuffer_SerializedStackInfo_Size);
    }

    pthread_mutex_lock(&thread->StackMutex);
    tmp = thread->SerializedStackInfo;
    thread->SerializedStackInfo = gBackBuffer_SerializedStackInfo;
    gBackBuffer_SerializedStackInfo = tmp;

 	*size = sizeof(serialized_stack_info_t) * thread->StackSize;
	*buf = gBackBuffer_SerializedStackInfo;
    
    int tmp_sz;
    tmp_sz = gBackBuffer_SerializedStackInfo_Size;
    gBackBuffer_SerializedStackInfo_Size = thread->SerializedStackInfo_Size;
    thread->SerializedStackInfo_Size = tmp_sz;
    pthread_mutex_unlock(&thread->StackMutex);
    
    // protocol: 0番目レコードには現在の時刻情報をいれる
    gBackBuffer_SerializedStackInfo[0].start_time = NOW_TIME;
    
}

void BufferIteration_GetBuffer(thread_iterator *iter, void **buf, unsigned int *size)
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

unsigned long long BufferIteration_GetThreadID(thread_iterator *iter)
{
    return iter->current_thread->ThreadID;
}

int BufferIteration_GetBufferType(thread_iterator *iter)
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





