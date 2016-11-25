#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"

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

    forceinline bool validate(Transaction *xact, Transaction *currentXact) {
        auto start = DNow, end = start;
        auto head = xact->predicateHead;
        while (head != nullptr) {
            bool headMatches = head->matchesAny(currentXact); //shouldValidate always true
            if (headMatches) {
                rollback(xact);
                end = DNow;
                xact->validateTime += DDurationNS(end - start);
                return false; //1<<63 is not valid commitTS , so used to denote failure
                //Will do full rollback
                //Rollback takes care of versions
            } else {
                PRED * child = head->firstChild;
                while (child != nullptr) {
                    bool childMatches = child->matchesAny(currentXact); //shouldValidate always true
                    if (childMatches) {
                        rollback(xact);
                        end = DNow;
                        xact->validateTime += DDurationNS(end - start);
                        return false;
                    } else {
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

    forceinline bool validate(Transaction *xact, Transaction *currentXact) {
        auto start = DNow;
        bool validated = true;
        auto head = xact->predicateHead;
        while (head != nullptr) {
            bool headMatches = head->matchesAny(currentXact); //shouldValidate always true
            if (headMatches) {
                validated = false;
                DELTA* headDV = head->DVsInClosureHead;
                while (headDV != nullptr) {
                    headDV->removeFromVersionChain(xact->threadId);
                    headDV = headDV->nextInDVsInClosure;
                }

                PRED* child = head->firstChild;
                while (child != nullptr) {
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
                    child = child->nextChild;
                }
                head->isInvalid = true;
                head->firstChild = nullptr;
                head->DVsInClosureHead = nullptr;
            } else {
                PRED * child = head->firstChild;
                while (child != nullptr) {
                    bool childMatches = child->matchesAny(currentXact); //shouldValidate always true
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
            DELTA* dvOld = dv->olderVersion.load(); //dvOld could possibly be marked for deletion
            if (dv->isWWversion) {
                //Will have a older version as it is update                    

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
        //Otherwsie new transactions read incorrect value for other fields
        dv = xact->undoBufferHead;
#endif

        counterLock.lock();
        xact->commitTS = timestampGen.fetch_add(1);
        counterLock.unlock();

        commitLock.unlock();

        while (dv) {
            dv->xactId = xact->commitTS; //TOFIX: DOES NOT SET TS on the moved version
            //This is still correct , as the getCorrectDV looks up commit TS of transaction even if not set in version
            // BUt this has performance overhead as in almost all cases, it will lookup transaction un necessarily
            dv = dv->nextInUndoBuffer;
        }
        auto end = DNow;
        xact->commitTime += DDurationNS(end - start);
    }

    forceinline bool validateAndCommit(Transaction *xact, Program *state = nullptr, bool critical_compensate = false) {
        if (xact->undoBufferHead == nullptr) {
            commitLock.lock();
            commit(xact);
            return true;
        }

        bool validated = true;
        Transaction *startXact = committedXactsTail, *endXact = nullptr, *currentXact;
        xact->numValidations++;


        do {
            xact->numRounds++;
            if (startXact != nullptr) {
                currentXact = startXact;
                //wait until committed??  current->xact->status == COMMITTED
                //                while (currentXact->commitTS == initCommitTS);
                //Required only when we have slice on value, as the value may not be ready

                while (currentXact != nullptr && currentXact != endXact && currentXact->commitTS > xact->startTS) {
#if OMVCC
                    if (!validate(xact, currentXact))
                        return false;
#else
                    validated &= validate(xact, currentXact);
#endif
                    currentXact = currentXact->prevCommitted;
                    xact->numXactsValidatedAgainst++;
                }
            }
            endXact = startXact;
            if (committedXactsTail != startXact || !commitLock.try_lock()) { //there are new transactions to be validated against
                startXact = committedXactsTail;
                continue;
            }
            //we have the lock here
#if !OMVCC
            if (validated) {
#endif
                xact->prevCommitted = startXact;
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

                counterLock.lock();
                xact->startTS = timestampGen++;
                counterLock.unlock();

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
                }

            }
#endif
        } while (true);
    }
};


#endif /* TRANSACTIONMANAGER_H */

