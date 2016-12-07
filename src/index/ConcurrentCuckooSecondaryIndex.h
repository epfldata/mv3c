#ifndef CONCCUCKOOSECONDARYINDEX_H
#define CONCCUCKOOSECONDARYINDEX_H
#include "index/SecondaryIndex.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <atomic>

template <typename K, typename V, typename P, typename Alloc = std::allocator<char>>
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
    // Inserts an entry into the secondary index.
    //Uses cuckoo hashmap as backend
    //Inserts the entry if it does not exist already in cuckoo index, otherwise if it exists,updates it
    //Cuckoo points towards a sentinel to protect against concurrent insertions/deletions

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

    //Marks an entry for removal from concurrent list
    //Will be actually removed only by a later traversal
    //Removes other nodes marked for removal during its traversal
    // Loop focus on cur. prev is before cur, curNext is afterCur. prevNext is node after prev (ideally, cur)
    // ..... prev -> prevNext (....) cur -> curNext ...
    // a node is said to be marked for removal if its next pointer is marked.
    //For example, to see if cur is deleted, we check isMarked(curNext)

    void erase(EntryPtrType e, const V& val) override {
        Container *sentinel, *cur;
        if (index.find(P(e, val), sentinel) && ((cur = sentinel->next) != nullptr)) {
            Container* prev = sentinel, *curNext = cur->next, *prevNext = cur;
            //while cur is a deleted node or not what we are looking for
            while (isMarked(curNext) || cur->e != e) {
                //if cur is not a deleted node, we can remove everything between prev and cur
                if (!isMarked(curNext)) {
                    if (prevNext != cur) { //if prevNext is same as cur, there is nothing in between
                        prev->next.compare_exchange_strong(prevNext, cur);
                    }
                    //cur becomes prev and the new value of cur comes from curNext                    
                    prev = cur;
                    prevNext = curNext;
                    cur = curNext;
                } else {
                    //curNext is marked, so we get the unmarked version
                    cur = unmark(curNext);
                }
                if (!cur) //stop at nullptr
                    break;
                curNext = cur->next;
            }
            if (cur == nullptr) {
                //Element not present
                //Remove any marked nodes from prev until here
                if (prevNext != cur) {
                    prev->next.compare_exchange_strong(prevNext, cur);
                }

            } else {
                //mark cur for deletion by marking its next pointer 
                while (!cur->next.compare_exchange_weak(curNext, mark(curNext)));

                //attempt to fix prev, if it fails, we dont care
                prev->next.compare_exchange_strong(prevNext, curNext);
            }
        } else {
            throw std::logic_error("Entry to be deleted not in Secondary Index");
        }

    }

    //Returns the first valid node in the list stored under given partial key

    forceinline Container * slice(const P key) {
        Container *sentinel;
        if (index.find(key, sentinel)) {
            Container *first = sentinel->next, *cur = first, *curNext = cur->next;
            //Skip all deleted nodes and remove them
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

//Macro to go to the next valid node in the list. NOT TESTED
#define TraverseSlice(name, tblptr, txnptr)\
    auto name##Cur = name->next.load(), name##CurNext = name , name##Prev = name, name##PrevNext = name##Cur;\
    auto name##Entry = name->e;\
    auto name##DV = tblptr->getCorrectDV(txnptr, name##Entry);\
    auto name##Val = name##DV->val;\
    if(name##Cur != nullptr){\
      name##CurNext = name##Cur->next;\
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

