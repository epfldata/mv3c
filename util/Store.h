
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
#ifdef STORE_ENABLE

    Store(size_t size, const std::string& n, uint8_t numT) : start((numT + 1) * size), numThreads(numT + 1), poolsize(size), name(n) {
        freeLists = new Element<T>*[numThreads];
        currentArrayIndex = new size_t[numThreads];
        for (uint i = 0; i < numT + 1; ++i) {
            freeLists[i] = nullptr;
            currentArrayIndex[i] = 0;
        }

    }

    ~Store() {
        //        cerr << name << "about to  be deleted "<<endl;
        delete[] freeLists;
        delete[] currentArrayIndex;
        //        cerr << name << "deleted "<<endl;

        //        Pool<T>* cur;
        //        currentPool = start.next;
        //        while (currentPool != nullptr) {
        //            cur = currentPool;
        //            currentPool = currentPool->next;
        //            delete cur;
        //        }
    }
#else

    Store(size_t size, const std::string& n, uint8_t numT) : start(0), numThreads(numT + 1), poolsize(size), name(n) {
    }
#endif

    T* add(uint8_t tid) {
#ifndef STORE_ENABLE
        return (T*) malloc(sizeof (T));
#else
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
            freeLists[tid] = ptr->cell.next;

        }
        assert(ptr->isActive == false);
        ptr->isActive = true;
        return (T*) ptr;
#endif
    }

    void remove(const T* optr, uint8_t thread_id) {
        if (optr == nullptr)
            return;
#ifndef STORE_ENABLE
        free((void *) optr);
#else
        Element<T>* ptr = (Element<T>*)optr;
        //TO REMOVE:
        assert(thread_id != 0);
        assert(thread_id == ((ptr - start.array) / poolsize));
        assert(ptr->isActive);
        //        memset(&ptr->cell, 0xff, sizeof (ptr->cell));
        ptr->isActive = false;
        ptr->cell.next = freeLists[thread_id];
        freeLists[thread_id] = ptr;
#endif
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

