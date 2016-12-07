#ifndef VERSION_H
#define VERSION_H
#include "util/types.h"
#include "predicate/Predicate.h"
#include "storage/Table.h"

struct DELTA {
    DELTA* nextInUndoBuffer;
    DELTA* nextInDVsInClosure;

    timestamp xactId; //Either temporary id of the transaction or its commit timestamp
    Operation op;

#if (ALLOW_WW) 
    bool isWWversion; //To mark if this version was created as result of an update that allows multiple uncommitted versions
    std::atomic<DELTA*> olderVersion;

    forceinline void removeFromVersionChain(uint8_t tid) {
        if (op == INSERT)
            removeEntry(tid); //guaranteed that no other threads see the entry. Safe to remove.
        else {
            //Mark version for removal. Would be removed from the linked list by some later traversal.
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

    /**
     * Constructor
     * @id Temporary id of the transaction
     * @ub Previous head of undo buffer of current transaction
     * @par Parent predicate, in whose closure this version is created. nullptr, if created in root scope.
     */
    DELTA(timestamp id, DELTA* ub, Operation o, PRED* par) {
        nextInUndoBuffer = ub;
        xactId = id;
        op = o;
#if (ALLOW_WW)
        isWWversion = false;
#endif
        //If there is a parent predicate, add to its DVs In Closure list
        if (par != nullptr) {
            nextInDVsInClosure = par->DVsInClosureHead;
            par->DVsInClosureHead = this;
        } else {
            nextInDVsInClosure = nullptr;
        }
    }
    //Version is uncommitted if the xactId is temporary and the commit timestamp of transaction is not yet assigned

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
#endif

    DeltaVersion() = default;

#if (!ALLOW_WW)

    void removeFromVersionChain(uint8_t tid) override {
        if (op == INSERT)
            removeEntry(tid); //guaranteed that no other threads see the entry. Safe to remove.
        else {
            //When WW is not allowed, any version that is being undone HAS to be the first version
            DeltaVersion<K, V>* dv = this;
            auto dvOld = (DeltaVersion<K, V>*)olderVersion.load();
            if (!entry->dv.compare_exchange_strong(dv, dvOld)) {
                cerr << "DV being removed is not the first version" << endl;
            }
            //        store.remove(this, tid); //Cannot garbage collect version as some other thread could be traversing version chain

        }
        op = INVALID;
    }
#else
    //When WW is allowed, after committing, the version should be moved next to other committed versions.
    //To make it easier in presence of multiple threads, a copy of the current version is added next to the latest committed version, and this version is marked invalid.

    void moveNextToCommitted(Transaction *xact) override {
        auto newdv = store.add(xact->threadId);
#ifdef ATTRIB_LEVEL
        //If attribute level, we need to copy value from committed version. Here we initialize the other fields
        //If record level, we can duplicate this version entirely, so no need to initialize here.
        newdv->entry = entry;
        newdv->isWWversion = true;
        newdv->nextInDVsInClosure = (DELTA*) nonAccessibleMemory; //for catching errors.
        newdv->nextInUndoBuffer = (DELTA*) nonAccessibleMemory;
        assert(op == UPDATE);
        newdv->op = UPDATE;
        newdv->xactId = xactId;
#endif
        DeltaVersion<K, V>* committed;
        DELTA * old, *oldOld, *temp;
        bool first = true;
        //The focus of this loop is cur, initialized with the this version's olderversion 
        //old points to  the previous value of cur, and ideally, the newever version after cur.
        //oldOld refers to olderversion after old, which is ideally cur.
        //curOV refers to olderversion of cur.
        // .....  -> old -> oldOld(-> .... ->)cur -> curOV ......
        //To know if a node is invalid, we check if its olderversion is marked.
        //For example, to check if cur is deleted, we do isMarked(curOV)

        do {
            old = this;
            oldOld = olderVersion.load();
            //guaranteed that olderversion != nullptr and uncommitted, and unmarked
            DELTA* cur = oldOld;
            DELTA* curOV = cur->olderVersion.load();
            while (isMarked(curOV) || cur->isUncommitted()) { //while cur is marked for deletion or is uncommitted, we go forward
                if (!isMarked(curOV)) { //if cur is not marked for deletion, then we remove everything between old and cur
                    if (oldOld != cur) { //if oldOld is cur, there is nothing in between
                        old->olderVersion.compare_exchange_strong(oldOld, cur); //we dont care abt the result
                    }
                    //move forward, cur is now old, and curOV is oldOld as well as new value of cur.
                    old = cur;
                    oldOld = curOV;
                    cur = curOV;
                } else {
                    cur = unmark(curOV); //curOV is marked. get the unmarked version as cur. old and oldOld are not updated.
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
                store.remove(newdv, xact->threadId); //delete newly created copy.
#ifdef ATTRIB_LEVEL
                val.copyColsFromExcept(committed->val, cols); //copy other cols from committed version except the ones this version updated.
#endif
                return;
            }
            if (first) { //first time we find the committed version. 
#ifdef ATTRIB_LEVEL                
                memcpy(&newdv->val, &committed->val, sizeof (V)); //copy committed value
                newdv->val.copyColsFrom(val, cols); //apply changes by this version
#else
                memcpy(newdv, this, sizeof (*this)); //duplicate the whole version
#endif
                newdv->olderVersion.store(committed);
                first = false; //we may loop again and again until the oldOld is stabilized, but committed version doesnt change
            }
        } while (!old->olderVersion.compare_exchange_strong(oldOld, newdv));

        DELTA* thisOld = olderVersion.load();
        while (!olderVersion.compare_exchange_weak(thisOld, mark(thisOld)));

    }
#ifdef ATTRIB_LEVEL
    //If already next to committed version, we still need to copy columns from the committed version

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

    //Used instead of the constructor. This is because version is created with the value in-place and other fields are updated later

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

