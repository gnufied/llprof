#ifndef LLPROF_DATA_STORE_H
#define LLPROF_DATA_STORE_H

#include <pthread.h>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <memory>
#include <assert.h>
#include "boost_wrap.h"

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

class RecordNode
{
    DataStore *ds_;
    NodeID id_, parent_id_;
    NameID node_name_;
    vector<ProfileValue> values_;
    
public:
    RecordNode();
    void SetDataStore(DataStore *ds);
    NodeID GetNodeID() const {return id_;}
    void SetNodeID(NodeID id){ id_ = id; }
    
    NodeID GetParentNodeID() const {return parent_id_; }
    void SetParentNodeID(NodeID id){ parent_id_ = id; }
    
    NameID GetNameID() const { return node_name_; }
    void SetNameID(NameID id){ node_name_ = id; }
    
    ProfileValue GetProfileValue(int idx)
    {
        return values_[idx];
    }
    void SetProfileValue(int idx, ProfileValue value)
    {
        values_[idx] = value;
    }
    ProfileValue AddProfileValue(int idx, ProfileValue adding_value)
    {
        return (values_[idx] += adding_value);
    }
    vector<ProfileValue> &GetValuesVector() {return values_;}
    const vector<ProfileValue> &GetValuesVector() const {return values_;}
    void Accumulate(const vector<ProfileValue> &values);
    
    DataStore *GetDataStore() const {return ds_;}
};

void write_json(stringstream &strm, const RecordNode &node);

class TimeSliceStore
{
    ThreadStore *ts_;
    int time_number_;
    map< NodeID, vector<ProfileValue> > tree_;
public:
    void SetThreadStore(ThreadStore *ts);
    void SetTimeNumber(int tn);
    ProfileValue GetProfileValue(NodeID nid, int idx);
    void SetProfileValue(NodeID nid, int idx, ProfileValue value);    
    void DumpText(ostream &strm);
    
    typedef map< NodeID, vector<ProfileValue> >::iterator map_iterator;
    
    
    map_iterator nodes_begin(){return tree_.begin();}
    map_iterator nodes_end(){return tree_.end();}
};

class ThreadStore
{
    DataStore *ds_;
    map<NodeID, RecordNode> current_tree_;
    
    int start_time_number_;
    vector<TimeSliceStore> time_slice_;
    ThreadID thread_id_;
public:
    ThreadStore(DataStore *ds): ds_(ds){}
    
    void SetThreadID(ThreadID id){thread_id_ = id;}
    ThreadID GetThreadID(){return thread_id_;}
    
    void Store(intrusive_ptr<MessageBuf> buf);
    
    TimeSliceStore *GetTimeSlice(int ts);
    
    void DumpText(ostream &strm);
    
    DataStore *GetDataStore(){return ds_;}
    void DumpJSON(stringstream &strm, int flag, int p1, int p2);
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
    int socket_;
    int id_;
    int current_time_number_;

    pthread_t conn_thread_;
    bool closed_;
    string host_, port_;
    string target_name_;
    
    map<string, int> member_offset_info_;
    map<int, RecordMetadata> record_metadata_;
    int num_records_;

    map<NameID, string> name_table_;
    map<ThreadID, ThreadStore> threads_;


public:
    DataStore(int id);
    ~DataStore();
    
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
    
    int GetMemberOffsetOf(const string &name);
    
    int GetID(){return id_;}
    
    string GetTargetName(){return target_name_;}
    string GetTargetHost(){return host_;}
    void SetConnectedSocket(int sock);
    void SetConnectTo(string host, string port);
    void StartThread();
    void Close();
    void *ThreadMain();
    
    void SendMessage(unsigned int msg_type, void *msg = NULL, int msg_size = 0);
    intrusive_ptr<MessageBuf> RecvMessage();

    // SendRequest = SendMessaage + RecvMessage
    intrusive_ptr<MessageBuf> SendRequest(unsigned int msg_type, void *msg = NULL, int msg_size = 0);
    
    intrusive_ptr<MessageBuf> SendQueryInfoRequest(int request_val);
    
    
    bool GetProfileData();

    void DumpText(ostream &strm);
    
    void DumpJSON(stringstream &strm, int flag, int p1, int p2);

    void Lock();
    void Unlock();
};

const int DF_CURRENT_TREE = 1;
const int DF_DIFF_TREE = 2;  // Get differencial Tree p1 to p2

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
