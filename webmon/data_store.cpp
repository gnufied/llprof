
#include <iostream>
#include <memory>
#include <algorithm>
#include <boost/crc.hpp>
#include "boost_wrap.h"
#include "data_store.h"
#include "../llprofcommon/network.h"
#include "../llprofcommon/measurement.h"
#include "../llprofcommon/llprof_const.h"
#include "http.h"


namespace llprof{



void *DataStoreAcceptThreadMain(void *);
map<int, DataStore*> *gAllDataStore;
int gDataStoreIDSeq;
pthread_t g_datastore_server_thread;
GlobalDataStore *gGlobalDataStore;

bool IsSingleTreeMode()
{
    if(gGlobalDataStore && gGlobalDataStore->IsSingleTreeMode())
        return true;
    return false;
}


void RecordNodeBasic::SetDataStore(DataStore *ds)
{
    ds_ = ds;
    if(!IsSingleTreeMode())
    {
        all_values_.resize(ds_->GetNumProfileValues());
        children_values_.resize(ds_->GetNumProfileValues());
    }
    UpdateDataStore();
}
    
void TimeSliceStore::SetThreadStore(ThreadStore *ts)
{
    ts_ = ts;
}

void TimeSliceStore::SetTimeNumber(int tn)
{
    time_number_ = tn;
}

void TimeSliceStore::DumpText(ostream &strm)
{
    strm << "<TimeSlice [" << time_number_ << "]>" << endl;
    
    for(map< NodeID, RecordNodeBasic >::iterator it = tree_.begin(); it != tree_.end(); it++)
    {
        strm << "  Node " << (*it).first << " ";
        strm << (*it).second;
        strm << endl;
    }
}


DataStore::DataStore(int id)
{
    id_ = id;
    socket_ = 0;
    closed_ = false;
    current_time_number_ = 0;
    recv_ = false;
    first_received_ = false;
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

void* DataStore_ReceiveThread(void *data)
{
    int interval = 10;
    if(getenv("WEBMON_INTERVAL"))
    {
        interval = atoi(getenv("WEBMON_INTERVAL"));
        if(interval < 1)
            interval = 1;
    } 
    
    
    while(true)
    {
        sleep(interval);
        time_val_t start_tv = get_time_now_nsec();
        for(map<int,DataStore*>::iterator iter = gAllDataStore->begin(); iter != gAllDataStore->end(); iter++)
        {
            (*iter).second->ReceiveLoop();

        }
        if(gGlobalDataStore)
            gGlobalDataStore->Integrate();
        
        cout << "Fetch Time: " << (double(get_time_now_nsec() - start_tv) / 1000.0 / 1000.0 / 1000.0 ) << endl;
    }
    return NULL;
}

void DataStore::StartReceive()
{
    recv_ = true;
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


Lockable::Lockable()
{
    pthread_mutex_init(&mtx_, NULL);
}

void Lockable::Lock()
{
    pthread_mutex_lock(&mtx_);
}

void Lockable::Unlock()
{
    pthread_mutex_unlock(&mtx_);
}


void DataStore::SendMessage(unsigned int msg_type, void *msg, int msg_size)
{
    SendProfMessage(socket_, msg_type, msg, msg_size);
}

intrusive_ptr<MessageBuf> DataStore::SendRequest(unsigned int msg_type, void *msg, int msg_size)
{
    SendMessage(msg_type, msg, msg_size);
    return RecvMessage();
}

intrusive_ptr<MessageBuf> DataStore::RecvMessage()
{
    intrusive_ptr<MessageBuf> buf(new MessageBuf());
    buf->Resize(8);
    if(buf->ReadFrom(socket_, 0, 8) != 0)
        return intrusive_ptr<MessageBuf>();
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


intrusive_ptr<MessageBuf> DataStore::SendQueryInfoRequest(int request_val)
{
    return SendRequest(MSG_QUERY_INFO, &request_val, 4);
}




bool DataStore::ReceiveLoop()
{
    if(!recv_)
        return false;

    if(!first_received_)
    {
        first_received_ = true;
        if(socket_ == 0)
        {
            socket_ = SocketConnectTo(host_, port_);
            if(socket_ <= 0)
            {
                socket_ = 0;
                closed_ = true;
                cout << "Failed to connect:" << host_ << ":" << port_ << endl;
                recv_ = false;
                return false;
            }
        }
        cout << "DataStore Started." << endl;
        
        intrusive_ptr<MessageBuf> res;

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
        }

        // レコードメタデータの取得
        {
            record_metadata_.clear();
            res = SendQueryInfoRequest(INFO_RECORD_METAINFO);
            stringstream strm(res->AsString());
            strm >> num_records_;
            record_metadata_.resize(num_records_);
            cout << "Num Metadata:" << num_records_ << endl;
            while(ws(strm), !strm.eof())
            {
                int idx = 0;
                strm >> idx;
                RecordMetadata &md = record_metadata_[idx];
                strm
                    >> md.FieldName
                    >> md.Unit
                    >> md.Flags
                    ;
                md.UpdateFlag();
            }
            cout << "V:" << record_metadata_ << endl;
        }

        // プロファイルターゲット名の取得
        {
            res = SendQueryInfoRequest(INFO_PROFILE_TARGET);
            target_name_ = res->AsString();
        }

        int interval = 10;
        if(getenv("WEBMON_INTERVAL"))
        {
            interval = atoi(getenv("WEBMON_INTERVAL"));
            if(interval < 1)
                interval = 1;
        }
    }


    if(!GetProfileData())
    {
        recv_ = false;
        return false;
    }

    

    return true;
}

bool DataStore::GetProfileData()
{
    Locker lk(this);
    current_time_number_++;

    intrusive_ptr<MessageBuf> res;
    map< ThreadID, intrusive_ptr<MessageBuf> > buf_for_threads, nowinfo_buf_for_threads;
    vector<NameID> query_names;
    
    int offset_nameid = GetMemberOffsetOf("pdata.nameid");
    int pdata_record_size = GetMemberOffsetOf("pdata.record_size");

    SendMessage(MSG_REQ_PROFILE_DATA);
    while(true)
    {
        res = RecvMessage();
        if(!res.get())
        {
            return false;
        }
        switch(res->GetMessageType())
        {
        case MSG_COMMAND_SUCCEED:
            goto end_of_loop;

        case MSG_ERROR:
            return false;
            
        case MSG_PROFILE_DATA:{
            ThreadID thread_id = res->Get<uint64_t>();
            res->SetCurrentPtr(8);
            while(!res->IsEOF())
            {
                char *p = res->GetCurrentBufPtr();
                NameID nameid = *(NameID *)(p + offset_nameid);
                if(!HasNameID(nameid))
                {
                    if(query_names.size() == 0)
                        query_names.push_back(0);
                    query_names.push_back(nameid);
                    SetNameID(nameid, "<Asking>");
                }
                
                res->Seek(pdata_record_size);
            }
            buf_for_threads[thread_id] = res;
            break;
        }

        case MSG_NOWINFO:{
            ThreadID thread_id = res->Get<uint64_t>();
            nowinfo_buf_for_threads[thread_id] = res;
            break;
        }
        }
    }
end_of_loop:

    if(!query_names.empty())
    {
        ((unsigned int *)&query_names[0])[0] = QUERY_NAMES;
        ((unsigned int *)&query_names[0])[1] = query_names.size() - 1;
        intrusive_ptr<MessageBuf> names = SendRequest(MSG_QUERY_NAMES, &query_names[0], query_names.size() * sizeof(query_names[0]));
        stringstream names_strm(names->AsString());
        for(size_t i = 0; i < query_names.size() - 1; i++)
        {
            string s;
            getline(names_strm, s);
            SetNameID(query_names[i+1], s);

        }
    }

    for(map< ThreadID, intrusive_ptr<MessageBuf> >::iterator iter = buf_for_threads.begin();
        iter != buf_for_threads.end(); iter++)
    {

        map<ThreadID, ThreadStore *>::iterator tpos = threads_.find((*iter).first);
        if(tpos == threads_.end())
        {
            threads_.insert(make_pair((*iter).first, new ThreadStore(this)));
            tpos = threads_.find((*iter).first);
            (*tpos).second->SetThreadID((*iter).first);
        }
        ThreadStore *ts = (*tpos).second;
        // Store profile data
        intrusive_ptr<MessageBuf> nibuf = nowinfo_buf_for_threads[(*iter).first];
        ts->Store((*iter).second, nibuf);


    }
    return true;
}


ThreadStore::ThreadStore(DataStore *ds): ds_(ds)
{
    running_node_ = 0;
    
    RecordNode *node = current_tree_[1];
    node.SetDataStore(ds_);
    node.SetNodeID(1);
    node.SetParentNodeID(0);
    node.SetNameID(0);
    node.SetGNID(1);


    RecordNode *node = AddCurrentNode(1, 0, 0);
    node->SetGNID(1);

}

void ThreadStore::UpdateNowValues(NodeID running_node, void *pdata)
{
    running_node_ = running_node;
    if(now_values_.empty())
        now_values_.resize(ds_->GetNumProfileValues());
    memcpy(&now_values_[0], pdata, ds_->GetNumProfileValues() * sizeof(ProfileValue));
}

int DataStore::GetMemberOffsetOf(const string &name)
{
    return member_offset_info_[name];
}

void DataStore::DumpText(ostream &strm)
{
    strm << "<Object: DataStore " << (void *)this << ">" << endl;
    strm << "ID: " << id_ << endl;
    strm << "Socket: " << socket_ << endl;
    strm << "current_time_number: " << current_time_number_ << endl;
    strm << "Closed: " << closed_ << endl;
    strm << "ConnectTo: " << host_ << ":" << port_ << endl;
    strm << "TargetName: " << target_name_ << endl;
    strm << "num_records: " << num_records_ << endl;
    strm << "[member_offset_info]" << endl << member_offset_info_ << endl;
    strm << "[record_metadata] " << endl << record_metadata_ << endl;
    strm << "[NameTable]" << endl << name_table_ << endl << endl;

    for(map<ThreadID, ThreadStore *>::iterator it = threads_.begin(); it != threads_.end(); it++)
    {
        (*it).second->DumpText(strm);
    }
    strm << "<End>" << endl;
}

void DataStore::DumpJSON(stringstream &strm, int flag, int p1, int p2)
{
    JsonDict data(strm);
    data.add("timenum", GetCurrentTimeNumber());
    JsonList threads;
    for(map<ThreadID, ThreadStore *>::iterator it = threads_.begin(); it != threads_.end(); it++)
    {
        stringstream item;
        (*it).second->DumpJSON(item, flag, p1, p2);
        threads.add<stringstream &>(item);
    }
    data.add<stringstream &>("threads", threads.strm());

    JsonDict record_metadata_dict;
    for(size_t i = 0; i < record_metadata_.size(); i++)
    {
        JsonDict dmd;
        dmd.add("field_name", record_metadata_[i].FieldName);
        dmd.add("unit", record_metadata_[i].Unit);
        dmd.add("flags", record_metadata_[i].Flags);

        record_metadata_dict.add<stringstream &>(lexical_cast<string>(i), dmd.strm());
    }
    data.add<int>("numrecords", record_metadata_.size());
    data.add<stringstream &>("metadata", record_metadata_dict.strm());
}


void ProfileValuesToStream(stringstream &out, const vector<ProfileValue> &vec)
{
    JsonList list(out);
    for(vector<ProfileValue>::const_iterator it = vec.begin(); it != vec.end(); it++)
    {
        list.add(*it);
    }
}

void write_json(stringstream &strm, const RecordNodeBasic &node)
{
    JsonDict out(strm);
    out.add<unsigned int>("id", node.GetNodeID());
    out.add<unsigned int>("pid", node.GetParentNodeID());
    out.add<string>("name", node.GetDataStore()->GetNameFromID(node.GetNameID()));
    
    stringstream s;
    ProfileValuesToStream(s, node.GetAllValues());
    out.add<stringstream &>("all", s);

    s.str("");
    ProfileValuesToStream(s, node.GetChildrenValues());
    out.add<stringstream &>("cld", s);
    

}


const vector<ProfileValue> *ThreadStore::GetStartProfileValue(int from, int to)
{
    TimeSliceStore *tss = NULL;
    for(int t = from-1; t <= to; t++)
    {
        tss = GetTimeSlice(t);
        if(tss)
            break;
    }
    if(!tss)
        return NULL;
    return &tss->GetNowValues();
}

TimeSliceStore *ThreadStore::MergeTimeSlice(map<NodeID, RecordNodeBasic> &integrated, int from, int to)
{
    TimeSliceStore *last_tss = NULL;
    int nslice = 0;

    for(int t = from; t <= to; t++)
    {
        TimeSliceStore *tss = GetTimeSlice(t);
        if(!tss)
            continue;
        last_tss = tss;
        nslice++;
        for(TimeSliceStore::map_iterator it = tss->nodes_begin(); it != tss->nodes_end(); it++)
        {
            map<NodeID, RecordNodeBasic>::iterator pos = integrated.find((*it).first);
            if(pos == integrated.end())
            {
                RecordNodeBasic &node = integrated[(*it).first];
                node.SetDataStore(ds_);
                node.SetNodeID((*it).first);
                node.SetParentNodeID(current_tree_[(*it).first].GetParentNodeID());
                node.SetNameID(current_tree_[(*it).first].GetNameID());
                pos = integrated.find((*it).first);
            }
            if((*it).first == 5)
                (*pos).second.GetAllValues()[0] += 200;
            (*pos).second.Accumulate((*it).second);
        }
    } 

    return last_tss;
}

bool RecordNodeBasic::ValuesEquals(const RecordNodeBasic &rhs)
{
    if(all_values_ == rhs.all_values_ && children_values_ == rhs.children_values_)
    {
        return true;
    }
    return false;
}

void ThreadStore::CheckAllTimeSlice()
{
    cout << "[Slice error check]" << endl;
    map<NodeID, RecordNodeBasic> integrated;
    MergeTimeSlice(integrated, 0, ds_->GetCurrentTimeNumber());
    
    cout << "  Integrated size: " << integrated.size() << endl;
    cout << "  Current Tree: " << current_tree_.size() << endl;
    
    for(map<NodeID, RecordNode>::iterator it = current_tree_.begin(); it != current_tree_.end(); it++)
    {
        NodeID nid = (*it).first;
        RecordNode &current_node = (*it).second;
        map<NodeID, RecordNodeBasic>::iterator ii = integrated.find(nid);
        if(!current_node.ValuesEquals((*ii).second))
        {
            cout << "Error: " << endl;
            cout << "  Current:    " << current_node << endl;
            cout << "  Integrated: " << (*ii).second << endl;
        }
    }
    cout << "Check Done." << endl;
}

void ThreadStore::DumpJSON(stringstream &strm, int flag, int p1, int p2)
{
    JsonDict store(strm);
    store.add("id", GetThreadID());
    if((flag & DF_CURRENT_TREE) == DF_CURRENT_TREE)
    {
        JsonList nodes;
        for(map<NodeID, RecordNode>::iterator it = current_tree_.begin(); it != current_tree_.end(); it++)
            nodes.add<RecordNode &>((*it).second);
        store.add<stringstream &>("nodes", nodes.strm());
        
        stringstream tmp;
        ProfileValuesToStream(tmp, now_values_);
        store.add<stringstream &>("now_values", tmp);
        store.add<unsigned int>("running_node", running_node_);
    }
    else if((flag & DF_DIFF_TREE) == DF_DIFF_TREE)
    {
        JsonList nodes;
        if(p2 == 0)
            p2 = ds_->GetCurrentTimeNumber();
        
        if(p1 - p2 > 10)
        {
            cout << "Too many diff: " << p1 << " - " << p2 << endl;
        }
        map<NodeID, RecordNodeBasic> integrated;
        TimeSliceStore *last_tss = MergeTimeSlice(integrated, p1, p2);
        const vector<ProfileValue> *start_values = GetStartProfileValue(p1,p2);
        
        
        for(map<NodeID, RecordNodeBasic>::iterator it = integrated.begin(); it != integrated.end(); it++)
            nodes.add<RecordNodeBasic &>((*it).second);
        
        store.add<stringstream &>("nodes", nodes.strm());
        if(last_tss)
        {
            stringstream tmp;
            ProfileValuesToStream(tmp, last_tss->GetNowValues());
            store.add<stringstream &>("now_values", tmp);
            if(start_values)
            {
                tmp.str("");
                ProfileValuesToStream(tmp, *start_values);
                store.add<stringstream &>("start_values", tmp);
            }

            store.add<unsigned int>("running_node", last_tss->GetRunningNode());
        }
    }
}


void ThreadStore::DumpText(ostream &strm)
{
    strm << "<ThreadStore>" << endl;
    for(map<NodeID, RecordNode>::iterator it = current_tree_.begin(); it != current_tree_.end(); it++)
    {
        RecordNode &n = (*it).second;
        strm << "  Node:" << n << endl;
        strm << endl;
    }
    
    
    for(size_t i = 0; i < time_slice_.size(); i++)
    {
        strm << endl;
        time_slice_[i].DumpText(strm);
    }
}

void ThreadStore::UnmarkRunningNodes()
{
    RecordNode *node = GetNodeFromID(running_node_);
    while(node)
    {
        node->SetRunning(false);
        node = GetNodeFromID(node->GetParentNodeID());
    }
}

void ThreadStore::MarkRunningNodes()
{
    RecordNode *node = GetNodeFromID(running_node_);
    MarkNodeDirty(node);
    while(node)
    {
        node->SetRunning(true);
        node = GetNodeFromID(node->GetParentNodeID());
    }
}

RecordNode *ThreadStore::AddCurrentNode(NodeID cnid, NodeID parent_cnid, NameID nameid)
{
    RecordNode &node = current_tree_[cnid];
    node.SetDataStore(ds_);
    node.SetNodeID(cnid);
    node.SetParentNodeID(parent_cnid);
    node.SetNameID(nameid);
    return &node;
}

void ThreadStore::Store(intrusive_ptr<MessageBuf> buf, intrusive_ptr<MessageBuf> nibuf)
{

    UnmarkRunningNodes();

    // Store Now info
    nibuf->SetCurrentPtr(8);
    unsigned running_node = nibuf->Get<unsigned int>();
    
    nibuf->SetCurrentPtr(16);
    assert((size_t)nibuf->GetRemainSize() >= ds_->GetNumProfileValues() * sizeof(ProfileValue));
    UpdateNowValues(running_node, nibuf->GetCurrentBufPtr());

    MarkRunningNodes();

    int offset_call_node_id = ds_->GetMemberOffsetOf("pdata.call_node_id");
    int offset_parent_node_id = ds_->GetMemberOffsetOf("pdata.parent_node_id");
    int offset_nameid = ds_->GetMemberOffsetOf("pdata.nameid");
    int pdata_record_size = ds_->GetMemberOffsetOf("pdata.record_size");
    int offset_value = ds_->GetMemberOffsetOf("pdata.value");

    buf->SetCurrentPtr(8);
    vector<ProfileValue> values(ds_->GetNumProfileValues());

    while(!buf->IsEOF())
    {
        char *p = buf->GetCurrentBufPtr();
        
        unsigned int cnid = *(unsigned int *)(p + offset_call_node_id);
        unsigned int parent_cnid = *(unsigned int *)(p + offset_parent_node_id);
        NameID nameid = *(NameID *)(p + offset_nameid);
        

        
        map<NodeID, RecordNode>::iterator iter = current_tree_.find(cnid);
        if(iter == current_tree_.end())
        {
            RecordNode *node = AddCurrentNode(cnid, parent_cnid, nameid);
            ds_->TouchGNID(this, node);
            if(RecordNode *parent = GetNodeFromID(parent_cnid))
                parent->AddChildID(cnid);
            
            iter = current_tree_.find(cnid);
        }
        RecordNode &node = (*iter).second;

        for(int i = 0; i < ds_->GetNumProfileValues(); i++)
        {
            values[i] = ((ProfileValue *)(p + offset_value))[i];
        }
        MarkNodeDirty(&node);
        node.SetTempValues(values);
        buf->Seek(pdata_record_size);
    }
    
    TimeSliceStore *tss = GetCurrentTimeSlice();
    assert(tss);
    tss->SetNowValues(running_node_, now_values_);
    ClearDirtyNode(GetRootNode(), tss);
}

TimeSliceStore *ThreadStore::GetCurrentTimeSlice()
{
    return GetTimeSlice(ds_->GetCurrentTimeNumber());
}

void ThreadStore::MarkNodeDirty(RecordNode *node)
{
    if(!node)
        return;
    if(!node->IsDirty())
    {
        node->SetDirty(true);
        node->ClearTempValues();
        // StaticValueはデフォルトで前回と同一値をとる
        for(int i = 0; i < ds_->GetNumProfileValues(); i++)
            if(ds_->GetRecordMetadata(i).StaticValueFlag)
                node->GetTempValues()[i] = node->GetAllValues()[i];
        
        MarkNodeDirty(GetNodeFromID(node->GetParentNodeID()));
    }
}

void ThreadStore::ClearDirtyNode(RecordNode *node, TimeSliceStore *tss)
{
    if(!node || !node->IsDirty())
        return;

    RecordNodeBasic *tss_node = tss->GetNodeFromID(node->GetNodeID());
    assert(tss_node);

    for(RecordNode::children_iterator it = node->children_begin(); it != node->children_end(); it++)
    {
        RecordNode *child = GetNodeFromID(*it);
        ClearDirtyNode(child, tss);
        
        for(int i = 0; i < ds_->GetNumProfileValues(); i++)
        {
            if(!ds_->GetRecordMetadata(i).StaticValueFlag)
            {
                if(!node->GetChildrenValues().empty())
                    node->GetChildrenValues()[i] += child->GetTempValues()[i];
                tss_node->GetChildrenValues()[i] += child->GetTempValues()[i];

                if(!ds_->GetRecordMetadata(i).AccumulatedValueFlag)
                {
                    node->GetTempValues()[i] += child->GetTempValues()[i];
                }
            }
        }

    }
    
    for(int i = 0; i < ds_->GetNumProfileValues(); i++)
    {
        assert(tss_node->GetAllValues()[i] == 0);
        if(ds_->GetRecordMetadata(i).StaticValueFlag)
        {
            if(!node->GetAllValues().empty())
                node->GetAllValues()[i] = node->GetTempValues()[i];
            tss_node->GetAllValues()[i] = node->GetAllValues()[i];
        }
        else
        {
            if(!node->GetAllValues().empty())
                node->GetAllValues()[i] += node->GetTempValues()[i];
            tss_node->GetAllValues()[i] += node->GetTempValues()[i];
        }
    }
    node->SetDirty(false);
}

RecordNode *ThreadStore::GetRootNode()
{
    RecordNode *root = GetNodeFromID(1);
    return root;
}

void RecordNodeBasic::Accumulate(const RecordNodeBasic &rhs)
{
    for(int i = 0; i < ds_->GetNumProfileValues(); i++)
    {
        if(ds_->GetRecordMetadata(i).StaticValueFlag)
        {
            all_values_[i] = rhs.all_values_[i];
            children_values_[i] = rhs.children_values_[i];
        }
        else
        {
            all_values_[i] += rhs.all_values_[i];
            children_values_[i] += rhs.children_values_[i];
        }
    }
}

TimeSliceStore::TimeSliceStore()
{
    cout << "Allocate TimeSliceStore" << endl;
}

TimeSliceStore::~TimeSliceStore()
{
}

TimeSliceStore *ThreadStore::GetTimeSlice(int ts)
{
    if(time_slice_.empty())
    {
        start_time_number_ = ts;
    }
    if(ts < start_time_number_)
        return NULL;

    if((unsigned int)(ts - start_time_number_) >= time_slice_.size())
    {
        time_slice_.resize(ts - start_time_number_ + 1);
    }
    time_slice_[ts - start_time_number_].SetTimeNumber(ts);
    time_slice_[ts - start_time_number_].SetThreadStore(this);
    return &time_slice_[ts - start_time_number_];
}


string RecordNodeBasic::GetNameString()
{
    assert(ds_);
    return ds_->GetNameFromID(GetNameID());
}



void InitDataStore()
{
    gAllDataStore = new map<int, DataStore*>();
    gDataStoreIDSeq = 0;

    gGlobalDataStore = new GlobalDataStore(gDataStoreIDSeq);
    (*gAllDataStore)[gDataStoreIDSeq] = gGlobalDataStore;


    pthread_create(&g_datastore_server_thread, NULL, DataStoreAcceptThreadMain, NULL);
    pthread_detach(g_datastore_server_thread);

    pthread_create(&g_datastore_server_thread, NULL, DataStore_ReceiveThread, NULL);
    pthread_detach(g_datastore_server_thread);
}


DataStore *NewDataStore()
{
    gDataStoreIDSeq++;
    DataStore *ds = new DataStore(gDataStoreIDSeq);
    Locker lk(ds);
    (*gAllDataStore)[gDataStoreIDSeq] = ds;
    return ds;
}


void *DataStoreAcceptThreadMain(void *)
{
    int lsock = MakeServerSocket(12300);
    cout << "Socket created." << endl;
	while(1)
	{
		struct sockaddr_in client_addr;
		int len = sizeof(client_addr);
		int sock = accept(lsock,  (struct sockaddr *)&client_addr, (socklen_t *)&len);
        if(sock <= 0)
        {
            cout << "Accept Error" << endl;
            break;
        }
        cout << "Socket Accepted:" << sock << endl;
        DataStore *ds = NewDataStore();
        Locker lk(ds);
        ds->SetConnectedSocket(sock);
        ds->StartReceive();
	}
    return NULL;
}


bool DataStoreRequest(std::stringstream &out, string &mime, const std::vector<std::string> &path,
        const std::map<std::string,std::string> &data)
{
    mime = "text/javascript";
    if(path.size() > 0 && path[0] == "new")
    {
        vector<string> connect_to;
        string hostval = getm(data, "host", "");
        algorithm::split(connect_to, hostval, is_any_of(":"));
        if(connect_to.size() == 1)
            connect_to.push_back("12321");
        
        DataStore *ds = NewDataStore();
        Locker lk(ds);
        ds->SetConnectTo(connect_to[0], connect_to[1]);
        ds->StartReceive();
        ::write_json(out, "OK");
        return true;
    }
    else if(path.size() > 0 && path[0] == "list")
    {
        JsonList lw(out);
        for(map<int,DataStore*>::iterator iter = gAllDataStore->begin(); iter != gAllDataStore->end(); iter++)
        {
            Locker lk((*iter).second);
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
    else if(path.size() > 1 && path[0] == "tree")
    {
        map<int, DataStore*>::iterator pos;
        try{
            pos = gAllDataStore->find(lexical_cast<int>(path[1]));
        }
        catch(bad_lexical_cast &e)
        {
            out << "{}";
            return true;
        }
        if(pos == gAllDataStore->end())
        {
            out << "{}";
            return true;
        }
        DataStore *ds = (*pos).second;
        Locker lk(ds);

        if(path.size() > 2 && path[2] == "current")
        {
            ds->DumpJSON(out, DF_CURRENT_TREE, 0, 0);
        }
        else if(path.size() > 3 && path[2] == "diff")
        {
            ds->DumpJSON(
                out,
                DF_DIFF_TREE,
                lexical_cast<int>(path[3]), 
                path.size() > 2 ? lexical_cast<int>(path[4]) : 0
            );
        }
        else if(path.size() > 3 && path[2] == "check")
        {
            ds->CheckAllTimeSlice();
            out << "Done.";
            mime = "text/plain";
        }
        return true;
    }
    else if(path.size() > 0 && path[0] == "dump")
    {
        for(map<int, DataStore*>::iterator it = gAllDataStore->begin(); it != gAllDataStore->end(); it++)
        {
            out << "*** DataStore "  << (*it).first << endl;
           (*it).second->DumpText(out);
            out << endl;
            out << endl;
        }
        return true;
    }
    return false;
}



void RecordNode::UpdateDataStore()
{
    temp_values_.resize(GetDataStore()->GetNumProfileValues());
}

RecordNodeBasic *TimeSliceStore::GetNodeFromID(NodeID id, bool add)
{
    map< NodeID, RecordNodeBasic >::iterator it = tree_.find(id);
    if(it == tree_.end())
    {
        if(!add)
            return NULL;
        RecordNodeBasic &new_node = tree_[id];
        new_node.SetDataStore(ts_->GetDataStore());
        new_node.SetNodeID(id);
        new_node.SetParentNodeID(0);
        return &new_node;
    }
    return &(*it).second;
}

void RecordNodeBasic::PrintToStream(ostream &out) const
{
    out << "B ID:" << id_ << " p:" << parent_id_ << " " << ds_->GetNameFromID(node_name_) << "[" << node_name_ << "] ";
    
    for(int i = 0; i < ds_->GetNumProfileValues(); i++)
    {
        out << "(" << i << ")" << all_values_[i] << ":" << children_values_[i];
    }
    if(running_)
        out << "[Run]";
}

void RecordNode::PrintToStream(ostream &out) const
{
    out << "R ID:" << id_ << " GNID:" << gnid_ << " p:" << parent_id_ << " " << ds_->GetNameFromID(node_name_) << "[" << node_name_ << "] ";
    
    for(int i = 0; i < ds_->GetNumProfileValues(); i++)
    {
        out << "(" << i << ")" << all_values_[i] << ":" << children_values_[i] << ":" << temp_values_[i];
    }
    if(running_)
        out << "[Run]";
    if(dirty_)
        out << "[Dirty]";
}

void DataStore::CheckAllTimeSlice()
{
    for(map<ThreadID, ThreadStore *>::iterator it = threads_.begin(); it != threads_.end(); it++)
    {
        (*it).second->CheckAllTimeSlice();
    }

}

unsigned long long int HashStrHard(const string &str)
{
    crc_basic<64> crc64( 0x1021F0A7542AEC14, 0xFFFFFFFFFFFFFFFF, 0, false, false );
    crc64.process_bytes( str.c_str(), str.length() );
    return crc64.checksum();
}

NodeID DataStore::TouchGNID(ThreadStore *ts, RecordNode *node)
{
    if(node->GetGNID())
        return node->GetGNID();
    
    RecordNode *parent = ts->GetNodeFromID(node->GetParentNodeID());
    NodeID parent_gnid = TouchGNID(ts, parent);
    
    NodeID result = HashStrHard(lexical_cast<string>(parent_gnid) + "/" + GetNameFromID(node->GetNameID()));
    node->SetGNID(result);
    return result;
}

GlobalDataStore::GlobalDataStore(int id)
    :DataStore(id)
{
    current_time_number_ = 0;
    target_name_ = "global";
    num_records_ = 0;
    is_single_tree_mode_ = true;
}

NodeID GlobalThreadStore::GNIDToSeq(NodeID gnid)
{
    if(gnid < 2)
        return gnid;

    map<NodeID, NodeID>::iterator it = gnid_to_seq_.find(gnid);
    if(it == gnid_to_seq_.end())
    {
        seq_num_++;
        gnid_to_seq_[gnid] = seq_num_;
        return seq_num_;
    }
    return (*it).second;
}

void GlobalThreadStore::Integrate()
{

    TimeSliceStore *dest_tss = GetCurrentTimeSlice();
    bool nowvalue_copied = false;
    
    for(map<int, DataStore*>::iterator dsit = gAllDataStore->begin(); dsit != gAllDataStore->end(); dsit++)
    {
        DataStore *ds = (*dsit).second;
        for(DataStore::thread_store_iterator tsit = ds->thread_store_begin(); 
                    tsit != ds->thread_store_end(); tsit++)
        {
            ThreadStore *ts = (*tsit).second;
            if(ts == this)
                continue;
            TimeSliceStore *src_tss = ts->GetCurrentTimeSlice();
            
            if(!nowvalue_copied)
            {
                nowvalue_copied = true;
                running_node_ = src_tss->GetRunningNode();
                now_values_ = src_tss->GetNowValues();
                dest_tss->SetNowValues(src_tss->GetRunningNode(), src_tss->GetNowValues());
            }
            
            
            
            for(TimeSliceStore::map_iterator nit = src_tss->nodes_begin(); nit != src_tss->nodes_end(); nit++)
            {
                RecordNodeBasic *src = &(*nit).second;
                RecordNode *src_current = src_tss->GetThreadStore()->GetNodeFromID(src->GetNodeID());
                if(!src_current)
                {
                    cout << "Error:" << endl;
                    src_tss->GetThreadStore()->DumpText(cout);
                    assert(false);
                }
                NodeID dest_node_id = GNIDToSeq(src_current->GetGNID());
                
                RecordNode *dest_current = dest_tss->GetThreadStore()->GetNodeFromID(dest_node_id);
                if(!dest_current)
                {
                    NodeID dest_parent_id = 0;
                    RecordNode *src_current_parent = src_tss->GetThreadStore()->GetNodeFromID(src_current->GetParentNodeID());
                    if(src_current_parent)
                        dest_parent_id = GNIDToSeq(src_current_parent->GetGNID());

                    dest_current = AddCurrentNode(dest_node_id, dest_parent_id, dest_node_id);
                    assert(dest_current->GetNodeID() < 1000000);
                    dest_current->SetGNID(src_current->GetGNID());
                    this->GetDataStore()->SetNameID(dest_node_id, src_current->GetNameString());
                }
                RecordNodeBasic *dest = dest_tss->GetNodeFromID(dest_node_id, true);
                assert(dest_current->GetNodeID() < 1000000);
                
                for(int i = 0; i < ds_->GetNumProfileValues(); i++)
                {
                    if(!ds_->GetRecordMetadata(i).StaticValueFlag)
                    {
                        dest->GetAllValues()[i] += src->GetAllValues()[i];
                        dest->GetChildrenValues()[i] += src->GetChildrenValues()[i];
                        dest_current->GetAllValues()[i] += src->GetAllValues()[i];
                        dest_current->GetChildrenValues()[i] += src->GetChildrenValues()[i];
                    }
                }
            }
            if(IsSingleTreeMode())
            {
                cout << "Removing time slice..." << endl;
                ts->RemoveTimeSlice();
            }
        }
    }
}


void GlobalDataStore::Integrate()
{
    
    if(num_records_ == 0)
    {
        for(map<int, DataStore*>::iterator dsit = gAllDataStore->begin(); dsit != gAllDataStore->end(); dsit++)
        {
            DataStore *ds = (*dsit).second;
            if(ds == this)
                continue;
            
            if(ds->GetNumProfileValues() != 0)
            {
                num_records_ = ds->GetNumProfileValues();
                record_metadata_ = ds->GetRecordMetadataVector();
                threads_[0] = new GlobalThreadStore(this);
                threads_[0]->SetThreadID(0);
                break;
            }
            
        }
    }
    if(num_records_ != 0)
    {
        current_time_number_++;
        ((GlobalThreadStore*)threads_[0])->Integrate();
    }
}

} // namespace llprof
