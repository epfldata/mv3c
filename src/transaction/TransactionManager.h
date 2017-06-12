#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "util/types.h"
#include "transaction/Transaction.h"
#include "storage/DeltaVersion.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;
    SpinLock commitLock, counterLock;
    std::atomic<Transaction *> committedXactsTail;

    TransactionManager() : timestampGen(0) {
        committedXactsTail = nullptr;
    }

    forceinline void begin(Transaction *xact) {
        counterLock.lock();
        auto ts = timestampGen++;
        xact->startTS = ts;
        counterLock.unlock();
    }

    forceinline void rollback(Transaction * xact) {
        auto dv = xact->undoBufferHead;

        while (dv) {
            dv->removeFromVersionChain(xact->threadId);
            dv = dv->nextInUndoBuffer;
        }
        xact->undoBufferHead = nullptr;
        xact->predicateHead = nullptr;
    }
#if OMVCC
    //Validate transaction by matching its predicates with versions of recently committed transactions 

    forceinline bool validate(Transaction *xact, Transaction *currentXact) {
        auto start = DNow, end = start;
        auto head = xact->predicateHead;
        while (head != nullptr) {
            //go through root predicates
            bool headMatches = head->matchesAny(currentXact);
            if (headMatches) {
                //we stop as soon as one predicate matches, and there is no point in continuing validation
                rollback(xact);
                end = DNow;
                xact->validateTime += DDurationNS(end - start);
                return false;
                //Will do full rollback
                //Rollback takes care of versions
            } else {
                PRED * child = head->firstChild;
                while (child != nullptr) {
                    //go through level 1 predicates
                    bool childMatches = child->matchesAny(currentXact);
                    if (childMatches) {
                        rollback(xact);
                        end = DNow;
                        xact->validateTime += DDurationNS(end - start);
                        return false;
                    } else {
                        //go through level 2 predicates
                        PRED * childChild = child->firstChild;
                        while (childChild != nullptr) {
                            bool childChildMatches = childChild->matchesAny(currentXact);
                            if (childChildMatches) {
                                rollback(xact);
                                end = DNow;
                                xact->validateTime += DDurationNS(end - start);
                                return false;
                            }
                            childChild = childChild->nextChild;
                        }
                    }
                    child = child->nextChild;
                }
            }
            head = head->nextChild;
        }
        end = DNow;
        xact->validateTime += DDurationNS(end - start);
        return true;
    }

#else
    //Validate transaction by matching its predicates with versions of recently committed transactions
    //Validate is hardcoded for upto 3 levels in predicate graph
    //TODO: Add generalized validate to be invoked beyong 3 levels

    forceinline bool validate(Transaction *xact, Transaction *currentXact) {
        auto start = DNow;
        bool validated = true;
        auto head = xact->predicateHead;
        //Unlike OMVCC, in MV3C validation continues at the next predicate when a match is found, instead of stopping
        while (head != nullptr) {
            bool headMatches = head->matchesAny(currentXact);
            if (headMatches) {
                //If predicate matches, its entire subtree is rolled back
                validated = false;
                DELTA* headDV = head->DVsInClosureHead;
                //Undo all its versions
                while (headDV != nullptr) {
                    headDV->removeFromVersionChain(xact->threadId);
                    headDV = headDV->nextInDVsInClosure;
                }

                PRED* child = head->firstChild;
                //Do recursively for each child
                while (child != nullptr) {
                    DELTA* childDV = child->DVsInClosureHead;
                    //undo child's versions
                    while (childDV != nullptr) {
                        childDV->removeFromVersionChain(xact->threadId);
                        childDV = childDV->nextInDVsInClosure;
                    }
                    PRED* childChild = child->firstChild;
                    //Do recursively for each grandchild
                    while (childChild != nullptr) {
                        DELTA * childChildDV = childChild->DVsInClosureHead;
                        //undo grandchild's versions
                        while (childChildDV != nullptr) {
                            childChildDV->removeFromVersionChain(xact->threadId);
                            childChildDV = childChildDV -> nextInDVsInClosure;
                        }
                        childChild = childChild->nextChild;
                    }
                    child = child->nextChild;
                }

                head->isInvalid = true;
                head->firstChild = nullptr;
                head->DVsInClosureHead = nullptr;
            } else {
                //Since root predicate validated successfully, try to validate child predicates
                PRED * child = head->firstChild;
                while (child != nullptr) {
                    bool childMatches = child->matchesAny(currentXact);
                    if (childMatches) {
                        validated = false;
                        DELTA* childDV = child->DVsInClosureHead;
                        while (childDV != nullptr) {
                            childDV->removeFromVersionChain(xact->threadId);
                            childDV = childDV->nextInDVsInClosure;
                        }
                        PRED* childChild = child->firstChild;
                        while (childChild != nullptr) {
                            DELTA * childChildDV = childChild->DVsInClosureHead;
                            while (childChildDV != nullptr) {
                                childChildDV->removeFromVersionChain(xact->threadId);
                                childChildDV = childChildDV -> nextInDVsInClosure;
                            }
                            childChild = childChild->nextChild;
                        }
                        child->isInvalid = true;
                        child->firstChild = nullptr;
                        child->DVsInClosureHead = nullptr;
                    } else {
                        //Now validate level 2 predicates
                        PRED * childChild = child->firstChild;
                        while (childChild != nullptr) {
                            bool childChildMatches = childChild->matchesAny(currentXact);
                            if (childChildMatches) {
                                validated = false;
                                DELTA * childChildDV = childChild->DVsInClosureHead;
                                while (childChildDV != nullptr) {
                                    childChildDV->removeFromVersionChain(xact->threadId);
                                    childChildDV = childChildDV -> nextInDVsInClosure;
                                }
                                childChild->isInvalid = true;
                                childChild->firstChild = nullptr;
                                childChild->DVsInClosureHead = nullptr;
                            }
                            //TODO: else {validate level 3 and beyond}
                            childChild = childChild->nextChild;
                        }
                    }
                    child = child->nextChild;
                }
            }
            head = head->nextChild;
        }
        auto end = DNow;
        xact->validateTime += DDurationNS(end - start);
        return validated;
    }
#endif

#if(!OMVCC)
    //Repair transactions that fail validation

    forceinline TransactionReturnStatus reexecute(Transaction *xact, Program *state) {
        auto start = DNow;
        PRED* head = xact->predicateHead;
        int rex = 0;
        while (head != nullptr) {
            if (head->isInvalid) {
                rex++;
                assert(head->firstChild == nullptr);
                assert(head->DVsInClosureHead == nullptr);
                auto status = head->compensateAndExecute(xact, state);
                if (status != SUCCESS)
                    return status;

            } else {

                PRED * child = head->firstChild;
                while (child != nullptr) {
                    if (child->isInvalid) {
                        rex++;
                        assert(child->firstChild == nullptr);
                        assert(child->DVsInClosureHead == nullptr);
                        auto status = child->compensateAndExecute(xact, state);
                        if (status != SUCCESS)
                            return status;
                    } else {
                        PRED* childChild = child->firstChild;
                        while (childChild != nullptr) {

                            if (childChild->isInvalid) {
                                rex++;
                                assert(childChild->firstChild == nullptr);
                                assert(childChild->DVsInClosureHead == nullptr);
                                auto status = childChild->compensateAndExecute(xact, state);
                                if (status != SUCCESS)
                                    return status;
                            }
                            childChild = childChild->nextChild;
                        }
                    }
                    child = child->nextChild;
                }
            }
            head = head->nextChild;
        }
        assert(rex > 0);
        auto end = DNow;
        xact->compensateTime += DDurationNS(end - start);
        return SUCCESS;
    }
#endif

    forceinline void commit(Transaction *xact) {
        auto start = DNow;
        //In case of WW, we need to move versions next to committed.
        // In case of attribute level validation, we  need to copy cols from prev committed
        auto dv = xact->undoBufferHead;
#if(ALLOW_WW)
        while (dv) {

            DELTA* dvOld = dv->olderVersion.load(); //dvOld could possibly be marked for deletion. Unsafe in presence of GC
            if (dv->isWWversion) {
                //Will have a older version as it is update                    
                //if dv is already next to a committed version, there is no need to move it, only copying of columns is needed
                //Otherwise, a copy of the version will be placed next to the committed version, and the columns will be copied from the latest committed
                if (dvOld->isUncommitted()) {
                    dv->moveNextToCommitted(xact);
                } else {
#ifdef ATTRIB_LEVEL
                    dv->mergeWithPrevCommitted(xact);
#endif
                }
            }
#ifdef ATTRIB_LEVEL
            //else if(dv->op == UPDATE && dvOld->isWWversion && tbl->allowWW)  //Needed when we have table which have both WW and non-WW updates.
            //dv->mergeWithPrevCommitted();
#endif
            dv = dv->nextInUndoBuffer;
        }
        //Should acquire commitTS only after copying cols from prev committed
        //Otherwise new transactions read incorrect value for other fields
        dv = xact->undoBufferHead;
#endif

        counterLock.lock();
        xact->commitTS = timestampGen.fetch_add(1);
        counterLock.unlock();

        commitLock.unlock();

        while (dv) {
            dv->xactId = xact->commitTS; //TODO: DOES NOT SET CommitTS on the moved version
            //This is still correct , as the getCorrectDV looks up commit TS of transaction even if not set in version
            // BUt this has performance overhead as in almost all cases, it will lookup transaction un necessarily
            dv = dv->nextInUndoBuffer;
        }
        auto end = DNow;
        xact->commitTime += DDurationNS(end - start);
    }

    forceinline bool validateAndCommit(Transaction *xact, Program *state = nullptr, bool critical_compensate = false) {
        if (xact->undoBufferHead == nullptr) { //Read only transaction
            xact->commitTS = xact->startTS;
            return true;
        }

        bool validated = true;
        Transaction *startXact = committedXactsTail, *endXact = nullptr, *currentXact;
        xact->numValidations++;

        /*
         We validate in rounds. We start from the most recently committed transaction and go backwards until
             we reach nullptr, 
             or transaction that committed before this transaction began,
             or whatever was most recent transaction in the previous round.
        For example, if we have a commit list .. 6 - 7 - 8 - 9 (tail)
        and the current transaction began at 5, a possible validation order is 
        9 - 8 - 7 - 6 - 12 - 11 -10 - 16 - 15 - 14 - 13, and so on
         */

        do {
            xact->numRounds++;
            if (startXact != nullptr) {
                currentXact = startXact;
                //wait until committed??  current->xact->status == COMMITTED
                //                while (currentXact->commitTS == initCommitTS);
                //Required only when we have slice on value, as the final value may not be ready(copy from previous committed etc)

                //go until end of recently committed list
                while (currentXact != nullptr && currentXact != endXact && currentXact->commitTS > xact->startTS) {
#if OMVCC
                    //in OMVCC, if validate returns false, we abort and do full restart
                    if (!validate(xact, currentXact))
                        return false;
#else
                    //in MV3C, we continue validation even if it fails
                    validated &= validate(xact, currentXact);
#endif
                    currentXact = currentXact->prevCommitted;
                    xact->numXactsValidatedAgainst++;
                }
            }
            endXact = startXact;

            //If either the tail changed or if we cannot get commit lock, then there are new transactions that we should validate against
            if (committedXactsTail != startXact || !commitLock.try_lock()) {
                startXact = committedXactsTail;
                continue;
            }
            //we have the lock here
#if !OMVCC
            if (validated) {
#endif
                xact->prevCommitted = startXact;
                //try adding this transaction as the latest committed
                if (!committedXactsTail.compare_exchange_strong(startXact, xact)) {
                    commitLock.unlock();
                    continue;
                }

                commit(xact); //will release commitLock internally
                return true;
#if (!OMVCC)
            } else {
                //To validate against all transactions, even if we are sure we will fail
                if (startXact != committedXactsTail) {
                    startXact = committedXactsTail;
                    commitLock.unlock();
                    continue;
                }
                DELTA *dv = xact->undoBufferHead, *prev = nullptr, *dvold;
                //start repair phase by getting new start time stamp
                counterLock.lock();
                xact->startTS = timestampGen++;
                counterLock.unlock();

                //for critical compensate, the repair happens inside the same critical section as commit
                //So, we do the repair without releasing the lock
                if (critical_compensate) {
                    //Remove rolledback versions from undobuffer
                    while (dv != nullptr) {
                        if (dv->op != INVALID) {
                            if (prev == nullptr)
                                xact->undoBufferHead = dv;
                            else
                                prev->nextInUndoBuffer = dv;
                            prev = dv;
                        }
                        dv = dv->nextInUndoBuffer;
                    }
                    if (prev == nullptr)
                        xact->undoBufferHead = nullptr;
                    else
                        prev->nextInUndoBuffer = nullptr;

                    auto status = reexecute(xact, state);
                    if (status != SUCCESS) {
                        cerr << "Cannot fail execution during critical reexecution";
                        exit(-6);
                    }
                    xact->prevCommitted = startXact;
                    committedXactsTail = xact;
                    commit(xact); //will release commitLock internally
                    return false;
                } else {
                    //Not critical compensate. repair done outside critical section
                    //The lock is therefore released
                    commitLock.unlock();
                    //Remove rolledback versions from undobuffer
                    while (dv != nullptr) {
                        if (dv->op != INVALID) {
                            if (prev == nullptr)
                                xact->undoBufferHead = dv;
                            else
                                prev->nextInUndoBuffer = dv;
                            prev = dv;
                        }
                        dv = dv->nextInUndoBuffer;
                    }
                    if (prev == nullptr)
                        xact->undoBufferHead = nullptr;
                    else
                        prev->nextInUndoBuffer = nullptr;
                    return false;
                    //Call reexecute externally after this returns false
                }

            }
#endif
        } while (true);
    }
};


#endif /* TRANSACTIONMANAGER_H */

