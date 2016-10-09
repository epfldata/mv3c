
#ifndef CONCSTORE_H
#define CONCSTORE_H

#include <cstring>
#include <functional>
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>
#include <mutex>
#include "types.h"
template <typename T>
struct ConcurrentElement;

template <typename T>
union ConcurrentCell {
    T entry;
    ConcurrentElement<T>* next;

    ConcurrentCell() {
        memset(this, 0, sizeof (ConcurrentCell));
    }

    ~ConcurrentCell() {
    }
};

template <typename T>
struct ConcurrentElement {
    ConcurrentCell<T> cell;
    std::atomic<char> accessCounter;
    bool isActive;

    ConcurrentElement(ConcurrentElement<T>* next = nullptr) {
        isActive = false;
        cell.next = next;
        accessCounter = 0;

    }
};

template <typename T>
struct ConcurrentPool {
    ConcurrentElement<T> *array;
    ConcurrentPool<T>* next;

    ConcurrentPool(size_t size, ConcurrentPool<T>* n = nullptr) {
        array = new ConcurrentElement<T>[size];
        next = n;
    }

    ~ConcurrentPool() {
        delete[] array;
    }
};

template <typename T>
class ConcurrentStore {
    ConcurrentPool<T> start, *currentPool;
    const size_t poolsize;
    std::atomic<size_t> currentArrayIndex;
    std::atomic<ConcurrentElement<T>*> free;
//    std::mutex mutex;
    const std::string name;
    //    friend StoreIterator<T>;
public:

    forceinline char getCtr(T *optr) {
        return ((ConcurrentElement<T>*)optr)->accessCounter;
    }

    forceinline void clearCtr(T *optr) {
        ((ConcurrentElement<T>*)optr)->accessCounter = 0;
    }

    forceinline bool compareSwap(T* optr, char& expected, char newval) {
        ConcurrentElement<T>* ptr = (ConcurrentElement<T>*)optr;
        return ptr->accessCounter.compare_exchange_strong(expected, newval);
    }

    forceinline char incr(T* optr, char v, int pos) {
        //        mutex.lock();
        assert(optr != nullptr);
        ConcurrentElement<T>* ptr = (ConcurrentElement<T>*)optr;
        //        std::cerr << ptr << " " << pos << " " << (int) v << "  " << (int) ptr->accessCounter << " " << std::this_thread::get_id() << std::endl;
        //        mutex.unlock();
        return ptr->accessCounter += v;
    }

    ConcurrentStore(size_t size, const std::string& n) : start(size), poolsize(size), name(n) {
        free = nullptr;
        currentPool = &start;
        currentArrayIndex = 0;

    }

    ~ConcurrentStore() {
        ConcurrentPool<T>* cur;
        currentPool = start.next;
        while (currentPool != nullptr) {
            cur = currentPool;
            currentPool = currentPool->next;
            delete cur;
        }
    }

    forceinline T* add() {
//        mutex.lock();
        ConcurrentElement<T>* ptr = free, *nextptr;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        do {
            if (ptr == nullptr) {
                if (currentArrayIndex == poolsize) {
                    throw std::runtime_error(name + " Store size exceeded");
                    ConcurrentPool<T>* newPool = new ConcurrentPool<T>(poolsize);
                    currentPool->next = newPool;
                    currentPool = newPool;
                    currentArrayIndex = 0;
                }
                ptr = currentPool->array + currentArrayIndex++;
                break;
            } else {
                nextptr = ptr->cell.next;
                if (free.compare_exchange_weak(ptr, nextptr))
                    break;
            }
        } while (true);
        auto pos = ptr - currentPool->array;
        assert(pos >= 0 && pos < currentArrayIndex);
        assert(ptr->isActive == false);
        ptr->isActive = true;
//        mutex.unlock();
        return (T*) ptr;
    }

    forceinline void remove(const T * optr) {
        if (optr == nullptr)
            return;
//        mutex.lock();
        optr->~T();
        ConcurrentElement<T>* ptr = (ConcurrentElement<T>*)optr;
        assert(ptr->isActive == true);
        auto pos = ptr - currentPool->array;
        assert(pos >= 0 && pos < currentArrayIndex);
        ptr->isActive = false;
        ptr->cell.next = free;
        assert(ptr->isActive == false);
        while (!free.compare_exchange_weak(ptr->cell.next, ptr));
//        mutex.unlock();
    }
};

#endif /* CONCSTORE_H */

