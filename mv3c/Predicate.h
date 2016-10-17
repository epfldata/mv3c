#include "types.h"
#include "Transaction.h"
#include "Entry.h"
#ifndef PREDICATE_H
#define PREDICATE_H

enum PredicateID {
    BUNDLE3, BUNDLE2, GET, UNINITIALIZED
};

struct PRED {
    const PredicateID predType;
    PRED* firstChild, *nextChild;
    DELTA* DVsInClosureHead;
    uint closureState;
    bool isInvalid, isEvaluated;
    virtual bool matchesAny(Transaction *t) = 0;

    PRED() : predType(UNINITIALIZED) {
    }

    PRED(Transaction *x, PRED* par, PredicateID pid) : predType(pid) {
        isInvalid = true;
        isEvaluated = false;
        //        shouldValidate = validate; always true
        closureState = -1;
        firstChild = nullptr;
        DVsInClosureHead = nullptr;
        if (par == nullptr) {
            nextChild = x->predicateHead;
            x->predicateHead = this;
        } else {
            nextChild = par->firstChild;
            par->firstChild = this;
        }
    }
#if !OMVCC
    virtual TransactionReturnStatus compensateAndExecute(Transaction *xact, Program *state) = 0;
#endif
    virtual ~PRED() {
    }
};


#endif /* PREDICATE_H */

