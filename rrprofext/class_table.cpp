
#include <iostream>
using namespace std;

#include "platforms.h"
#include "class_table.h"

NameTable gMethodTbl, gClassTbl;

NameTable *GetMethodNameTable()
{
    return &gMethodTbl;
}
NameTable *GetClassNameTable()
{
    return &gClassTbl;
}

int clstbl_cmp(NameTable::Key a, NameTable::Key b) 
{
    return (a != b);
}

int clstbl_hash(NameTable::Key key)
{
    int hval = *reinterpret_cast<int *>(&key);
    return hval;
}

struct st_hash_type clstbl_hash_type = {
    (int (*)(...))clstbl_cmp,
    (st_index_t (*)(...)) clstbl_hash
};

const char *nullstr = "(null)";
const char *new_str(const char *str)
{
    if(!str)
        return nullstr;
    char *new_buf = (char *)malloc(strlen(str)+1);
    strcpy(new_buf, str);
    return new_buf;
}

NameTable::NameTable()
{
    pthread_mutex_init(&mtx_, NULL);
    table_ = st_init_table(&clstbl_hash_type);
}


// value: rb_class2name(klass)
void NameTable::AddCB(Key key, const char * cb(Key key))
{
    char *name;
    pthread_mutex_lock(&mtx_);
	if(!st_lookup(table_, (st_data_t)key, (st_data_t *)&name))
	{
        const char *ns = new_str(cb(key));
        // printf("Add Class : %s\n", ns);
		st_insert(
            table_,
            (st_data_t)key,
            (st_data_t)ns
        );
        pthread_mutex_unlock(&mtx_);
	}
    pthread_mutex_unlock(&mtx_);
}

const char *null_str = "(ERR)";
const char * NameTable::Get(Key entry)
{
    char *name;
    pthread_mutex_lock(&mtx_);
	if(!st_lookup(table_, (st_data_t)entry, (st_data_t *)&name))
	{
        pthread_mutex_unlock(&mtx_);
        return null_str;
        //return NULL;
	}
    pthread_mutex_unlock(&mtx_);
    return name;
}



