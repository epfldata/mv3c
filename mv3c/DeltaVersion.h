#ifndef VERSION_H
#define VERSION_H
#include "types.h"
#include "Predicate.h"
#include "Table.h"

struct DELTA {
    DELTA* nextInUndoBuffer;
    DELTA* nextInDVsInClosure;
    timestamp xactId;
    Operation op;
#if (ALLOW_WW) 
    bool isWWversion;
    std::atomic<DELTA*> olderVersion; // *newerVersion;

    forceinline void removeFromVersionChain(uint8_t tid) {
        if (op == INSERT)
            removeEntry(tid);
        else {
            DELTA* old = olderVersion.load();
            while (!olderVersion.compare_exchange_weak(old, mark(old)));
        }
    }

    virtual void moveNextToCommitted(Transaction* xact) = 0;
#ifdef ATTRIB_LEVEL
    virtual void mergeWithPrevCommitted(Transaction *xact) = 0;
#endif
#else
    DELTA* olderVersion;
    virtual void removeFromVersionChain(uint8_t tid) = 0;
#endif
    virtual void removeEntry(uint8_t tid) = 0;
    DELTA() = default;

    DELTA(timestamp id, DELTA* ub, Operation o, PRED* par) {
        nextInUndoBuffer = ub;
        xactId = id;
        op = o;
#if (ALLOW_WW)
        isWWversion = false;
#endif
        if (par != nullptr) {
            nextInDVsInClosure = par->DVsInClosureHead;
            par->DVsInClosureHead = this;
        } else {
            nextInDVsInClosure = nullptr;
        }
    }

    forceinline bool isUncommitted() {
        timestamp ts = xactId;
        return isTempTS(ts) && TStoPTR(ts)->commitTS == initCommitTS;
    }
};

template <typename K, typename V>
struct DeltaVersion : DELTA {
    Entry<K, V>* entry;
    V val;
    typedef Store<DeltaVersion<K, V>> StoreType;
    static StoreType store;
#ifdef ATTRIB_LEVEL
    col_type cols;
    //
    //    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val(), cols(-1) {
    //        olderVersion = entry->dv;
    //        if (olderVersion)
    //            olderVersion->newerVersion = this;
    //        entry->dv = this;
    //    }
#else
    //
    //    DeltaVersion(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation op, PRED* parent) : DELTA(id, nextUndo, op, parent), entry(e), val() {
    //
    //        olderVersion = entry->dv;
    //        if (olderVersion)
    //            olderVersion->newerVersion = this;
    //        entry->dv = this;
    //    }

#endif
    DeltaVersion() = default;
#if (!ALLOW_WW)

    void removeFromVersionChain(uint8_t tid) override {
        if (op == INSERT)
            removeEntry(tid);
        else {
            DeltaVersion<K, V>* dv = this;
            op = INVALID;
            if (!entry->dv.compare_exchange_strong(dv, (DeltaVersion<K, V>*)olderVersion)) {
                cerr << "DV being removed is not the first version" << endl;
            }
            //        store.remove(this, tid);

        }
    }
#else

    void moveNextToCommitted(Transaction *xact) override {
        auto newdv = store.add(xact->threadId);

        DeltaVersion<K, V>* committed;
        DELTA * old, *oldOld;
        bool first = true;

        do {
            old = this;
            oldOld = olderVersion.load();
            //guaranteed that olderversion != nullptr and uncommitted, and unmarked
            DELTA* cur = oldOld;
            DELTA* curOV = cur->olderVersion.load();
            while (isMarked(curOV) || cur->isUncommitted()) {
                if (!isMarked(curOV)) {
                    if (oldOld != cur) {
                        old->olderVersion.compare_exchange_strong(oldOld, cur); //we dont care abt the result
                    }
                    old = cur;
                    oldOld = curOV;
                    cur = curOV;
                } else {
                    cur = unmark(curOV);
                }
                curOV = cur->olderVersion.load();
            }
            if (oldOld != cur) {
                old->olderVersion.compare_exchange_strong(oldOld, cur); //we dont care abt the result
            }
            committed = (DeltaVersion<K, V> *) cur;
            if (old == this) { //no need to move
                store.remove(newdv, xact->threadId);
#ifdef ATTRIB_LEVEL
                val.copyColsFromExcept(committed->val, cols);
#endif
                return;
            }
            if (first) {
                newdv->olderVersion.store(committed);
                memcpy(newdv, committed, sizeof (*this));
#ifdef ATTRIB_LEVEL
                newdv->val.copyColsFrom(val, cols);
#endif
                first = false;
            }
        } while (!old->olderVersion.compare_exchange_strong(oldOld, newdv));

        DELTA* thisOld = olderVersion.load();
        while (!olderVersion.compare_exchange_weak(thisOld, mark(thisOld)));

    }
#ifdef ATTRIB_LEVEL

    void mergeWithPrevCommitted(Transaction *xact) override {
        auto old = (DeltaVersion<K, V> *)olderVersion.load();
        val.copyColsFromExcept(old->val, cols);
    }
#endif
#endif

    void removeEntry(uint8_t tid) override {
        entry->tbl->primaryIndex.erase(entry->key);
        store.remove(this, tid);
    }

    void initialize(Entry<K, V>* e, timestamp id, DELTA* nextUndo, Operation o, PRED* parent, const col_type& colsChanged = col_type(-1)) {
#ifdef ATTRIB_LEVEL
        cols = colsChanged;
#endif
        entry = e;
        nextInUndoBuffer = nextUndo;
        xactId = id;
        op = o;
        if (parent != nullptr) {
            nextInDVsInClosure = parent->DVsInClosureHead;
            parent->DVsInClosureHead = this;
        } else {
            nextInDVsInClosure = nullptr;
        }

    }

};


#endif /* VERSION_H */

