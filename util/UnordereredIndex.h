
#ifndef UNORDEREREDINDEX_H
#define UNORDEREREDINDEX_H
#include "RWLock.h"
#include <unordered_map>

template <typename K, typename E, typename H>
struct UnorderedIndex {
    RWLock lock_;
    std::unordered_map<K, E, H> index;

    UnorderedIndex(size_t s) : index(s) {
    }

    bool insert(const K& key, E val) {
        lock_.AcquireWriteLock();
        if (index.find(key) == index.end()) {
            index[key] = val;
            lock_.ReleaseWriteLock();
            return true;
        } else {
            lock_.ReleaseWriteLock();
            return false;
        }
    }

    bool find(const K& key, E& val) {
        lock_.AcquireReadLock();
        if (index.find(key) == index.end()) {
            lock_.ReleaseReadLock();
            return false;
        } else {
            val = index.at(key);
            lock_.ReleaseReadLock();
            return true;
        }


    }

    bool erase(const K& key) {
        lock_.AcquireWriteLock();
        if (index.find(key) == index.end()) {
            lock_.ReleaseWriteLock();
            return false;
        } else {
            index.erase(key);
            lock_.ReleaseWriteLock();
            return true;
        }
    }
};


#endif /* UNORDEREREDINDEX_H */

