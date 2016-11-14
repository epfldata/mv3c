#ifndef CONCCUCKOOSECONDARYINDEX_H
#define CONCCUCKOOSECONDARYINDEX_H
#include "SecondaryIndex.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <atomic>

template <typename K, typename V, typename P, typename Alloc>
struct ConcurrentCuckooSecondaryIndex : SecondaryIndex<K, V> {
    typedef Entry<K, V>* EntryPtrType;

    struct Container {
        EntryPtrType e;
        std::atomic<Container *> next;

        Container(EntryPtrType e) :
        e(e), next(nullptr) {
        }

        Container(Container *nxt) : e(nullptr), next(nxt) {

        }
    };
    typedef typename Alloc::template rebind<Container>::other ContainerPoolType;
    typedef typename Alloc::template rebind<std::pair<const P, Container *>>::other CuckooAllocType;
    static thread_local ContainerPoolType* store;
    cuckoohash_map<P, Container *, CityHasher<P>, std::equal_to<P>, CuckooAllocType> index;

    ConcurrentCuckooSecondaryIndex(size_t size = 100000) : index(size) {
    }

    void insert(EntryPtrType e, const V& val) override {
        P key(e, val);
        Container *newc = store->allocate(1, nullptr);
        new (newc) Container(e);
        Container *sentinel = store->allocate(1, nullptr);
        new(sentinel) Container(newc);
        auto updatefn = [newc, sentinel](Container* &c) {
            delete sentinel;
            Container *nxt = c->next;
            do {
                newc->next = nxt;
            } while (!c->next.compare_exchange_weak(nxt, newc));
        };
        index.upsert(key, updatefn, sentinel);
    }

    void erase(EntryPtrType e, const V& val) override {
        Container *sentinel, *cur;
        if (index.find(P(e, val), sentinel) && ((cur = sentinel->next) != nullptr)) {
            Container* prev = sentinel, *curNext = cur->next, *prevNext = cur;
            while (isMarked(curNext) || cur->e != e) {
                if (!isMarked(curNext)) {
                    if (prevNext != cur) {
                        prev->next.compare_exchange_strong(prevNext, cur);
                    }
                    prev = cur;
                    prevNext = curNext;
                    cur = curNext;
                } else {
                    cur = unmark(curNext);
                }
                if (!cur)
                    break;
                curNext = cur->next;
            }
            if (cur == nullptr) {
                //Element not present
                if (prevNext != cur) {
                    prev->next.compare_exchange_strong(prevNext, cur);
                }

            } else {
                while (!cur->next.compare_exchange_weak(curNext, mark(curNext)));

                //attempt to fix prev, if it fails, we dont care
                prev->next.compare_exchange_strong(prevNext, curNext);
            }
        } else {
            throw std::logic_error("Entry to be deleted not in Secondary Index");
        }

    }

    forceinline Container * slice(const P key) {
        Container *sentinel;
        if (index.find(key, sentinel)) {
            Container *first = sentinel->next, *cur = first, *curNext = cur->next;
            while (isMarked(curNext)) {
                cur = unmark(curNext);
                if (!cur)
                    break;
                curNext = cur->next;
            }
            sentinel->next.compare_exchange_strong(first, cur);
            return cur;
        } else {
            return nullptr;
        }
    }
};

template <typename K, typename V, typename P, typename A>
thread_local typename ConcurrentCuckooSecondaryIndex<K, V, P, A>::ContainerPoolType * ConcurrentCuckooSecondaryIndex<K, V, P, A>::store;

#define TraverseSlice(name, type, txnptr)\
    auto name##Cur = name->next, name##CurNext , name##Prev = name, name##PrevNext = name##Cur;\
    auto name##Entry = name->e;\
    auto name##DV = type##Table->getCorrectDV(txnptr, name##Entry);\
    auto name##Val = name##DV->val;\
    if(name##Cur != nullptr){\
      name##curNext = name##Cur->next;\
      while(isMarked(name##CurNext)){\
        name##Cur = unmark(name##CurNext);\
        if(!name##Cur)\
            break;\
        name##CurNext = name##Cur->next;\
      }\
    }\
    name##Prev->next.compare_exchange_strong(name##PrevNext, name##Cur);\
    name = name##Cur


#endif /* CUCKOOSECONDARYINDEX_H */

