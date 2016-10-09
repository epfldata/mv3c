
#ifndef ENTRY_H
#define ENTRY_H
#include "types.h"

template<typename K, typename V>
struct Entry {
    const K key;
    Table<K, V> * const tbl;
    DeltaVersion<K, V>* dv;

    Entry(Table<K, V> * tb, const K&k) : key(k), tbl(tb), dv(nullptr) {
    }

};


#endif /* ENTRY_H */

