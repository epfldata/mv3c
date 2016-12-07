#include "util/types.h"
#include "transaction/Transaction.h"
#include "storage/Entry.h"
#ifndef PREDICATE_H
#define PREDICATE_H

enum PredicateID {
    BUNDLE3, BUNDLE2, GET, SLICE, UNINITIALIZED
};

struct PRED {
    const PredicateID predType;
    PRED* firstChild, *nextChild; //predicate graph represented in first child - sibling form
    DELTA* DVsInClosureHead; //head of list of DVs in its clousure
    uint closureState; //used to denote some counter to indicate the predicate when done inside a loop
    bool isInvalid, isEvaluated; //isInvalid means that the result has to be repaired. If the predicate is evaluated once, then one doesnt have to assign closure etc again.
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
        if (par == nullptr) { //Adds to child list of parent, or else to list of root predicates of transaction
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

