
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <vector>

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

        cout << "[NewThread]"  << endl << "  pNodes = " << SerializedNodeInfoArray << endl;
        cout << "  pStack = " << SerializedStackArray << endl;

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
        CurrentCallNodeID = AddCallNode(0, (ID)0, (VALUE)0);

        next = NULL;

        #ifdef PRINT_DEBUG
            printf("New thread %lld\n", thread_id);
        #endif
    }

    unsigned int AddCallNode(unsigned int parent_node_id, ID mid, VALUE klass);


};




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
vector<MethodNodeSerializedInfo>* gBackBuffer_SerializedNodeInfoArray;
vector<StackInfo>* gBackBuffer_SerializedStackArray;


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

template <class T>
T *new_tail_elem(vector<T> &arr)
{
    arr.resize(arr.size() + 1);
    return &arr[arr.size() - 1];
}


unsigned int GetChildNodeID(unsigned int parent_id, ID mid, VALUE klass)
{
    ThreadInfo *ti = CURRENT_THREAD;

	MethodNodeInfo *node_info = &ti->NodeInfoArray[parent_id];
    
    unsigned int id = MethodTableLookup(node_info->children, klass, mid);
	if(id == 0)
	{
		id = ti->AddCallNode(parent_id, mid, klass);
	}
    
	return id;
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
	sinfo->mid = call_node->mid;
	sinfo->klass = call_node->klass;
	sinfo->all_time = 0;
    sinfo->children_time = 0;
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


const char *GetRbClassName(NameTable::Key key)
{
    return rb_class2name((VALUE)key);
}

const char *GetRbMethodName(NameTable::Key key)
{
    return rb_id2name((ID)key);
}

unsigned int ThreadInfo::AddCallNode(unsigned int parent_node_id, ID mid, VALUE klass)
{
    MethodNodeInfo *call_node = new_tail_elem(NodeInfoArray);
    call_node->serialized_node_index = 0;
    call_node->generation_number = 0;
    call_node->mid = mid;
    call_node->klass = klass;
    call_node->children = MethodTableCreate();
    
    GetClassNameTable()->AddCB(klass, GetRbClassName);
    GetMethodNameTable()->AddCB(mid, GetRbMethodName);
    
    call_node->parent_node_id = parent_node_id;
    call_node->st_table_key = MethodKeyCreate(call_node);
    if(parent_node_id != 0)
    {
        st_insert(
            GetNodeInfo(parent_node_id)->children->tbl,
            (st_data_t)call_node->st_table_key,
            (st_data_t)(NodeInfoArray.size() - 1)
        );
    }

    return NodeInfoArray.size() - 1;
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
    StackInfo *stack_info = new_tail_elem(*ti->SerializedStackArray);
    stack_info->start_time = ninfo->start_time;
    stack_info->node_id = ti->CurrentCallNodeID;
    pthread_mutex_unlock(&ti->StackMutex);

	ti->stop = 0;	
}


void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID _d_id, VALUE _d_klass)
{
    

    ThreadInfo* ti = CURRENT_THREAD;
    // Rubyのバグ？
    // rrprofのrequire終了時にも呼ばれる(バージョンがある)ようなのでスキップ

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
    sinfo->all_time += diff;

    ti->CurrentCallNodeID = sinfo->parent_node_id;
    assert(ti->CurrentCallNodeID != 0);
    parent_sinfo->children_time += diff;

    pthread_mutex_lock(&ti->StackMutex);
    ti->SerializedStackArray->resize(ti->SerializedStackArray->size()-1);
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
        printf("  Call Nodes: %d\n", (int)iter.current_thread->NodeInfoArray.size());
        node_counter += iter.current_thread->NodeInfoArray.size();
        
        
        unsigned long long int tbl_sz = 0;
        int i;
        for(i = 1; i < iter.current_thread->NodeInfoArray.size(); i++)
        {
            tbl_sz += st_memsize(iter.current_thread->NodeInfoArray[i].children->tbl);
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
    gBackBuffer_SerializedNodeInfoArray = new vector<MethodNodeSerializedInfo>();
    gBackBuffer_SerializedStackArray = new vector<StackInfo>();

    cout << "[Init]"  << endl << "  pNodes = " << gBackBuffer_SerializedNodeInfoArray << endl;
    cout << "  pStack = " << gBackBuffer_SerializedStackArray << endl;

    get_current_thread();
   

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
}





