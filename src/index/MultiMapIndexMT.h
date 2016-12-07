#ifndef MULTIMAPINDEXMT_H
#define MULTIMAPINDEXMT_H
#include <map>
#include "index/SecondaryIndex.h"
#include "lock/RWLock.h"

template <typename K, typename V, typename P>
struct MultiMapIndexMT : SecondaryIndex<K, V> {
    typedef Entry<K, V>* EntryPtrType;
    typedef typename std::multimap<P, EntryPtrType>::iterator iterator;
    RWLock lock;
    std::multimap<P, EntryPtrType> index;

    void insert(EntryPtrType e, const V& val) override {
        lock.AcquireWriteLock();
        index.insert({P(e, val), e});
        lock.ReleaseWriteLock();
    }

    void erase(EntryPtrType e, const V& val) override {
        lock.AcquireWriteLock();
        index.erase(P(e, val));
        lock.ReleaseWriteLock();
    }

    EntryPtrType get(const P& pkey) {
        lock.AcquireReadLock();
        auto it = index.find(pkey);
        if (it == index.end()) {
            lock.ReleaseReadLock();
            return nullptr;
        } else {
            lock.ReleaseReadLock();
            return it->second;
        }
    }

    std::pair<iterator, iterator> slice(const P& key) {
        lock.AcquireReadLock(); //released outside
        return index.equal_range(key);
    }

};


#endif /* MULTIMAPINDEXMT_H */

