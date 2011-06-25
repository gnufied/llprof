#ifndef LLPROF_DATA_STORE_H
#define LLPROF_DATA_STORE_H

#include <pthread.h>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>

class MessageBuf
{
    char *buf_;
    int sz_;
    
public:
    MessageBuf()
    {
        buf_ = NULL;
        sz_ = 0;
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
            std::memcpy(newbuf, buf_, sz_);
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
    
    std::string AsString()
    {
        return std::string(buf_+8, sz_-8);
    }
};


class DataStore
{
    int socket_;
    int id_;
    pthread_t conn_thread_;
    bool closed_;
    std::string host_, port_;
    std::string target_name_;

public:
    DataStore(int id);
    ~DataStore();
    
    int GetID(){return id_;}
    
    std::string GetTargetName(){return target_name_;}
    std::string GetTargetHost(){return host_;}
    void SetConnectedSocket(int sock);
    void SetConnectTo(std::string host, std::string port);
    void StartThread();
    void Close();
    void ThreadMain();
    
    MessageBuf *SendRequest(unsigned int msg_type, void *msg, int msg_size);
    
    
};

void InitDataStore();
bool DataStoreRequest(std::stringstream &out, const std::vector<std::string> &path,
        const std::map<std::string,std::string> &data);

#endif // LLPROF_DATA_STORE_H
