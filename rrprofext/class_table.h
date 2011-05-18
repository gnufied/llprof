
/* class_table.h - クラス名テーブルとメソッド名テーブルのデータ構造
 */

#ifndef RRPROF_CLASSTBL_H
#define RRPROF_CLASSTBL_H

#include <ruby/ruby.h>

#include "platforms.h"


class NameTable
{
public:
    typedef unsigned long long Key;

private:
    pthread_mutex_t mtx_;
    struct st_table *table_;
    
public:
    NameTable();

    void AddCB(Key key, const char * cb(Key key));
    const char *Get(Key key);

};

NameTable *GetMethodNameTable();
NameTable *GetClassNameTable();


#endif
