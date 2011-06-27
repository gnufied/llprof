
#include <iostream>
#include <memory>
#include <algorithm>

#include "boost_wrap.h"
#include "data_store.h"
#include "../llprofcommon/network.h"
#include "../llprofcommon/llprof_const.h"
#include "http.h"


namespace llprof{



RecordNode::RecordNode()
{
}


void RecordNode::SetDataStore(DataStore *ds)
{
    ds_ = ds;
    values_.resize(ds_->GetNumProfileValues());
}
    
void TimeSliceStore::SetThreadStore(ThreadStore *ts)
{
    ts_ = ts;
}
void TimeSliceStore::SetTimeNumber(int tn)
{
    time_number_ = tn;
}

ProfileValue TimeSliceStore::GetProfileValue(NodeID nid, int idx)
{
    if(tree_[nid].empty())
        tree_[nid].resize(ts_->GetDataStore()->GetNumProfileValues());
    return tree_[nid][idx];
}

void TimeSliceStore::SetProfileValue(NodeID nid, int idx, ProfileValue value)
{
    if(tree_[nid].empty())
        tree_[nid].resize(ts_->GetDataStore()->GetNumProfileValues());
    tree_[nid][idx] = value;
}

void TimeSliceStore::DumpText(ostream &strm)
{
    strm << "<TimeSlice [" << time_number_ << "]>" << endl;
    
    for(map< NodeID, vector<ProfileValue> >::iterator it = tree_.begin(); it != tree_.end(); it++)
    {
        strm << "  Node " << (*it).first << " ";
        for(int i = 0; i < ts_->GetDataStore()->GetNumProfileValues(); i++)
        {
            strm << ":" << (*it).second[i];
        }
        strm << ":" << endl;
    }
}


DataStore::DataStore(int id)
{
    id_ = id;
    socket_ = 0;
    closed_ = false;
    current_time_number_ = 0;
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
    
    
    while(GetProfileData())
    {
        sleep(1);
    }
    

    return NULL;
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
                    name_table_[nameid] = "<Asking>";
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
            name_table_[query_names[i+1]] = s;
        }
    }

    for(map< ThreadID, intrusive_ptr<MessageBuf> >::iterator iter = buf_for_threads.begin();
        iter != buf_for_threads.end(); iter++)
    {

        map<ThreadID, ThreadStore>::iterator tpos = threads_.find((*iter).first);
        if(tpos == threads_.end())
        {
            threads_.insert(make_pair((*iter).first, ThreadStore(this)));
            tpos = threads_.find((*iter).first);
            (*tpos).second.SetThreadID((*iter).first);
        }
        ThreadStore &ts = (*tpos).second;

        // Store Now info
        intrusive_ptr<MessageBuf> nibuf = nowinfo_buf_for_threads[(*iter).first];
        nibuf->SetCurrentPtr(8);
        unsigned running_node = nibuf->Get<unsigned int>();
        
        nibuf->SetCurrentPtr(16);
        assert((size_t)nibuf->GetRemainSize() >= GetNumProfileValues() * sizeof(ProfileValue));
        ts.UpdateNowValues(running_node, nibuf->GetCurrentBufPtr());

        // Store profile data
        ts.Store((*iter).second);


    }
    return true;
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

    for(map<ThreadID, ThreadStore>::iterator it = threads_.begin(); it != threads_.end(); it++)
    {
        (*it).second.DumpText(strm);
    }
    strm << "<End>" << endl;
}

void DataStore::DumpJSON(stringstream &strm, int flag, int p1, int p2)
{
    JsonDict data(strm);
    data.add("timenum", GetCurrentTimeNumber());
    JsonList threads;
    for(map<ThreadID, ThreadStore>::iterator it = threads_.begin(); it != threads_.end(); it++)
    {
        stringstream item;
        (*it).second.DumpJSON(item, flag, p1, p2);
        threads.add<stringstream &>(item);
    }
    data.add<stringstream &>("threads", threads.strm());

    JsonDict record_metadata_dict;
    cout << "Adding md" << endl;
    for(size_t i = 0; i < record_metadata_.size(); i++)
    {
        cout << "Adding: " << i << endl;
        JsonDict dmd;
        dmd.add("field_name", record_metadata_[i].FieldName);
        dmd.add("unit", record_metadata_[i].Unit);
        dmd.add("flags", record_metadata_[i].Flags);

        record_metadata_dict.add<stringstream &>(lexical_cast<string>(i), dmd.strm());
    }
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

void write_json(stringstream &strm, const RecordNode &node)
{
    JsonDict out(strm);
    out.add<unsigned int>("id", node.GetNodeID());
    out.add<unsigned int>("pid", node.GetParentNodeID());
    out.add<string>("name", node.GetDataStore()->GetNameFromID(node.GetNameID()));
    
    stringstream s;
    ProfileValuesToStream(s, node.GetValuesVector());
    out.add<stringstream &>("values", s);

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
        
        map<NodeID, RecordNode> integrated;
        TimeSliceStore *last_tss = NULL;
        for(int t = p1; t <= p2; t++)
        {
            TimeSliceStore *tss = GetTimeSlice(t);
            if(!tss)
                continue;
            last_tss = tss;
            for(TimeSliceStore::map_iterator it = tss->nodes_begin(); it != tss->nodes_end(); it++)
            {
                map<NodeID, RecordNode>::iterator pos = integrated.find((*it).first);
                if(pos == integrated.end())
                {
                    RecordNode &node = integrated[(*it).first];
                    node.SetDataStore(ds_);
                    node.SetNodeID((*it).first);
                    node.SetParentNodeID(current_tree_[(*it).first].GetParentNodeID());
                    node.SetNameID(current_tree_[(*it).first].GetNameID());
                    pos = integrated.find((*it).first);
                }
                (*pos).second.Accumulate((*it).second);
            }
        } 
        
        for(map<NodeID, RecordNode>::iterator it = integrated.begin(); it != integrated.end(); it++)
            nodes.add<RecordNode &>((*it).second);
        
        store.add<stringstream &>("nodes", nodes.strm());
        if(last_tss)
        {
            stringstream tmp;
            ProfileValuesToStream(tmp, last_tss->GetNowValues());
            store.add<stringstream &>("now_values", tmp);
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
        strm << "  Node:" << n.GetNodeID() << " name:" << ds_->GetNameFromID(n.GetNameID()) 
                            << "(" << n.GetNameID() << ")  parent:" << n.GetParentNodeID() << " ";
                            
        for(int i = 0; i < ds_->GetNumProfileValues(); i++)
        {
            strm << ":" << n.GetProfileValue(i);
        }
        strm << ":" << endl;
    }
    
    
    for(size_t i = 0; i < time_slice_.size(); i++)
    {
        strm << endl;
        time_slice_[i].DumpText(strm);
    }
}

void ThreadStore::Store(intrusive_ptr<MessageBuf> buf)
{
    int offset_call_node_id = ds_->GetMemberOffsetOf("pdata.call_node_id");
    int offset_parent_node_id = ds_->GetMemberOffsetOf("pdata.parent_node_id");
    int offset_nameid = ds_->GetMemberOffsetOf("pdata.nameid");
    int pdata_record_size = ds_->GetMemberOffsetOf("pdata.record_size");
    int offset_value = ds_->GetMemberOffsetOf("pdata.value");

    buf->SetCurrentPtr(8);
    vector<ProfileValue> values(ds_->GetNumProfileValues());

    TimeSliceStore *tss = GetTimeSlice(ds_->GetCurrentTimeNumber());
    assert(tss);
    tss->SetNowValues(running_node_, now_values_);
    while(!buf->IsEOF())
    {
        char *p = buf->GetCurrentBufPtr();
        
        unsigned int cnid = *(unsigned int *)(p + offset_call_node_id);
        unsigned int parent_cnid = *(unsigned int *)(p + offset_parent_node_id);
        NameID nameid = *(NameID *)(p + offset_nameid);
        

        
        map<NodeID, RecordNode>::iterator iter = current_tree_.find(cnid);
        if(iter == current_tree_.end())
        {
            RecordNode &node = current_tree_[cnid];
            node.SetDataStore(ds_);
            node.SetNodeID(cnid);
            node.SetParentNodeID(parent_cnid);
            node.SetNameID(nameid);
            iter = current_tree_.find(cnid);
        }
        RecordNode &node = (*iter).second;

        for(int i = 0; i < ds_->GetNumProfileValues(); i++)
        {
            values[i] = ((ProfileValue *)(p + offset_value))[i];
            tss->SetProfileValue(cnid, i, values[i]);
        }
        node.Accumulate(values);
        buf->Seek(pdata_record_size);
    }
    
}

void RecordNode::Accumulate(const vector<ProfileValue> &values)
{
    for(int i = 0; i < ds_->GetNumProfileValues(); i++)
    {
        if(ds_->GetRecordMetadata(i).AccumuratedValueFlag)
            values_[i] = values[i];
        else
            values_[i] += values[i];
    }
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


map<int, DataStore*> *gAllDataStore;
int gDataStoreIDSeq;
void InitDataStore()
{
    gAllDataStore = new map<int, DataStore*>();
    gDataStoreIDSeq = 0;
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
        
        gDataStoreIDSeq++;
        DataStore *ds = new DataStore(gDataStoreIDSeq);
        Locker lk(ds);



        (*gAllDataStore)[gDataStoreIDSeq] = ds;
        ds->SetConnectTo(connect_to[0], connect_to[1]);
        ds->StartThread();
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


} // namespace llprof
