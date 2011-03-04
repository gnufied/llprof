
#include <pthread.h>
#include "class_table.h"

st_table *gClassTbl;

pthread_mutex_t gClassTblMutex;

typedef unsigned long long int class_key_t;


int clstbl_cmp(class_key_t *a, class_key_t *b) 
{
    return (*a != *b);
}

int clstbl_hash(class_key_t *key)
{
    return *reinterpret_cast<int *>(&key);
}

struct st_hash_type clstbl_hash_type = {
    (int (*)(...))clstbl_cmp,
    (st_index_t (*)(...)) clstbl_hash
};


void InitClassTbl()
{
    pthread_mutex_init(&gClassTblMutex, NULL);
	gClassTbl = st_init_table(&clstbl_hash_type);
}

char *new_str(const char *str)
{
    char *new_buf = (char *)malloc(strlen(str)+1);
    strcpy(new_buf, str);
    return new_buf;
}

const char *AddClassName(VALUE klass)
{
    class_key_t entry = (class_key_t)klass;
    char *name;
    pthread_mutex_lock(&gClassTblMutex);
	if(!st_lookup(gClassTbl, (st_data_t)&entry, (st_data_t *)&name))
	{
        char *ns = new_str(rb_class2name(klass));
        // printf("Add Class : %s\n", ns);
		st_insert(
            gClassTbl,
            (st_data_t)entry,
            (st_data_t)ns
        );
        pthread_mutex_unlock(&gClassTblMutex);
        return ns;
	}
    pthread_mutex_unlock(&gClassTblMutex);
    return name;
}

__thread const char *null_str = "(ERR)";
const char * GetClassName(VALUE klass)
{
    class_key_t entry = (class_key_t)klass;
    char *name;
    pthread_mutex_lock(&gClassTblMutex);
	if(!st_lookup(gClassTbl, (st_data_t)&entry, (st_data_t *)&name))
	{
        pthread_mutex_unlock(&gClassTblMutex);
        // return null_str;
        return NULL;
	}
    pthread_mutex_unlock(&gClassTblMutex);
    return name;
}



