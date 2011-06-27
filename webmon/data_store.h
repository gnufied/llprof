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

namespace llprof{

using namespace std;

class MessageBuf
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

typedef unsigned long long NodeID;
typedef unsigned long long NameID;
typedef unsigned long long ThreadID;


class DataStore;

class RecordNode
{
    DataStore *ds_;
    NodeID id_;
    NameID node_name_;
public:
    RecordNode(DataStore *ds): ds_(ds)
    {
    }
    
};

class ThreadStore
{
    DataStore *ds_;
    map<NodeID, RecordNode> current_tree_;
public:
    ThreadStore(DataStore *ds): ds_(ds){}
    void Store(MessageBuf *buf);
};

class DataStore
{
    int socket_;
    int id_;
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
    auto_ptr<MessageBuf> RecvMessage();

    // SendRequest = SendMessaage + RecvMessage
    auto_ptr<MessageBuf> SendRequest(unsigned int msg_type, void *msg = NULL, int msg_size = 0);
    
    auto_ptr<MessageBuf> SendQueryInfoRequest(int request_val);
    
    
    void GetProfileData();

};

void InitDataStore();
bool DataStoreRequest(stringstream &out, const vector<string> &path,
        const map<string,string> &data);


}
#endif // LLPROF_DATA_STORE_H
