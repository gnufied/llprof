
/* class_table.h - クラス名テーブルとメソッド名テーブルのデータ構造
 */

#ifndef RRPROF_CLASSTBL_H
#define RRPROF_CLASSTBL_H

#include <ruby/ruby.h>

#include "platforms.h"
#include "call_tree.h"

class NameTable
{

private:
    pthread_mutex_t mtx_;
    struct st_table *table_;
    
public:
    NameTable();

    void AddCB(nameid_t key, const char * cb(void *data_ptr), void *data_ptr);

    const char *Get(nameid_t key);

};

NameTable *GetNameIDTable();


#endif
