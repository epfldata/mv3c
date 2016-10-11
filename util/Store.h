
#ifndef STORE_H
#define STORE_H

#include <cstring>
#include <functional>
#include <iostream>
#include <cassert>

template <typename T>
struct Element;

template <typename T>
union Cell {
    T entry;
    Element<T>* next;

    Cell() {
        memset(this, 0, sizeof (Cell));
    }

    ~Cell() {
    }
};

template <typename T>
struct Element {
    Cell<T> cell;
    bool isActive;

    Element(Element<T>* next = nullptr) {
        isActive = false;
        cell.next = next;

    }
};

template <typename T>
struct Pool {
    Element<T> *array;
    Pool<T>* next;

    Pool(size_t size, Pool<T>* n = nullptr) {
        array = new Element<T>[size];
        next = n;
    }

    ~Pool() {
        delete[] array;
    }
};

template <typename T>
class Store {
public:
    Pool<T> start; //,*currentPool;
    const size_t poolsize;
    uint8_t numThreads;
    size_t *currentArrayIndex;
    Element<T>** freeLists;
    const std::string name;

    Store(size_t size, const std::string& n, uint8_t numT) : start((numT + 1) * size), numThreads(numT), poolsize(size), name(n) {
        freeLists = new Element<T>*[numT + 1];
        currentArrayIndex = new size_t[numT + 1];
        for (uint i = 0; i < numT; ++i) {
            freeLists[i] = nullptr;
            currentArrayIndex[i] = 0;
        }

    }

    ~Store() {
        //        Pool<T>* cur;
        //        currentPool = start.next;
        //        while (currentPool != nullptr) {
        //            cur = currentPool;
        //            currentPool = currentPool->next;
        //            delete cur;
        //        }
    }

    T* add(uint8_t tid) {

        Element<T>* ptr;
        if (freeLists[tid] == nullptr) {
            if (currentArrayIndex[tid] == poolsize) {
                cerr << name << "Store size exceeded" << endl;
                exit(-1);
                //                Pool<T>* newPool = new Pool<T>(poolsize);
                //                currentPool->next = newPool;
                //                currentPool = newPool;
                //                currentArrayIndex = 0;
            }
            ptr = start.array + (tid * poolsize + currentArrayIndex[tid]);
            currentArrayIndex[tid]++;
        } else {
            ptr = freeLists[tid];
            freeLists[tid] = freeLists[tid]->cell.next;
        }
        assert(ptr->isActive == false);
        ptr->isActive = true;
        return (T*) ptr;
    }

    void remove(const T* optr, uint8_t thread_id) {
        if (optr == nullptr)
            return;

        optr->~T();
        Element<T>* ptr = (Element<T>*)optr;
        //TO REMOVE:
        uint8_t tid = (ptr - start.array) / poolsize;
        assert(tid == thread_id); // or thread_id == GC_tid
        ptr->isActive = false;
        ptr->cell.next = freeLists[tid];
        freeLists[tid] = ptr;
    }

    //    void forEach(const std::function<void(T&) >& func) const {
    //        size_t i = 0;
    //        const Pool<T> *current = &start;
    //        while (current != currentPool || i != currentArrayIndex) {
    //            if (current->array[i].isActive) {
    //                func(current->array[i].cell.entry);
    //            }
    //
    //            if (i == poolsize) {
    //                i = 0;
    //                current = current->next;
    //            } else
    //                i++;
    //        }
    //    }

};

//template <typename T>
//class StoreIterator {
//    Store<T>& store;
//    const Pool<T> *current;
//    size_t i;
//
//public:
//
//    StoreIterator(Store<T>& st) : store(st), current(&st.start), i(0) {
//    }
//
//    T* get() {
//        while (true) {
//            if (current == store.currentPool && i == store.currentArrayIndex)
//                return nullptr;
//            if (current->array[i].isActive)
//                return &current->array[i].cell.entry;
//            if (i == store.poolsize) {
//                i = 0;
//                current = current->next;
//            } else
//                i++;
//        }
//    }
//
//    T* incr_get() {
//        if (i == store.poolsize) {
//            i = 0;
//            current = current->next;
//        } else
//            i++;
//        while (true) {
//            if (current == store.currentPool && i == store.currentArrayIndex)
//                return nullptr;
//            if (current->array[i].isActive)
//                return &current->array[i].cell.entry;
//            if (i == store.poolsize) {
//                i = 0;
//                current = current->next;
//            } else
//                i++;
//        }
//    }
//
//    void reset() {
//        current = &store.currentPool;
//        i = 0;
//    }
//};
#endif /* STORE_H */

