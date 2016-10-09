#ifndef VERSION_H
#define VERSION_H
#include "types.h"
#include "Predicate.h"
#include "Table.h"
#include "ConcurrentStore.h"
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

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, const V&v, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(v), cols(-1) {
        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, const V&v, const col_type& colsChanged, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(v), cols(colsChanged) {
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

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, const V&v, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(v) {
        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }

    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, const V&v, const col_type& colsChanged, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(v) {
        olderVersion = entry->dv;
        if (olderVersion)
            olderVersion->newerVersion = this;
        entry->dv = this;
    }

    void free() override {
        entry->tbl->dvStore.remove(this);

    }

#endif
};


#endif /* VERSION_H */

