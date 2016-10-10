#ifndef PREDICATE_HPP
#define PREDICATE_HPP
#include "Predicate.h"
#include "DeltaVersion.h"

template <typename K, typename V>
struct GetPred : public PRED {
    typedef const DeltaVersion<K, V>* DvPtr;
    typedef TransactionReturnStatus(*FuncType)(Program *p,  DvPtr, uint cs);
    Table<K, V> * tbl;
    FuncType andThenFn;
    const K key;
#ifdef ATTRIB_LEVEL
    const col_type colsToBeChecked;

    GetPred(Table<K, V>* id, Transaction* n, const K& k, PRED* par = nullptr, const col_type& cols = col_type(-1), bool shouldValidate = true) : PRED(n, par, shouldValidate, GET), key(k), tbl(id), colsToBeChecked(cols) {
    }
#else

    GetPred(Table<K, V>* id, Transaction* n, const K& k, PRED* par = nullptr, const col_type& cols = col_type(-1), bool shouldValidate = true) : PRED(n, par, shouldValidate, GET), key(k), tbl(id) {
    }
#endif
    GetPred(){}

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
        if (dv->entry->tbl != tbl)
            return false;
#ifdef ATTRIB_LEVEL
        if ((colsToBeChecked & dv->cols).none())
            return false;
#endif  
        return key == dv->entry->key;
    }

    forceinline DvPtr evaluateInternal(Transaction *xact) {
        Entry<K, V> *e;
        isInvalid = false;
        if (tbl->primaryIndex.find(key, e)) {
            return e->dv;
        } else {
            return nullptr;
        }
    }

};
//P2 , P1 and then bundle

//struct PredicateBundle2 : PRED {
//    PRED * const p1;
//    PRED * const p2;
//    bool flag1, flag2;
//  
//    PredicateBundle2(Transaction *xact, PRED* P1, PRED* P2, PRED* par = nullptr, bool shouldValidate = true) : PRED(xact, par, shouldValidate, BUNDLE2), p1(P1), p2(P2) {
//        p1->shouldValidate = false;
//        p2->shouldValidate = false;
//        isEvaluated = true;
//        isInvalid = false;
//        flag1 = false;
//        flag2 = false;
//    }
//
//    virtual bool matchesAny(Transaction *t) {
//        //TODO: Check for any validaton order assumptions
//        //SBJ: Since we have only GET PREDICATES and it doesnt have to go through all transactions, bundle too doesnt have to go through all transactions
//        if (flag1 && flag2)
//            return true;
//        bool flag = false;
//        if (!flag1 && p1->matchesAny(t)) { //No need to validate against p2 if it has been already failed validation (Assuming no preds that "fix" result)
//            p1->isInvalid = true;
//            flag = true;
//            flag1 = true;
//        }
//        if (!flag2 && p2->matchesAny(t)) {
//            p2->isInvalid = true;
//            flag = true;
//            flag2 = true;
//        }
//        //Need to set this here as there is no call to "evaluate" which sets it to false
//        //Once set, it should not be set to false, even if flag returns false for a particular transaction
//        if (!isInvalid && flag)
//            isInvalid = true;
//        return flag;
//    }
//
//
//};
////P3, P2 , P1 and then bundle
//
//struct PredicateBundle3 : PRED {
//    PRED * const p1;
//    PRED * const p2;
//    PRED * const p3;
//    bool flag1, flag2, flag3;

//
//    PredicateBundle3(Transaction *xact, PRED* P1, PRED* P2, PRED* P3, PRED* par = nullptr, bool shouldValidate = true) : PRED(xact, par, shouldValidate, BUNDLE3), p1(P1), p2(P2), p3(P3) {
//        p1->shouldValidate = false;
//        p2->shouldValidate = false;
//        p3->shouldValidate = false;
//        isEvaluated = true;
//        isInvalid = false;
//        flag1 = false;
//        flag2 = false;
//        flag3 = false;
//    }
//
//    virtual bool matchesAny(Transaction *t) {
//        //TODO: Check for any validaton order assumptions
//        //SBJ: Since we have only GET PREDICATES and it doesnt have to go through all transactions, bundle too doesnt have to go through all transactions
//        if (flag1 && flag2 && flag3)
//            return true;
//        bool flag = false;
//        if (!flag1 && p1->matchesAny(t)) { //No need to validate against p1 if it has been already failed validation (Assuming no preds that "fix" result)
//            p1->isInvalid = true;
//            flag = true;
//            flag1 = true;
//        }
//        if (!flag2 && p2->matchesAny(t)) {
//            p2->isInvalid = true;
//            flag = true;
//            flag2 = true;
//        }
//        if (!flag3 && p3->matchesAny(t)) {
//            p3->isInvalid = true;
//            flag = true;
//            flag3 = true;
//        }
//        //Need to set this here as there is no call to "evaluate" which sets it to false
//        //Once set, it should not be set to false, even if flag returns false for a particular transaction
//        if (!isInvalid && flag)
//            isInvalid = true;
//        return flag;
//    }
//
//};

#endif /* PREDICATE_HPP */

