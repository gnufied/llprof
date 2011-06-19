
#include <iostream>
using namespace std;

#include "platforms.h"
#include "class_table.h"

#include <cstring>

NameTable gNameIDTbl;

NameTable *GetNameIDTable()
{
    return &gNameIDTbl;
}

const char *nullstr = "(null)";
const char *new_str(const char *str)
{
    if(!str)
        return nullstr;
    char *new_buf = new char[strlen(str)+1];
    strcpy(new_buf, str);
    return new_buf;
}

NameTable::NameTable()
{
    pthread_mutex_init(&mtx_, NULL);
}


// value: rb_class2name(klass)
void NameTable::AddCB(nameid_t key, void *data_ptr)
{
    char *name;
    //pthread_mutex_lock(&mtx_);
    
    map<nameid_t, const char *>::iterator it = table_.find(key);
    if(it == table_.end())
    {
        const char *ns = new_str(llprof_call_name_func(key, data_ptr));
        table_.insert(make_pair(key, ns));
    }
    //pthread_mutex_unlock(&mtx_);
}

const char *null_str = "(ERR)";
const char * NameTable::Get(nameid_t entry)
{
    //pthread_mutex_lock(&mtx_);
    map<nameid_t, const char *>::iterator it = table_.find(entry);
    if(it == table_.end())
    {
        //pthread_mutex_unlock(&mtx_);
        return null_str;
    }
    //pthread_mutex_unlock(&mtx_);
    return (*it).second;
}



