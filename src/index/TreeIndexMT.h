#ifndef TREEINDEXMT_H
#define TREEINDEXMT_H
#include "index/SIndex.h"
#include "lock/RWLock.h"

template <typename K, typename ENTRY, typename FN>
struct TreeIndexMT : public Index<K, ENTRY> {
    RWLock lock;
    TreeIndex<K, ENTRY, FN> index;

    void add(ENTRY* obj, uint8_t tid) override {
        lock.AcquireWriteLock();
        auto h = FN::hash(obj->key);
        index.add(obj, h, tid);
        lock.ReleaseWriteLock();
    }

    void add(ENTRY* obj, const size_t h, uint8_t tid) override {
        lock.AcquireWriteLock();
        index.add(obj, h, tid);
        lock.ReleaseWriteLock();
    }

    void del(const ENTRY* obj, const size_t h, uint8_t tid) override {
        lock.AcquireWriteLock();
        index.del(obj, h, tid);
        lock.ReleaseWriteLock();
    }

    void del(const ENTRY* obj, uint8_t tid) override {
        lock.AcquireWriteLock();
        auto h = FN::hash(obj->key);
        index.del(obj, h, tid);
        lock.ReleaseWriteLock();
    }

    TreeIndex<K, ENTRY, FN>::iterator slice(const K& key) {
        lock.AcquireReadLock();
        auto it = index.slice(key);
        lock.ReleaseReadLock();
        return it;
    }

};

#endif /* TREEINDEXMT_H */

