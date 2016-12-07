
#ifndef ENTRY_H
#define ENTRY_H
#include "util/types.h"

template<typename K, typename V>
struct Entry {
    const K key;
    typedef Store<Entry<K, V>> StoreType;
    static StoreType store;
    Table<K, V> * const tbl;
    std::atomic<DeltaVersion<K, V>*> dv; //pointer to first version in version chain

    Entry(Table<K, V> * tb, const K&k) : key(k), tbl(tb), dv(nullptr) {
    }

};


#endif /* ENTRY_H */

