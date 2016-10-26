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
        op = INVALID;
    }

    virtual void moveNextToCommitted(Transaction* xact) = 0;
#ifdef ATTRIB_LEVEL
    virtual void mergeWithPrevCommitted(Transaction *xact) = 0;
#endif
#else
    std::atomic<DELTA*> olderVersion;
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
            auto dvOld = (DeltaVersion<K, V>*)olderVersion.load();
            if (!entry->dv.compare_exchange_strong(dv, dvOld)) {
                cerr << "DV being removed is not the first version" << endl;
            }
            //        store.remove(this, tid);

        }
        op = INVALID;
    }
#else

    void moveNextToCommitted(Transaction *xact) override {
        auto newdv = store.add(xact->threadId);
#ifdef ATTRIB_LEVEL
        //If attribute level, we need to copy value from committed version. Here we initialize the other fields
        //If record level, we can duplicate this version entirely, so no need to initialize here.
        newdv->entry = entry;
        newdv->isWWversion = true;
        newdv->nextInDVsInClosure = (DELTA*) nonAccessibleMemory;
        newdv->nextInUndoBuffer = (DELTA*) nonAccessibleMemory;
        assert(op == UPDATE);
        newdv->op = UPDATE;
        newdv->xactId = xactId;
#endif
        DeltaVersion<K, V>* committed;
        DELTA * old, *oldOld, *temp;
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
            committed = (DeltaVersion<K, V> *) cur;
            if (oldOld != cur) {
                //we should use temp here, because we rely on the value of oldOld in the C&S in the loop below
                temp = oldOld;
                old->olderVersion.compare_exchange_strong(temp, cur); //we dont care abt the result
            }

            if (old == this) { //no need to move
                store.remove(newdv, xact->threadId);
#ifdef ATTRIB_LEVEL
                val.copyColsFromExcept(committed->val, cols);
#endif
                return;
            }
            if (first) {
#ifdef ATTRIB_LEVEL
                memcpy(&newdv->val, &committed->val, sizeof (V));
                newdv->val.copyColsFrom(val, cols);
#else
                memcpy(newdv, this, sizeof (*this));
#endif
                newdv->olderVersion.store(committed);
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
        auto numIdx = entry->tbl->numIndexes;
        for (uint8_t i = 0; i < numIdx; ++i) {
            entry->tbl->secondaryIndexes[i]->erase(entry, val);
        }
        //        store.remove(this, tid);
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

