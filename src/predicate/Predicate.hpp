#ifndef PREDICATE_HPP
#define PREDICATE_HPP
#include "predicate/Predicate.h"
#include "storage/DeltaVersion.h"

//Primary key look up. Returns a const pointer to the version

template <typename K, typename V>
struct GetPred : public PRED {
    typedef const DeltaVersion<K, V>* DvPtr;
    typedef TransactionReturnStatus(*FuncType)(Program *p, DvPtr, uint cs);
    Table<K, V> * tbl;
    FuncType andThenFn;
    const K key;
#ifdef ATTRIB_LEVEL
    const col_type colsToBeChecked;

    GetPred(Table<K, V>* id, Transaction* n, const K& k, PRED* par = nullptr, const col_type& cols = col_type(-1)) : PRED(n, par, GET), key(k), tbl(id), colsToBeChecked(cols) {
    }
#else

    GetPred(Table<K, V>* id, Transaction* n, const K& k, PRED* par = nullptr, const col_type& cols = col_type(-1)) : PRED(n, par, GET), key(k), tbl(id) {
    }
#endif

    GetPred() {
    }

    //Check if predicate matches against any version created by t. A match is found if the key used by the predicate and that in the version are identical

    virtual bool matchesAny(Transaction *t) {
        if (isInvalid) //If the predicate is already invalid, there is no need to validate it against other transactions.
            return true;
        auto dv = t->undoBufferHead;
        while (dv != nullptr) { //Match predicate against every version created by t
            if (matches(dv))
                return true;
            dv = dv->nextInUndoBuffer;
        }
        return false;
    }

    forceinline DvPtr evaluateAndExecute(Transaction *xact, FuncType fn, uint cs = -1) {
        if (!isEvaluated) {
            isEvaluated = true;
            andThenFn = fn;
            closureState = cs;
        }
        //Need to re-initialize to nullptr if called during repair phase
        DVsInClosureHead = nullptr;
        return evaluateInternal(xact);
    }
private:

    forceinline bool matches(DELTA * d) {
        auto dv = (DvPtr) d;
        if (dv->entry->tbl != tbl) //Not same table
            return false;
#ifdef ATTRIB_LEVEL
        if ((colsToBeChecked & dv->cols).none()) //columns updated are not the ones that this predicate reads
            return false;
#endif  
        return key == dv->entry->key;
    }

    forceinline DvPtr evaluateInternal(Transaction *xact) {
        Entry<K, V> *e;
        isInvalid = false;
        if (tbl->primaryIndex.find(key, e)) {
            assert(e->dv);
            assert(e->key == key);
            return tbl->getCorrectDV(xact, e);
        } else {
            assert(false);
            return nullptr;
        }
    }
#if !OMVCC

    TransactionReturnStatus compensateAndExecute(Transaction* xact, Program* state) override {
        DvPtr result = evaluateInternal(xact);
        return andThenFn(state, result, closureState);
    }
#endif
};

//Slice pred is used for secondary key retrieval.  Returns unordered_set of const version pointers. 
//TODO:  Return type can be more efficient.
//TODO: Implemented for Cuckoo SI. Concurrent Cuckoo SI implementation not added.

template <typename K, typename V, typename P>
struct SlicePred : public PRED {
    typedef std::unordered_set<const DeltaVersion<K, V>*> ResultType;
    typedef TransactionReturnStatus(*FuncType)(Program *p, const ResultType&, uint cs);
    Table<K, V> * tbl;
    FuncType andThenFn;
    const P partialKey;
    const uint8_t indexNum;

#ifdef ATTRIB_LEVEL
    const col_type colsToBeChecked;

    SlicePred(Table<K, V>* tbl, Transaction *x, uint8_t idx, const P& pk, PRED* par = nullptr, const col_type& cols = col_type(-1)) : PRED(x, par, SLICE), tbl(tbl), partialKey(pk), indexNum(idx), colsToBeChecked(cols) {
    }
#else

    SlicePred(Table<K, V>* tbl, Transaction *x, uint8_t idx, const P& pk, PRED* par = nullptr, const col_type& cols = col_type(-1)) : PRED(x, par, SLICE), tbl(tbl), partialKey(pk), indexNum(idx) {
    }
#endif

    SlicePred() : indexNum(-1) {

    }
#ifdef CUCKOO_SI

    ResultType evaluateInternal(Transaction *xact) {
        CuckooSecondaryIndex<K, V, P>* index = (CuckooSecondaryIndex<K, V, P>*)tbl->secondaryIndexes[indexNum];
        auto range = index->slice(partialKey);
        ResultType results;
        isInvalid = false;
        if (range == nullptr)
            return results;
        range->lock.AcquireReadLock();
        uint count = 0;
        for (auto it = range->bucket.begin(); it != range->bucket.end(); ++it) {
            Entry<K, V>* e = *it;
            DeltaVersion<K, V> *dv = tbl->getCorrectDV(xact, e);
            if (dv != nullptr) {
                results.insert(dv);
                count++;
            }
        }
        range->lock.ReleaseReadLock();
        return results;
    }
#else

    ResultType evaluateInternal(Transaction *xact) {
        throw std::logic_error("not implemented for the case without Cuckoo SI");
    }
#endif

    ResultType evaluateAndExecute(Transaction *xact, const FuncType fn, uint cs = -1) {
        if (!isEvaluated) {
            andThenFn = fn;
            isEvaluated = true;
            closureState = cs;
        }
        DVsInClosureHead = nullptr;
        return evaluateInternal(xact);
    }
#if !OMVCC

    TransactionReturnStatus compensateAndExecute(Transaction* xact, Program* state) override {
        auto res = evaluateInternal(xact);
        return andThenFn(state, res, closureState);
    }
#endif
    //Predicate matches if its partial key is same as that constructed from version

    virtual bool matchesAny(Transaction *t) {
        if (isInvalid)
            return true;
        auto dv = t->undoBufferHead;
        while (dv != nullptr) {
            if (matches(dv))
                return true;
            dv = dv->nextInUndoBuffer;
        }
        return false;
    }

    forceinline bool matches(DELTA * d) {
        auto dv = (DeltaVersion<K, V>*) d;
        if (dv->entry->tbl != tbl)
            return false;
#ifdef ATTRIB_LEVEL
        if ((colsToBeChecked & dv->cols).none())
            return false;
#endif  
        //TODO: assumes indexed col in value doesnt change. If it does, need to check against before and after versions
        return partialKey == P(dv->entry, dv->val);
    }

};
#endif /* PREDICATE_HPP */

