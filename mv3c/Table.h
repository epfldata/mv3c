#ifndef TABLE_H
#define TABLE_H
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <string>

template <typename K, typename V>
struct Table {
    cuckoohash_map<K, Entry<K, V>*, CityHasher<K>> primaryIndex;
    ConcurrentStore<Entry<K, V>> entryStore;
    ConcurrentStore<DeltaVersion<K, V>> dvStore;

    Table(const size_t es, const float vl, const std::string& n) : entryStore(es, n + "EntryStore"), dvStore(es * vl, n + "DVstore") {
    }

    forceinline OperationReturnStatus insert(Transaction *xact, const K& k, const V& v, PRED* parent = nullptr) {
        Entry<K, V>* e;
        if (primaryIndex.find(k, e)) {
            //            xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
            e->dv->val = v;
        } else {
            e = new(entryStore.add()) Entry<K, V>(this, k);
            auto ret = primaryIndex.insert(k, e);
            assert(ret);
            xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, v, parent);

        }
        return OP_SUCCESS;
    }

    forceinline OperationReturnStatus update(Transaction *xact, Entry<K, V> *e, const V& newVal, PRED* parent = nullptr, const bool allowWW = false, const col_type& colsChanged = col_type(-1)) {
        //        xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, newVal, colsChanged, parent);
        e->dv->val = newVal;
        return OP_SUCCESS;
    }

    forceinline const DeltaVersion<K, V>* getReadOnly(Transaction *xact, const K& key) {
        Entry<K, V> *e;
        if (primaryIndex.find(key, e)) {
            assert(e->dv);
            return e->dv;
        } else {
            return nullptr;
        }
    }
};


#endif /* TABLE_H */

