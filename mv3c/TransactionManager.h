#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;
    std::atomic_flag commitLock, counterLock;
    std::atomic<Transaction *> committedXactsTail;
    std::atomic<size_t> numValidations, numXactsValidatedAgainst, numRounds;

    TransactionManager() : timestampGen(0) {
        committedXactsTail = nullptr;
        numValidations = 0;
        numXactsValidatedAgainst = 0;
        numRounds = 0;
    }

    forceinline void begin(Transaction *xact) {
        while (!counterLock.test_and_set());
        auto ts = timestampGen++;
        xact->startTS = ts;
        counterLock.clear();
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

    forceinline TransactionReturnStatus reexecute(Transaction *xact, Program *state) {
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
        return SUCCESS;
    }

    forceinline bool validate(Transaction *xact, Transaction *currentXact) {
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
        return validated;
    }

    forceinline void commit(Transaction *xact) {
        //In case of WW, we need to move versions next to committed.
        // In case of attribute level validation, we  need to copy cols from prev committed
        auto dv = xact->undoBufferHead;
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


        while (counterLock.test_and_set());
        xact->commitTS = timestampGen.fetch_add(1);
        counterLock.clear();

        commitLock.clear();
        dv = xact->undoBufferHead;
        while (dv) {
            dv->xactId = xact->commitTS;
            dv = dv->nextInUndoBuffer;
        }
    }

    forceinline bool validateAndCommit(Transaction *xact, Program *state = nullptr) {
        //TODO: Handle read only
        bool validated = true;
        Transaction *startXact = committedXactsTail, *endXact = nullptr, *currentXact;
        uint round = 0;
        numValidations++;
        size_t xactCount = 0;
        do {
            round++;
            if (startXact != nullptr) {
                currentXact = startXact;
                //wait until committed??  current->xact->status == COMMITTED
                //                while (currentXact->commitTS == initCommitTS);
                //Required only when we have slice on value, as the value may not be ready

                while (currentXact != nullptr && currentXact != endXact && currentXact->commitTS > xact->startTS) {
                    validated &= validate(xact, currentXact);
                    currentXact = currentXact->prevCommitted;
                    xactCount++;
                }
            }
            endXact = startXact;
            if (committedXactsTail != startXact || commitLock.test_and_set()) { //there are new transactions to be validated against
                startXact = committedXactsTail;
                continue;
            }
            //we have the lock here
            if (validated) {
                if (!committedXactsTail.compare_exchange_strong(startXact, xact)) {
                    commitLock.clear();
                    continue;
                }
                xact->prevCommitted = startXact;
                commit(xact); //will release commitLock internally
                numXactsValidatedAgainst += xactCount;
                numRounds += round;
                return true;
            } else {
                //To validate against all transactions, even if we are sure we will fail
                if (startXact != committedXactsTail) {
                    startXact = committedXactsTail;
                    commitLock.clear();
                    continue;
                }
                DELTA *dv = xact->undoBufferHead, *prev = nullptr, *dvold;

                while (!counterLock.test_and_set());
                xact->startTS = timestampGen++;
                counterLock.clear();
#if CRITICAL_COMPENSATE
                while (dv != nullptr) {
                    dvold = dv->olderVersion.load();
                    if (!isMarked(dvold)) {
                        if (prev == nullptr)
                            xact->undoBufferHead = dv;
                        else
                            prev->nextInUndoBuffer = dv;
                        prev = dv;
                    }
                    dv = dv->nextInUndoBuffer;
                }
                prev->nextInUndoBuffer = nullptr;

                reexecute(xact, state);
                xact->prevCommitted = startXact;
                commit(xact); //will release commitLock internally
                numXactsValidatedAgainst += xactCount;
                numRounds += round;
                return true;
#else
                commitLock.clear();
                while (dv != nullptr) {
                    dvold = dv->olderVersion.load();
                    if (!isMarked(dvold)) {
                        if (prev == nullptr)
                            xact->undoBufferHead = dv;
                        else
                            prev->nextInUndoBuffer = dv;
                        prev = dv;
                    }
                    dv = dv->nextInUndoBuffer;
                }
                prev->nextInUndoBuffer = nullptr;
                return false;
#endif

            }
        } while (true);
    }
};


#endif /* TRANSACTIONMANAGER_H */

