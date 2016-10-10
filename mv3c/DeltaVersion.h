#ifndef VERSION_H
#define VERSION_H
#include "types.h"
#include "Predicate.h"
#include "Table.h"

struct DELTA {
    DELTA* nextInUndoBuffer;
    DELTA* olderVersion, *newerVersion;
    DELTA* nextInDVsInClosure;
    timestamp xactId;
    Operation op;
    virtual void free() = 0;

    DELTA(timestamp id, DELTA* ub, Operation o, PRED* par) {
        nextInUndoBuffer = ub;
        xactId = id;
        op = o;
        newerVersion = nullptr;
        if (par != nullptr) {
            nextInDVsInClosure = par->DVsInClosureHead;
            par->DVsInClosureHead = this;
        } else {
            nextInDVsInClosure = nullptr;
        }
    }
};

template <typename K, typename V>
struct DeltaVersion : DELTA {
    Entry<K, V>* entry;
    V val;
#ifdef ATTRIB_LEVEL
    col_type cols;

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(), cols(-1) {
        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }
#else

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val() {

        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }

#endif
//TODO: ADd static ref to dvstore
    void free() override {
        entry->tbl->dvStore.remove(this);

    }

    void initialize(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation o, PRED* parent, const col_type& colsChanged = col_type(-1)) {
#ifdef ATTRIB_LEVEL
        cols = colsChanged;
#endif
        entry = e;
        nextInUndoBuffer = nextUndo;
        xactId = id;
        op = o;
        newerVersion = nullptr;
        if (parent != nullptr) {
            nextInDVsInClosure = parent->DVsInClosureHead;
            parent->DVsInClosureHead = this;
        } else {
            nextInDVsInClosure = nullptr;
        }
        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }

};


#endif /* VERSION_H */

