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
            if (dv->op == INSERT) {
                dv->removeEntry(xact->threadId);
            } else {
                DELTA * dvold = dv->olderVersion.load();
                while (!dv->olderVersion.compare_exchange_weak(dvold, mark(dvold)));
            }
            dv = dv->nextInUndoBuffer;

        }
        xact->undoBufferHead = nullptr;
        xact->predicateHead = nullptr;
    }

    forceinline bool validateAndCommit(Transaction *xact) {
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
                    auto head = xact->predicateHead;
                    while (head != nullptr) {
                        bool headMatches = head->matchesAny(currentXact); //shouldValidate always true
                        if (headMatches) {
                            validated = false;
                            rollback(xact);
                            return false; //1<<63 is not valid commitTS , so used to denote failure
                            //Will do full rollback
                            //Rollback takes care of versions
                        } else {
                            PRED * child = head->firstChild;
                            while (child != nullptr) {
                                bool childMatches = child->matchesAny(currentXact); //shouldValidate always true
                                if (childMatches) {
                                    validated = false;
                                    rollback(xact);
                                    return false;
                                } else {
                                    PRED * childChild = child->firstChild;
                                    while (childChild != nullptr) {
                                        bool childChildMatches = childChild->matchesAny(currentXact);
                                        if (childChildMatches) {
                                            validated = false;
                                            rollback(xact);
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
                    currentXact = currentXact->prevCommitted;
                    xactCount++;
                    //                    currentXact = nullptr;
                }
            }
            endXact = startXact;
            if (committedXactsTail != startXact || commitLock.test_and_set()) { //there are new transactions to be validated against
                startXact = committedXactsTail;
                continue;
            }
            //we have the lock here

            if (!committedXactsTail.compare_exchange_strong(startXact, xact)) {
                commitLock.clear();
                continue;
            }
            xact->prevCommitted = startXact;

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
            numXactsValidatedAgainst += xactCount;
            numRounds += round;
            return true;
        } while (true);
    }
};


#endif /* TRANSACTIONMANAGER_H */

