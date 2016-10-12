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

//#define CreateValUpdate(type, name, input)\
//  auto name##DV = DeltaVersion<type##Key, type##Val>::store.add();\
//  type##Val * name = &name##DV->val;\
//  memcpy(name, &input->val, sizeof(type##Val))

#define CreateValUpdate(type, name, input)\
  auto name##DV = const_cast<DeltaVersion<type##Key, type##Val> *>(input);\
  type##Val* name = &name##DV->val 


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
        V* val = &valDV->val;
        assert(val != &v);
        memcpy(val, &v, sizeof (V));
        valDV->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
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
            //            xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
            //            e->dv->val = v;
        } else {
            e = new(EntryType::store.add(xact->threadId)) Entry<K, V>(this, k);
            auto ret = primaryIndex.insert(k, e);
            if (ret) {
                dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, INSERT, parent);
                xact->undoBufferHead = dv;
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
        //        xact->undoBufferHead = new(dvStore.add()) DeltaVersion<K, V>(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, newVal, colsChanged, parent);
        //        dv->initialize(e, PTRtoTS(xact), xact->undoBufferHead, UPDATE, parent);
        assert(dv == e->dv);
        //        e->dv = dv;
        return OP_SUCCESS;
    }

    forceinline const DeltaVersion<K, V>* getReadOnly(Transaction *xact, const K& key) {
        Entry<K, V> *e;
        if (primaryIndex.find(key, e)) {
            assert(e->dv);
            return e->dv;
        } else {
            return nullptr;
        }
    }
};


#endif /* TABLE_H */

