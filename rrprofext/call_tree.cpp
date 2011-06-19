

#include <assert.h>
#include <time.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>

#include "platforms.h"

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
    
    nameid_t nameid;

	MethodTable *children;
    time_val_t start_time;
	unsigned int parent_node_id;
};

struct MethodNodeSerializedInfo
{
    nameid_t nameid;
    unsigned int call_node_id;
	unsigned int parent_node_id;
	time_val_t all_time;
	time_val_t children_time;
	unsigned long long int call_count;

};

struct StackInfo
{
    int node_id;
    time_val_t start_time;
};





void CallTree_GetSlide(slide_record_t **ret, int *nfield)
{
    static slide_record_t slides[16];
    int slide_idx = 0;
    {
        MethodNodeSerializedInfo test;
        
        ADD_SLIDE(nameid, "pdata.nameid")
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
        CurrentCallNodeID = AddCallNode(0, 0, NULL);

        next = NULL;

        #ifdef PRINT_DEBUG
            printf("New thread %lld\n", thread_id);
        #endif
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



int mtbl_hash_type_cmp(nameid_t *a, nameid_t *b) 
{
    return (*a != *b);
}

int mtbl_hash_type_hash(nameid_t *key) 
{
   return *key;
}

struct st_hash_type mtbl_hash_type = {
    (int (*)(...)) mtbl_hash_type_cmp,
    (st_index_t (*)(...)) mtbl_hash_type_hash
};



MethodTable* MethodTableCreate();

unsigned int MethodTableLookup(MethodTable *mtbl, nameid_t nameid);


MethodTable* MethodTableCreate()
{
	MethodTable* mtbl = ALLOC(MethodTable);
	mtbl->tbl = st_init_table(&mtbl_hash_type);
	return mtbl;
}

unsigned int MethodTableLookup(MethodTable *mtbl, nameid_t nameid)
{
	void *ret;
	nameid_t key = nameid;

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



template <class T>
T *new_tail_elem(vector<T> &arr)
{
    arr.resize(arr.size() + 1);
    return &arr[arr.size() - 1];
}

#ifdef DEBUG_NAMEMAP

map<unsigned long long, nameid_t> g_namemap;
unsigned long long toLong(unsigned int nid, nameid_t nameid)
{
    return ((unsigned long long)nid << 48) + (unsigned long long)nameid;
}

int viz_table_print(nameid_t *key, unsigned long long id, st_data_t data)
{
    cout << "  - " << *key << " ID:" << id << endl;
    return ST_CONTINUE;
}

void viz_table(st_table *tbl)
{
    st_foreach(tbl, (int (*)(...))viz_table_print, NULL);
}

#endif

unsigned int GetChildNodeID(unsigned int parent_id, nameid_t nameid, void *name_data_ptr)
{
    ThreadInfo *ti = CURRENT_THREAD;

	MethodNodeInfo *node_info = &ti->NodeInfoArray[parent_id];
    
    
    
    unsigned int id = MethodTableLookup(node_info->children, nameid);
	if(id == 0)
	{
		id = ti->AddCallNode(parent_id, nameid, name_data_ptr);
#ifdef DEBUG_NAMEMAP
        cout << "[New] ";
    }
    else
    {
        cout << "      ";
#endif
    }
#ifdef DEBUG_NAMEMAP
    cout << "P-ID:" << parent_id << " NameID:" << nameid << " id:" << id << " tbl:" << (void *)node_info->children->tbl << endl;
    viz_table(node_info->children->tbl);

    map<unsigned long long, nameid_t>::iterator it = g_namemap.find(toLong(parent_id, nameid));
    if(it == g_namemap.end())
    {
        g_namemap.insert(make_pair(toLong(parent_id, nameid), id));
    }
    else
    {
        if(((*it).second) != id)
        {
            cout << "Error!! Prev:" << (*it).second << endl;
            assert(false);
        }
    }        
#endif
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
	sinfo->nameid = call_node->nameid;
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


const char *GetNameFromNameInfo(void *name_info_ptr)
{
    if(!name_info_ptr)
        return "(null)";
    struct ruby_name_info_t{
        VALUE klass;
        ID id;
    };
    
    stringstream s;
    
    s << rb_class2name(((ruby_name_info_t*)name_info_ptr)->klass);
    s << "::";
    s << rb_id2name(((ruby_name_info_t*)name_info_ptr)->id);
    return s.str().c_str();
}

unsigned int ThreadInfo::AddCallNode(unsigned int parent_node_id, nameid_t nameid, void *name_info_ptr)
{
    MethodNodeInfo *call_node = new_tail_elem(NodeInfoArray);
    call_node->serialized_node_index = 0;
    call_node->generation_number = 0;
    call_node->nameid = nameid;
    call_node->children = MethodTableCreate();
    
    GetNameIDTable()->AddCB(nameid, GetNameFromNameInfo, name_info_ptr);
    
    call_node->parent_node_id = parent_node_id;
    if(parent_node_id != 0)
    {
        st_insert(
            GetNodeInfo(parent_node_id)->children->tbl,
            (st_data_t)new nameid_t(call_node->nameid), // call_nodeはvector上にあるので、動く可能性がある
            (st_data_t)(NodeInfoArray.size() - 1)
        );
    }

    return NodeInfoArray.size() - 1;
}


nameid_t RubyNameInfoToNameID(VALUE klass, ID id)
{
    return (unsigned long long)klass * (unsigned long long)id;
}

void rrprof_calltree_call_hook(rb_event_flag_t event, VALUE data, VALUE self, ID p_id, VALUE p_klass)
{
    ThreadInfo* ti = CURRENT_THREAD;

	if(ti->stop)
	{
		return;
	}
	ti->stop = 1;
    
    struct{
        VALUE klass;
        ID id;
    } ruby_name_info;
    if(event == RUBY_EVENT_C_CALL)
    {
        ruby_name_info.id = p_id;
        ruby_name_info.klass = p_klass;
    }
    else
    {
        rb_frame_method_id_and_class(&ruby_name_info.id, &ruby_name_info.klass);
    }
    
    nameid_t nameid = RubyNameInfoToNameID(ruby_name_info.klass, ruby_name_info.id);

    unsigned int before = ti->CurrentCallNodeID;
    ti->CurrentCallNodeID = GetChildNodeID(ti->CurrentCallNodeID, nameid, &ruby_name_info);
    
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


void rrprof_calltree_ret_hook(rb_event_flag_t event, VALUE data, VALUE self, ID p_id, VALUE p_klass)
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

    /*
	ID id;
	VALUE klass;
    if(event == RUBY_EVENT_C_RETURN)
    {
        id = p_id;
        klass = p_klass;
    }
    else
    {
        rb_frame_method_id_and_class(&id, &klass);
    }

    nameid_t nameid = RubyNameInfoToNameID(ruby_name_info.klass, ruby_name_info.id);
    */

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
    
	rb_add_event_hook(&rrprof_calltree_call_hook, RUBY_EVENT_CALL | RUBY_EVENT_C_CALL, Qnil);
	rb_add_event_hook(&rrprof_calltree_ret_hook, RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN, Qnil);
    
	//rb_add_event_hook(&rrprof_calltree_call_hook, RUBY_EVENT_CALL, Qnil);
	//rb_add_event_hook(&rrprof_calltree_ret_hook, RUBY_EVENT_RETURN, Qnil);
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
    assert(0);
    /*
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
    */
}


void print_table()
{
    assert(0);
    /*
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
    */
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
    return 0;
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

void CallTree_InitModule()
{
    pthread_mutex_init(&gThreadDataMutex, NULL);
    pthread_key_create(&gCurrentThreadKey, NULL);
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
    return 0;
}





