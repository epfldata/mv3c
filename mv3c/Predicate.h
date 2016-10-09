#include "types.h"
#include "Transaction.h"
#include "Entry.h"
#ifndef PREDICATE_H
#define PREDICATE_H

struct ClosureState {
    virtual void free() = 0;
};

enum PredicateID {
    BUNDLE3, BUNDLE2, GET
};

struct PRED {
    const PredicateID predType;
    PRED* child, *next;
    DELTA* DVsInClosureHead;
    ClosureState* closureState;
    bool isInvalid, shouldValidate, isEvaluated;
    virtual bool matchesAny(Transaction *t) = 0;

    PRED(Transaction *x, PRED* par, bool validate, PredicateID pid) : predType(pid) {
        isInvalid = true;
        isEvaluated = false;
        shouldValidate = validate;
        closureState = nullptr;
        child = nullptr;
        DVsInClosureHead = nullptr;
        if (par == nullptr) {
            next = x->predicateHead;
            x->predicateHead = this;
        } else {
            next = par->child;
            par->child = this;
        }
    }

    virtual ~PRED() {
        if (closureState)
            closureState->free();
    }
    virtual void free() = 0;
};


#endif /* PREDICATE_H */

