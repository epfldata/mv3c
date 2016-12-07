#ifndef HASHINDEXMT_H
#define HASHINDEXMT_H
#include "index/SIndex.h"
#include "lock/RWLock.h"

template <typename K, typename ENTRY, typename FN>
struct HashIndexMT : public Index<K, ENTRY> {
    RWLock lock;
    HashIndex<K, ENTRY, FN, false> index;

    void add(ENTRY* obj, uint8_t tid) override {
        lock.AcquireWriteLock();
        auto h = FN::hash(obj->key);
        index.add(obj, h, tid);
        lock.ReleaseReadLock();
    }

    void del(const ENTRY* obj, uint8_t tid) override {
        lock.AcquireWriteLock();
        auto h = FN::hash(obj->key);
        index.del(obj, h, tid);
        lock.ReleaseWriteLock();
    }

    void add(ENTRY* obj, const HASH_RES_t h, uint8_t tid) override {
        lock.AcquireWriteLock();
        index.add(obj, h, tid);
        lock.ReleaseReadLock();
    }

    void del(const ENTRY* obj, const HASH_RES_t h, uint8_t tid) override {
        lock.AcquireWriteLock();
        index.del(obj, h, tid);
        lock.ReleaseWriteLock();

    }

    HashIndex<K, ENTRY, FN, false>::iterator slice(const K& key) {
        lock.AcquireReadLock();
        auto it = index.slice(key);
        lock.ReleaseReadLock();
        return it;
    }

};


#endif /* HASHINDEXMT_H */

