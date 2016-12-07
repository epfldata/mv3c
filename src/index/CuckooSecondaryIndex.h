#ifndef CUCKOOSECONDARYINDEX_H
#define CUCKOOSECONDARYINDEX_H
#include "index/SecondaryIndex.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <unordered_set>
#include <atomic>
#include "lock/RWLock.h"


//Uses cuckoo hash map to store a set along with a lock for every partial key

template <typename K, typename V, typename P>
struct CuckooSecondaryIndex : SecondaryIndex<K, V> {
    typedef Entry<K, V>* EntryPtrType;

    struct Container {
        std::unordered_set<EntryPtrType> bucket;
        RWLock lock;
    };
    typedef MemoryPool<Container, sizeof (Container) * 4096 * 1024 > PoolType;
    static thread_local PoolType* store;
    cuckoohash_map<P, Container *, CityHasher<P>> index;

    CuckooSecondaryIndex(size_t size = 100000) : index(size) {
    }

    void insert(EntryPtrType e, const V& val) override {
        Container *c;
        P key(e, val);
        //create new container, lock it, and try to insert it
        c = new Container();
        c->lock.AcquireWriteLock();

        //if container already exists, delete newly created one, and get lock on the existing one
        auto updatefn = [&c, e](Container*& c2) {
            delete c;
            c = c2;
            c->lock.AcquireWriteLock();
        };
        index.upsert(key, updatefn, c);

        //insert e into the bucket and release lock
        c->bucket.insert(e);
        c->lock.ReleaseWriteLock();
    }

    void erase(EntryPtrType e, const V& val) override {
        Container *c;
        //get the container lock it, remove e and then unlock
        if (index.find(P(e, val), c)) {
            c->lock.AcquireWriteLock();
            c->bucket.erase(e);
            c->lock.ReleaseWriteLock();
        } else {
            throw std::logic_error("Entry to be deleted not in Secondary Index");
        }

    }

    forceinline Container * slice(const P key) {
        Container *c;
        if (index.find(key, c)) {
            return c;
        } else {
            return nullptr;
        }
    }
};
template <typename K, typename V, typename P>
thread_local typename CuckooSecondaryIndex<K, V, P>::PoolType* CuckooSecondaryIndex<K, V, P>::store;
#endif /* CUCKOOSECONDARYINDEX_H */

