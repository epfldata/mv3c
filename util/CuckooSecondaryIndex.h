#ifndef CUCKOOSECONDARYINDEX_H
#define CUCKOOSECONDARYINDEX_H
#include "SecondaryIndex.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <unordered_set>
#include <atomic>
#include "RWLock.h"

template <typename K, typename V, typename P>
struct CuckooSecondaryIndex : SecondaryIndex<K, V> {
    typedef Entry<K, V>* EntryPtrType;
    struct Container {
        std::unordered_set<EntryPtrType> bucket;
        RWLock lock;
    };
    cuckoohash_map<P, Container *, CityHasher<P>> index;

    void insert(EntryPtrType e, const V& val) override {
        Container *c;
        P key(e, val);
        if (index.find(key, c)) {
            c->lock.AcquireWriteLock();
            c->bucket.insert(e);
            c->lock.ReleaseWriteLock();
        } else {
            c = new Container();
            c->lock.AcquireWriteLock();
            c->bucket.insert(e);
            if (index.insert(key, c)) {
                c->lock.ReleaseWriteLock();
            } else {
                delete c;
                index.find(key, c);
                c->lock.AcquireWriteLock();
                c->bucket.insert(e);
                c->lock.ReleaseWriteLock();
            }
        }
    }

    void erase(EntryPtrType e, const V& val) override {
        Container *c;
        if (index.find(P(e, val), c)) {
            c->lock.AcquireWriteLock();
            c->bucket.erase(e);
            c->lock.ReleaseWriteLock();
        } else {
            throw std::logic_error("Entry to be deleted not in Secondary Index");
        }

    }

    forceinline  Container * slice(const P key) {
        Container *c;
        if (index.find(key, c)) {
            return c;
        } else {
            return nullptr;
        }
    }
};

#endif /* CUCKOOSECONDARYINDEX_H */

