
#include <iostream>
#include <memory>

#include "boost_wrap.h"
#include "data_store.h"
#include "../llprofcommon/network.h"
#include "../llprofcommon/llprof_const.h"
#include "http.h"

namespace llprof{

DataStore::DataStore(int id)
{
    id_ = id;
    socket_ = 0;
    closed_ = false;
}

DataStore::~DataStore()
{
    Close();
}

void DataStore::SetConnectedSocket(int sock)
{
    socket_ = sock;
}

void DataStore::SetConnectTo(string host, string port)
{
    host_ = host;
    port_ = port;
}

void* DataStore_thread_main(void *data)
{
    return ((DataStore*)data)->ThreadMain();
}

void DataStore::StartThread()
{
    pthread_create(&conn_thread_, NULL, DataStore_thread_main, this);
    pthread_detach(conn_thread_);
}

void DataStore::Close()
{
    if(!closed_)
    {
        closed_ = true;
        if(socket_)
            close(socket_);
    }
}


void DataStore::SendMessage(unsigned int msg_type, void *msg, int msg_size)
{
    SendProfMessage(socket_, msg_type, msg, msg_size);
}

auto_ptr<MessageBuf> DataStore::SendRequest(unsigned int msg_type, void *msg, int msg_size)
{
    SendMessage(msg_type, msg, msg_size);
    return RecvMessage();
}

auto_ptr<MessageBuf> DataStore::RecvMessage()
{
    auto_ptr<MessageBuf> buf(new MessageBuf());
    buf->Resize(8);
    buf->ReadFrom(socket_, 0, 8);
    if(buf->GetMessageSize() == 0)
        return buf;
    buf->FitSize();
    buf->ReadFrom(socket_, 8, buf->GetMessageSize());
    return buf;
}

int MessageBuf::ReadFrom(int sock, int offset, int sz)
{
    return RecvFull(sock, (buf_ + offset), sz);
}


auto_ptr<MessageBuf> DataStore::SendQueryInfoRequest(int request_val)
{
    return SendRequest(MSG_QUERY_INFO, &request_val, 4);
}




void* DataStore::ThreadMain()
{
    if(socket_ == 0)
    {
        socket_ = SocketConnectTo(host_, port_);
        if(socket_ <= 0)
        {
            socket_ = 0;
            closed_ = true;
            cout << "Failed to connect:" << host_ << ":" << port_ << endl;
            return NULL;
        }
    }

    
    auto_ptr<MessageBuf> res;

    // オフセット値の取得
    {
        res = SendQueryInfoRequest(INFO_DATA_SLIDE);
        stringstream strm(res->AsString());
        
        while(ws(strm), !strm.eof())
        {
            pair<string, int> entry;
            strm >> entry.first >> entry.second;
            member_offset_info_.insert(entry);
        }
        cout << "Offset Info: " << endl << member_offset_info_ << endl;
    }

    // レコードメタデータの取得
    {
        record_metadata_.clear();
        res = SendQueryInfoRequest(INFO_RECORD_METAINFO);
        stringstream strm(res->AsString());
        strm >> num_records_;
        while(ws(strm), !strm.eof())
        {
            pair<int, RecordMetadata> entry;
            strm
                >> entry.first
                >> entry.second.FieldName
                >> entry.second.Unit
                >> entry.second.Flags
                ;
            record_metadata_.insert(entry);
        }
        cout << "Record Metadata: " << endl << record_metadata_ << endl;
    }

    // プロファイルターゲット名の取得
    {
        record_metadata_.clear();
        res = SendQueryInfoRequest(INFO_PROFILE_TARGET);
        target_name_ = res->AsString();
        cout << "Profile Target: " << target_name_ << endl;
    }
    
    GetProfileData();
    
    

    return NULL;
}

void DataStore::GetProfileData()
{
    auto_ptr<MessageBuf> res;
    cout << "Send MSG_REQ_PROFILE_DATA" << endl;
    SendMessage(MSG_REQ_PROFILE_DATA);
    while(true)
    {
        cout << "Receiving..." << endl;
        res = RecvMessage();
        cout << "Received: " << res->GetMessageType() << " Size: " << res->GetMessageSize() << endl;
        if(!res.get())
        {
            cout << "Recv Error" << endl;
            return;
        }
        switch(res->GetMessageType())
        {
        case MSG_COMMAND_SUCCEED:
            cout << "Command Succeed" << endl;
            return;

        case MSG_ERROR:
            cout << "Command Error" << endl;
            return;
            
        case MSG_PROFILE_DATA:{
            uint64_t thread_id = res->Get<uint64_t>();
            
            map<ThreadID, ThreadStore>::iterator iter = threads_.find(thread_id);
            if(iter == threads_.end())
            {
                threads_.insert(make_pair(thread_id, ThreadStore(this)));
                iter = threads_.find(thread_id);
            }
            
            (*iter).second.Store(res.get());
            
            
            break;
        }
        }
    }
    
}

int DataStore::GetMemberOffsetOf(const string &name)
{
    return member_offset_info_[name];
}

void ThreadStore::Store(MessageBuf *buf)
{
    buf->SetCurrentPtr(8);
    
    int offset_call_node_id = ds_->GetMemberOffsetOf("pdata.call_node_id");
    int offset_nameid = ds_->GetMemberOffsetOf("pdata.nameid");
    while(!buf->IsEOF())
    {
        char *p = buf->GetCurrentBufPtr();
        
        unsigned int cnid = *(unsigned int *)(p + offset_call_node_id);
        NameID nameid = *(NameID *)(p + offset_nameid);
        cout << "[" << cnid << "]  Name:" << nameid << endl;        
        
        buf->Seek(ds_->GetMemberOffsetOf("pdata.record_size"));
    }
    cout << "End ThreadStore" << endl;
}

map<int, DataStore*> *gAllDataStore;
int gDataStoreIDSeq;
void InitDataStore()
{
    gAllDataStore = new map<int, DataStore*>();
    gDataStoreIDSeq = 0;
}

bool DataStoreRequest(std::stringstream &out, const std::vector<std::string> &path,
        const std::map<std::string,std::string> &data)
{
    if(path.size() > 0 && path[0] == "new")
    {
        vector<string> connect_to;
        string hostval = getm(data, "host", "");
        algorithm::split(connect_to, hostval, is_any_of(":"));
        if(connect_to.size() == 1)
            connect_to.push_back("12321");
        
        gDataStoreIDSeq++;
        DataStore *ds = new DataStore(gDataStoreIDSeq);



        (*gAllDataStore)[gDataStoreIDSeq] = ds;
        ds->SetConnectTo(connect_to[0], connect_to[1]);
        ds->StartThread();
        write_json(out, "OK");
        return true;
    }
    else if(path.size() > 0 && path[0] == "list")
    {
        JsonList lw(out);
        for(map<int,DataStore*>::iterator iter = gAllDataStore->begin(); iter != gAllDataStore->end(); iter++)
        {
            stringstream substrm;
            JsonDict d(substrm);
            d.add("id", (*iter).second->GetID());
            d.add("name", (*iter).second->GetTargetName());
            d.add("host", (*iter).second->GetTargetHost());
            d.close();
            lw.add<stringstream&>(substrm);
        }
        return true;
    }

    return false;
}


} // namespace llprof
