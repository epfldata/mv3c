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
    std::atomic<size_t> numValidations, numXactsValidatedAgainst;

    TransactionManager() : timestampGen(0) {
        committedXactsTail = nullptr;
        numValidations = 0;
        numXactsValidatedAgainst = 0;
    }

    forceinline void begin(Transaction *xact) {
        while(!counterLock.test_and_set());
        auto ts = timestampGen++;
        xact->startTS = ts;
        counterLock.clear();
    }

    forceinline void rollback(Transaction * xact) {
        while (xact->undoBufferHead) {
            auto dv = xact->undoBufferHead;
            xact->undoBufferHead = xact->undoBufferHead->nextInUndoBuffer;
            dv->removeFromVersionChain();
        }
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
            assert(round < 10);
            if (startXact != nullptr) {
                currentXact = startXact;
                //wait until committed??  current->xact->status == COMMITTED
                //                while (currentXact->commitTS == initCommitTS);
                
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
            while(!counterLock.test_and_set());
            xact->commitTS = timestampGen.fetch_add(1);
            counterLock.clear();
            //TODO: Move next to committed for WW values
            commitLock.clear();
            auto dv = xact->undoBufferHead;
            while (dv) {
                dv->xactId = xact->commitTS;
                dv = dv->nextInUndoBuffer;
            }
            numXactsValidatedAgainst += xactCount;
            return true;
        } while (true);
    }
};


#endif /* TRANSACTIONMANAGER_H */

