#ifndef TABLE_H
#define TABLE_H
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <string>

#define CreateValInsert(type, name, args...)\
  auto name##DV = DeltaVersion<type##Key, type##Val>::store.add(xact.threadId);\
  new (name##DV) DeltaVersion<type##Key, type##Val>();\
  type##Val * name = &name##DV->val;\
  new (name) type##Val(args)

#define CreateValUpdate(type, name, input)\
  auto name##DV = DeltaVersion<type##Key, type##Val>::store.add(xact.threadId);\
  new (name##DV) DeltaVersion<type##Key, type##Val>();\
  type##Val * name = &name##DV->val;\
  memcpy(name, &input->val, sizeof(type##Val))

#define MakeRecord(name)  name##DV

template <typename K, typename V>
struct Table {
    cuckoohash_map<K, Entry<K, V>*, CityHasher<K>> primaryIndex;
    typedef Entry<K, V> EntryType;
    typedef DeltaVersion<K, V> DVType;

    Table(size_t s) : primaryIndex(s) {
    }

    forceinline OperationReturnStatus insertVal(Transaction *xact, const K& k, const V& v, PRED* parent = nullptr) {
        Entry<K, V>* e;
        assert(xact->threadId == 0);
        e = new(EntryType::store.add(0)) Entry<K, V>(this, k);
        auto ret = primaryIndex.insert(k, e);
        auto valDV = DVType::store.add(0);
        new (valDV) DVType();
        V* val = &valDV->val;
        assert(val != &v);
        memcpy(val, &v, sizeof (V));
        valDV->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
        e->dv = valDV;
        xact->undoBufferHead = valDV;
        return OP_SUCCESS;
    }
    //
    //    forceinline OperationReturnStatus update(Transaction *xact, Entry<K, V> *e, const V& newVal, PRED* parent = nullptr, const bool allowWW = false, const col_type& colsChanged = col_type(-1)) {
    //        //        xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, newVal, colsChanged, parent);
    //        e->dv->val = newVal;
    //        return OP_SUCCESS;
    //    }

    forceinline OperationReturnStatus insert(Transaction *xact, const K& k, DeltaVersion<K, V> * dv, PRED* parent = nullptr) {
        Entry<K, V>* e;
        if (primaryIndex.find(k, e)) {
            DVType::store.remove(dv, xact->threadId);
            return DUPLICATE_KEY; //we do not have delete and then insert case
            //            xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
            //            e->dv->val = v;
        } else {
            e = new(EntryType::store.add(xact->threadId)) Entry<K, V>(this, k);
            auto ret = primaryIndex.insert(k, e);
            if (ret) {
                dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
                xact->undoBufferHead = dv;
                e->dv.store(dv);
                return OP_SUCCESS;
            } else {
                DVType::store.remove(dv, xact->threadId);
                EntryType::store.remove(e, xact->threadId);
                return DUPLICATE_KEY;
            }

        }
        return OP_SUCCESS;
    }

    forceinline OperationReturnStatus update(Transaction *xact, Entry<K, V> *e, DeltaVersion<K, V> * dv, PRED* parent = nullptr, const bool allowWW = false, const col_type& colsChanged = col_type(-1)) {

        dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, parent, colsChanged);
        //When we have a table that can have both WW and non-WW updates, mergeWithPrev or moveNextToCOmmitted must be called on all dvs of table
        DVType * old = e->dv.load();
        dv->olderVersion = old;
#if (ALLOW_WW)
        if (allowWW) {
            dv->isWWversion = true;
            while (!e->dv.compare_exchange_weak(old, dv)) {
                dv->olderVersion.store(old);
            }
        } else {
#endif
            if (!isVisible(xact, old)) {//can cause problems if old is reclaimed by some thread
                DVType::store.remove(dv, xact->threadId);
                return WW_VALUE;
            }

            if (!e->dv.compare_exchange_strong(old, dv)) {
                DVType::store.remove(dv, xact->threadId);
                return WW_VALUE;
            }
#if (ALLOW_WW)
        }
#endif
        xact->undoBufferHead = dv;

        return OP_SUCCESS;
    }

    forceinline const DeltaVersion<K, V>* getReadOnly(Transaction *xact, const K& key) {
        Entry<K, V> *e;
        if (primaryIndex.find(key, e)) {

            return getCorrectDV(xact, e);
        } else {
            return nullptr;
        }
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
            dv = (DVType *) dv->olderVersion;
        }
        assert(false);
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

