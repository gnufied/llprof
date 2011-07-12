#ifndef LLPROF_DATA_STORE_H
#define LLPROF_DATA_STORE_H

#include <pthread.h>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <memory>
#include <set>
#include <assert.h>
#include "boost_wrap.h"
#include <iostream>

namespace llprof{

using namespace std;


class ReferenceCounted
{
    int ref_cnt_;
public:
    ReferenceCounted()
    {
        ref_cnt_ = 0;
    }
    
    virtual ~ReferenceCounted()
    {
        assert(ref_cnt_ == 0);
    }
    
    void AddRef()
    {
        ref_cnt_++;
    }
    
    void Release()
    {
        ref_cnt_--;
        if(ref_cnt_ == 0)
            delete this;
    }

};


class MessageBuf: public ReferenceCounted
{
    char *buf_;
    int sz_, ptr_;
    
public:
    MessageBuf()
    {
        buf_ = NULL;
        sz_ = 0;
        ptr_ = 0;
    }
    ~MessageBuf()
    {
        delete []buf_;
    }
    void Resize(int sz)
    {
        char *newbuf = new char[sz];
        if(buf_)
        {
            memcpy(newbuf, buf_, sz_);
            delete []buf_;
        }
        buf_ = newbuf;
        sz_ = sz;
    }
    
    void FitSize()
    {
        Resize(GetMessageSize()+8);
        
    }
    
    int GetAllSize(){return sz_;}
    
    char *GetBuf(){return buf_;}
    unsigned int GetMessageType(){return ((unsigned int*)buf_)[0];}
    unsigned int GetMessageSize()
    {
        return ((unsigned int*)buf_)[1];
    }
    char *GetBody(){return buf_+8;}
    
    int ReadFrom(int sock, int offset, int sz);
    
    string AsString()
    {
        return string(buf_+8, sz_-8);
    }
    
    int GetCurrentPtr(){return ptr_;}
    void SetCurrentPtr(int val){ptr_ = val;}
    
    char *GetCurrentBufPtr(){return buf_ + ptr_ + 8;}
    void Seek(int offset)
    {
        assert(ptr_ + offset <= sz_);
        ptr_ += offset;
    }
    
    template<typename T>
    T Get()
    {
        assert(sizeof(T) + ptr_ <= (unsigned int)sz_);
        T result = *(T *)GetCurrentBufPtr();
        Seek(sizeof(T));
        return result;
    }
    
    bool IsEOF(){return GetRemainSize() <= 0;}
    int GetRemainSize(){return sz_ - ptr_ - 8;}
};


struct RecordMetadata
{
    string FieldName;
    string Unit;
    string Flags;
    bool AccumulatedValueFlag, StaticValueFlag;

    void AssocFlag(char c, bool &flag)
    {
        flag = (Flags.find(c) != string::npos);
    }

    void UpdateFlag()
    {
        AssocFlag('A', AccumulatedValueFlag);
        AssocFlag('S', StaticValueFlag);
    }

};
inline ostream &operator <<(ostream &s, const RecordMetadata &md)
{
    return s << "(Metadata: " << md.FieldName << " (" << md.Unit << ", " << md.Flags << ")";
}



typedef unsigned long NodeID;
typedef unsigned long long NameID;
typedef unsigned long long ThreadID;
typedef signed long long ProfileValue;

class DataStore;
class ThreadStore;

class RecordNodeBasic
{
protected:
    DataStore *ds_;
    NodeID id_, parent_id_;
    NameID node_name_;
    vector<ProfileValue> all_values_, children_values_;
    bool running_;
    virtual void UpdateDataStore(){};
    
public:
    RecordNodeBasic()
    {
        running_ = false;
    }
    void SetDataStore(DataStore *ds);
    NodeID GetNodeID() const {return id_;}
    void SetNodeID(NodeID id){ id_ = id; }
    
    NodeID GetParentNodeID() const {return parent_id_; }
    void SetParentNodeID(NodeID id){ parent_id_ = id; }
    
    NameID GetNameID() const { return node_name_; }
    void SetNameID(NameID id){ node_name_ = id; }
    
    string GetNameString();
    
    void SetRunning(bool f){running_ = f;}
    bool IsRunning() const {return running_;}
    
    
    vector<ProfileValue> &GetAllValues(){return all_values_;}
    vector<ProfileValue> &GetChildrenValues(){return children_values_;}
    const vector<ProfileValue> &GetAllValues() const {return all_values_;}
    const vector<ProfileValue> &GetChildrenValues() const {return children_values_;}

    DataStore *GetDataStore() const {return ds_;}
    
    void Accumulate(const RecordNodeBasic &rhs);
    
    virtual void PrintToStream(ostream &out) const;
    
    bool ValuesEquals(const RecordNodeBasic &rhs);
};

inline ostream &operator <<(ostream &s, const RecordNodeBasic &md)
{
    md.PrintToStream(s);
    return s;
}

class RecordNode: public RecordNodeBasic
{
    vector<ProfileValue> temp_values_;
    bool dirty_;
    set<NodeID> children_id_;
    NodeID gnid_;

protected:
    virtual void UpdateDataStore();

public:
    RecordNode()
    {
        dirty_ = false;
        gnid_ = 0;
    }
    
    
    NodeID GetGNID() const {return gnid_;}
    void SetGNID(NodeID gnid){gnid_ = gnid;}
    
    vector<ProfileValue> &GetTempValues(){return temp_values_;}
    const vector<ProfileValue> &GetTempValues() const {return temp_values_;}

    
    void AddChildID(NodeID cid){children_id_.insert(cid);}
    
    typedef set<NodeID>::iterator children_iterator;
    children_iterator children_begin()
    {
        return children_id_.begin();
    }
    children_iterator children_end()
    {
        return children_id_.end();
    }

    void SetDirty(bool f){dirty_ = f;}
    bool IsDirty() const {return dirty_;}
    

    void SetTempValues(const vector<ProfileValue> &value)
    {
        assert(temp_values_.size() == value.size());
        temp_values_ = value;
    }
    
    void ClearTempValues()
    {
        for(vector<ProfileValue>::iterator it = GetTempValues().begin(); it != GetTempValues().end(); it++)
        {
            (*it) = 0;
        }
    }
    
    void PrintToStream(ostream &out) const;

};



void write_json(stringstream &strm, const RecordNodeBasic &node);

class TimeSliceStore
{
    ThreadStore *ts_;
    int time_number_;
    map< NodeID, RecordNodeBasic > tree_;
    
    vector<ProfileValue> now_values_;
    NodeID running_node_;
    
public:
    ThreadStore *GetThreadStore(){return ts_;}

    void SetThreadStore(ThreadStore *ts);
    void SetTimeNumber(int tn);
    
    RecordNodeBasic *GetNodeFromID(NodeID id, bool add = true);
    
    void DumpText(ostream &strm);
    
    
    typedef map< NodeID, RecordNodeBasic >::iterator map_iterator;
    
    
    void SetNowValues(NodeID running_node, const vector<ProfileValue> &pdata)
    {
        now_values_ = pdata;
        running_node_ = running_node;
    }
    
    const vector<ProfileValue> &GetNowValues(){return now_values_;}
    NodeID GetRunningNode(){return running_node_;}
    
    map_iterator nodes_begin(){return tree_.begin();}
    map_iterator nodes_end(){return tree_.end();}
};

class ThreadStore
{
protected:
    DataStore *ds_;
    map<NodeID, RecordNode> current_tree_;
    
    int start_time_number_;
    vector<TimeSliceStore> time_slice_;
    vector<ProfileValue> now_values_;
    NodeID running_node_;
    ThreadID thread_id_;


public:
    ThreadStore(DataStore *ds);
    virtual ~ThreadStore(){}
    
    
    RecordNode *GetRootNode();
    
    RecordNode *GetNodeFromID(NodeID nid)
    {
        map<NodeID, RecordNode>::iterator it = current_tree_.find(nid);
        if(it == current_tree_.end())
            return NULL;
        return &(*it).second;
    }
    
    void SetThreadID(ThreadID id){thread_id_ = id;}
    ThreadID GetThreadID(){return thread_id_;}
    
    void Store(intrusive_ptr<MessageBuf> buf, intrusive_ptr<MessageBuf> nowinfo_buf);
    
    TimeSliceStore *GetTimeSlice(int ts);
    TimeSliceStore *GetCurrentTimeSlice();
    
    
    void DumpText(ostream &strm);
    
    DataStore *GetDataStore(){return ds_;}
    void DumpJSON(stringstream &strm, int flag, int p1, int p2);
    
    void UpdateNowValues(NodeID running_node, void *pdata);
    
    void MarkNodeDirty(RecordNode *node);
    
    void MarkRunningNodes();
    void UnmarkRunningNodes();
    
    void ClearDirtyNode(RecordNode *node, TimeSliceStore *tss);
    
    TimeSliceStore *MergeTimeSlice(map<NodeID, RecordNodeBasic> &integrated, int from, int to);
    const vector<ProfileValue> *GetStartProfileValue(int from, int to);
    void CheckAllTimeSlice();

    RecordNode* AddCurrentNode(NodeID cnid, NodeID parent_cnid, NameID name_id);

};



class Lockable
{
    pthread_mutex_t mtx_;
public:
    Lockable();
    virtual ~Lockable(){}
    void Lock();
    void Unlock();
};

class Locker
{
    Lockable *target_;
public:
    Locker(Lockable *target = NULL)
    {
        target_ = NULL;
        Lock(target);
    }
    
    ~Locker()
    {
        Unlock();
    }
    
    void Lock(Lockable *target)
    {
        if(target_)
            target_->Unlock();
        target_ = target;
        if(target_)
            target_->Lock();
    }
    
    void Unlock()
    {
        if(target_)
        {
            target_->Unlock();
            target_ = NULL;
        }
    }
};


class DataStore: public Lockable
{
protected:
    bool recv_, first_received_;
    int socket_;
    int id_;
    int current_time_number_;

    pthread_t conn_thread_;
    bool closed_;
    string host_, port_;
    string target_name_;
    
    map<string, int> member_offset_info_;
    vector<RecordMetadata> record_metadata_;
    int num_records_;

    map<NameID, string> name_table_;
    map<ThreadID, ThreadStore *> threads_;


public:
    DataStore(int id);
    virtual ~DataStore();
    
    typedef map<ThreadID, ThreadStore *>::iterator thread_store_iterator;
    
    thread_store_iterator thread_store_begin()
    {
        return threads_.begin();
    }

    thread_store_iterator thread_store_end()
    {
        return threads_.end();
    }
    
    const RecordMetadata &GetRecordMetadata(int idx) const {return record_metadata_[idx];}
    
    vector<RecordMetadata> &GetRecordMetadataVector(){return record_metadata_;}
    
    int GetCurrentTimeNumber(){return current_time_number_;}
    
    int GetNumProfileValues()
    {
        assert(num_records_ != 0);
        return num_records_;
    }
    
    bool HasNameID(NameID id){ return name_table_.find(id) != name_table_.end(); }
    string GetNameFromID(NameID id)
    {
         map<NameID, string>::iterator iter = name_table_.find(id);
         if(iter != name_table_.end())
            return (*iter).second;
        return "<Unknown>";
    }
    
    void SetNameID(NameID id, const string &name)
    {
        name_table_[id] = name;
    }
    
    int GetMemberOffsetOf(const string &name);
    
    int GetID(){return id_;}
    
    string GetTargetName(){return target_name_;}
    string GetTargetHost(){return host_;}
    void SetConnectedSocket(int sock);
    void SetConnectTo(string host, string port);
    void StartReceive();

    void Close();
    bool ReceiveLoop();
    
    void SendMessage(unsigned int msg_type, void *msg = NULL, int msg_size = 0);
    intrusive_ptr<MessageBuf> RecvMessage();

    // SendRequest = SendMessaage + RecvMessage
    intrusive_ptr<MessageBuf> SendRequest(unsigned int msg_type, void *msg = NULL, int msg_size = 0);
    
    intrusive_ptr<MessageBuf> SendQueryInfoRequest(int request_val);
    
    
    bool GetProfileData();

    void DumpText(ostream &strm);
    void DumpText()
    {
        cerr << "[DataStore Dump]" << endl;
        DumpText(cerr);
        cerr << endl;
    }
    
    void DumpJSON(stringstream &strm, int flag, int p1, int p2);

    void Lock();
    void Unlock();
    
    void CheckAllTimeSlice();
    NodeID TouchGNID(ThreadStore *ts, RecordNode *node);
    
};


class GlobalThreadStore: public ThreadStore
{
    NodeID seq_num_;
    map<NodeID, NodeID> gnid_to_seq_;
public:
    GlobalThreadStore(DataStore *ds): ThreadStore(ds){seq_num_ = 1;}
    
    NodeID GNIDToSeq(NodeID gnid);
    
    void Integrate();
};


class GlobalDataStore: public DataStore
{
public:
    GlobalDataStore(int id);
    void Integrate();
};

const int DF_CURRENT_TREE = 1;
const int DF_DIFF_TREE = 2;  // Get differencial Tree p1 to p2

void AddGNID(NodeID gnid, const string &name);

void InitDataStore();
bool DataStoreRequest(stringstream &out, string &mime, const vector<string> &path,
        const map<string,string> &data);

inline void intrusive_ptr_add_ref(ReferenceCounted *obj)
{
    obj->AddRef();
}
inline void intrusive_ptr_release(ReferenceCounted *obj)
{
    obj->Release();
}

}
#endif // LLPROF_DATA_STORE_H
