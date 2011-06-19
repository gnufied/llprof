/* class_table.h - クラス名テーブルとメソッド名テーブルのデータ構造
 */

#ifndef RRPROF_CLASSTBL_H
#define RRPROF_CLASSTBL_H

#include "platforms.h"
#include "call_tree.h"

#include <map>

class NameTable
{

private:
    pthread_mutex_t mtx_;
    std::map<nameid_t, const char *> table_;
    
public:
    NameTable();

    void AddCB(nameid_t key, void *data_ptr);

    const char *Get(nameid_t key);

};

NameTable *GetNameIDTable();


#endif
