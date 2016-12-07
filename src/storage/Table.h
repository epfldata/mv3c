#ifndef TABLE_H
#define TABLE_H
#include "util/types.h"
#include "transaction/Transaction.h"
#include "storage/DeltaVersion.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <string>
#include "index/UnordereredIndex.h"
#include "index/MultiMapIndexMT.h"
#include "index/CuckooSecondaryIndex.h"
#include "index/ConcurrentCuckooSecondaryIndex.h"

//Creates a new deltaversion with the value constructed in place
#define CreateValInsert(type, name, args...)\
  auto name##DV = DeltaVersion<type##Key, type##Val>::store.add(xact.threadId);\
  new (name##DV) DeltaVersion<type##Key, type##Val>();\
  type##Val * name = &name##DV->val;\
  new (name) type##Val(args)

//Creates a new delatversion with the value copied from another version.
#define CreateValUpdate(type, name, input)\
  auto name##DV = DeltaVersion<type##Key, type##Val>::store.add(xact.threadId);\
  new (name##DV) DeltaVersion<type##Key, type##Val>();\
  type##Val * name = &name##DV->val;\
  memcpy(name, &input->val, sizeof(type##Val))

//hide version from user programmer level
#define MakeRecord(name)  name##DV

template <typename K, typename V>
struct Table {
#if !CUCKOO
    UnorderedIndex<K, Entry<K, V>*, CityHasher<K>> primaryIndex;
#else
    cuckoohash_map<K, Entry<K, V>*, CityHasher<K>> primaryIndex;
#endif

    typedef Entry<K, V> EntryType;
    typedef DeltaVersion<K, V> DVType;
    SecondaryIndex<K, V> ** secondaryIndexes; //Array of pointers to secondary indexes
    uint8_t numIndexes;

    Table(size_t s) : primaryIndex(s) {
        secondaryIndexes = nullptr;
        numIndexes = 0;
    }

    //Insert operation for backwards compatibility with data loading functions
    //User programs should use the "insert" function which accepts version.

    forceinline OperationReturnStatus insertVal(Transaction *xact, const K& k, const V& v, PRED* parent = nullptr) {
        Entry<K, V>* e;
        assert(xact->threadId == 0); //only for main thread 

        //get new entry and  add it to primary and secondary indexes
        e = new(EntryType::store.add(0)) Entry<K, V>(this, k);
        primaryIndex.insert(k, e);
        for (uint8_t i = 0; i < numIndexes; ++i) {
            secondaryIndexes[i]->insert(e, v);
        }

        //create a new version and copy value into it
        auto valDV = DVType::store.add(0);
        new (valDV) DVType();
        V* val = &valDV->val;
        assert(val != &v);
        memcpy(val, &v, sizeof (V));
        valDV->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);

        e->dv = valDV; //make it head of version chain
        xact->undoBufferHead = valDV; //make it head of undobuffer
        return OP_SUCCESS;
    }

    //Insert function to be used in user programs. accepts a (hidden) deltaversion directly

    forceinline OperationReturnStatus insert(Transaction *xact, const K& k, DeltaVersion<K, V> * dv, PRED* parent = nullptr) {
        Entry<K, V>* e;
        //Check if it  already exists.
        //We do not have a case where an existing key is deleted and then inserted back.
        //Therefore, if the entry exists in index, then it is an error
        //If there are scenarios involving delete and then insert, then there 
        //   should be additional check that the visible version is not DELETE before returning the error
        if (primaryIndex.find(k, e)) {
            DVType::store.remove(dv, xact->threadId);
            return DUPLICATE_KEY;

        } else {
            //Create new entry
            e = new(EntryType::store.add(xact->threadId)) Entry<K, V>(this, k);
            auto ret = primaryIndex.insert(k, e);
            if (ret) { //attempt to insert successful
                //add to secondary indexes
                for (uint8_t i = 0; i < numIndexes; ++i) {
                    secondaryIndexes[i]->insert(e, dv->val);
                }
                //initialize other fields of the DV.
                dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);

                xact->undoBufferHead = dv; //add to undo buffer
                e->dv.store(dv); //add to version chain
                return OP_SUCCESS;
            } else { //another concurrent transaction inserted first
                DVType::store.remove(dv, xact->threadId);
                EntryType::store.remove(e, xact->threadId);
                return DUPLICATE_KEY;
            }

        }
        return OP_SUCCESS;
    }
    //Function to update an existing record.
    //Accepts a (hidden) delta version
    //Accepts columns that are modified
    //The flag allowWW indicates whether the operation should be allowed to continue if an uncommitted version already exists for this record

    //TODO:  Currently, if a table has atleast one update that allows ww conflicts, ALL updates on that table should allow it as well.
    //This is because, if WW is allowed on the table, some extra operations need to be performed during commit, even if a particular version did not allow ww.
    //When we have a table that can have both WW and non-WW updates, mergeWithPrev or moveNextToCOmmitted must be called on all dvs of table
    //Right now, these operations are done after checking the per-version isWWversion flag
    //In future, this should be rectified by adding a table-wide state whether the table supports ww as a whole.

    forceinline OperationReturnStatus update(Transaction *xact, Entry<K, V> *e, DeltaVersion<K, V> * dv, PRED* parent = nullptr, const bool allowWW = false, const col_type& colsChanged = col_type(-1)) {

        dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, parent, colsChanged);

        DVType * old = e->dv.load(); //get the current head of version chain
        dv->olderVersion = old;
#if (ALLOW_WW)
        if (allowWW) {
            //if WW is allowed, we loop until we successfully insert dv as head of version chain
            dv->isWWversion = true;
            while (!e->dv.compare_exchange_weak(old, dv)) {
                dv->olderVersion.store(old);
            }
        } else {
#endif
            //check if "old" is visible to this transaction. 
            // If it is not visible (ie it is uncommitted), since ww is not allowed, error is returned
            if (!isVisible(xact, old)) {
                //can cause problems if old is reclaimed by some thread. 
                //dv is not yet visible to anyone, so it can be safely garbage collected.
                //However, "old" must not be garbage collected by some other thread
                DVType::store.remove(dv, xact->threadId);
                return WW_VALUE;
            }

            if (!e->dv.compare_exchange_strong(old, dv)) {
                //C&S failed because head was changed by concurrent transaction.
                DVType::store.remove(dv, xact->threadId);
                return WW_VALUE;
            }
#if (ALLOW_WW)
        }
#endif
        xact->undoBufferHead = dv;

        return OP_SUCCESS;
    }

    //get the value without any predicate guarding it

    forceinline const DeltaVersion<K, V>* getReadOnly(Transaction *xact, const K& key) {
        Entry<K, V> *e;
        if (primaryIndex.find(key, e)) {
            return getCorrectDV(xact, e);
        } else {
            return nullptr;
        }
    }

#ifdef CC_SI

    template<typename P>
    forceinline uint sliceReadOnly(Transaction *xact, const P& key, uint8_t idx, DVType** results) {
        ConcurrentCuckooSecondaryIndex<K, V, P>* index = (ConcurrentCuckooSecondaryIndex<K, V, P>*)secondaryIndexes[idx];
        auto result = index->slice(key);
        uint count = 0;
        while (result != nullptr) {
            TraverseSlice(result, this, xact);
            if (resultDV != nullptr) {
                results[count++] = resultDV;
            }
        }
        return count;
    }
    //

    template<typename P>
    forceinline DVType* getFirst(Transaction *xact, const P& key, uint8_t idx) {
        ConcurrentCuckooSecondaryIndex<K, V, P>* index = (ConcurrentCuckooSecondaryIndex<K, V, P>*)secondaryIndexes[idx];
        auto result = index->slice(key);
        TraverseSlice(result, this, xact);
        return resultDV;
    }
#endif
#ifdef CUCKOO_SI

    template<typename P>
    forceinline uint sliceReadOnly(Transaction *xact, const P& key, uint8_t idx, DVType** results) {
        CuckooSecondaryIndex<K, V, P>* index = (CuckooSecondaryIndex<K, V, P>*)secondaryIndexes[idx];
        auto range = index->slice(key);
        if (range == nullptr)
            return 0;
        range->lock.AcquireReadLock();
        uint count = 0;
        for (auto it = range->bucket.begin(); it != range->bucket.end(); ++it) {
            EntryType* e = *it;
            assert(count < 15);
            DVType *dv = getCorrectDV(xact, e);
            if (dv != nullptr) {
                results[count++] = dv;
            }
        }
        range->lock.ReleaseReadLock();
        return count;
    }
    //

    template<typename P>
    forceinline DVType* getFirst(Transaction *xact, const P& key, uint8_t idx) {
        CuckooSecondaryIndex<K, V, P>* index = (CuckooSecondaryIndex<K, V, P>*)secondaryIndexes[idx];
        auto range = index->slice(key);
        if (range == nullptr)
            return nullptr;
        range->lock.AcquireReadLock();
        for (auto it = range->bucket.begin(); it != range->bucket.end(); ++it) {
            EntryType* e = *it;
            DVType *dv = getCorrectDV(xact, e);
            if (dv != nullptr) {
                range->lock.ReleaseReadLock();
                return dv;
            }
        }
        range->lock.ReleaseReadLock();
        return nullptr;
    }
#endif
#ifdef MM_SI

    template<typename P>
    forceinline uint sliceReadOnly(Transaction *xact, const P& key, uint8_t idx, DVType** results) {
        MultiMapIndexMT<K, V, P>* index = (MultiMapIndexMT<K, V, P>*)secondaryIndexes[idx];
        auto range = index->slice(key);

        uint count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            EntryType* e = it->second;
            assert(count < 15);
            DVType *dv = getCorrectDV(xact, e);
            if (dv != nullptr) {
                results[count++] = dv;
            }
        }
        index->lock.ReleaseReadLock();
        return count;
    }
    //

    template<typename P>
    forceinline DVType* getFirst(Transaction *xact, const P& key, uint8_t idx) {
        MultiMapIndexMT<K, V, P>* index = (MultiMapIndexMT<K, V, P>*)secondaryIndexes[idx];
        EntryType* e = index->get(key);
        if (!e) {
            return nullptr;
        }
        //Assumes there is visible version;
        return getCorrectDV(xact, e);
    }
#endif

    forceinline std::vector<std::pair<const K*, const V*>> getAll(Transaction *xact) {
#if CUCKOO
        auto lockedT = primaryIndex.lock_table();
#else
        primaryIndex.lock_.AcquireWriteLock();
        auto& lockedT = primaryIndex.index;
        primaryIndex.lock_.ReleaseWriteLock();
#endif
        std::vector<std::pair<const K*, const V*>> results;
        for (auto it = lockedT.cbegin(); it != lockedT.cend(); ++it) {
            auto e = it->second;
            auto dv = getCorrectDV(xact, e);
            if (dv != nullptr)
                results.push_back({&e->key, &dv->val});
        }
        return results;
    }

    forceinline bool isVisible(Transaction *xact, DVType *dv) {
        timestamp ts = dv->xactId;
        if (isTempTS(ts)) {
            Transaction* t = TStoPTR(ts);
            if (t == xact || t->commitTS < xact->startTS) //What if commitTS == startTS ?
                return true;
        } else { //transaction that wrote DV is definitely committed   dv->xactId == t->commitTS
            if (ts < xact->startTS)
                return true;
        }
        return false;
    }
#if (!ALLOW_WW)

    forceinline DVType* getCorrectDV(Transaction *xact, EntryType *e) {
        DVType* dv = e->dv.load(); //dv != nullptr as there will always be one version

        while (dv != nullptr) {
            if (isVisible(xact, dv))
                return dv;
            dv = (DVType *) dv->olderVersion.load();
        }
        //        assert(false);
        return nullptr;
    }
#else

    forceinline DVType* getCorrectDV(Transaction *xact, EntryType *e) {
        DELTA* dv = e->dv.load(); //dv != nullptr as there will always be one version
        DELTA* dvOld = dv->olderVersion;
        DELTA *old = nullptr;
        DELTA *oldOld = dv;
        while (isMarked(dvOld) || !isVisible(xact, (DVType *) dv)) {
            if (!isMarked(dvOld)) {
                if (oldOld != dv) {
                    if (old == nullptr) {
                        DVType * oldOldKV = (DVType *) oldOld;
                        e->dv.compare_exchange_strong(oldOldKV, (DVType *) dv); //dont care abt result
                    } else
                        old->olderVersion.compare_exchange_strong(oldOld, dv); //dont care abt result
                }
                old = dv;
                oldOld = dvOld;
                dv = dvOld;
            } else {
                dv = unmark(dvOld);
            }
            if (!dv)
                break;
            dvOld = dv->olderVersion.load();
        }
        if (oldOld != dv) {
            if (old == nullptr) {
                DVType * oldOldKV = (DVType *) oldOld;
                e->dv.compare_exchange_strong(oldOldKV, (DVType *) dv); //dont care abt result
            } else
                old->olderVersion.compare_exchange_strong(oldOld, dv); //dont care abt result
        }
        return (DVType *) dv;
    }
#endif
};


#endif /* TABLE_H */

