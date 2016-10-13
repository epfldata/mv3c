#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;
    std::atomic_flag commitLock;

    TransactionManager() : timestampGen(0) {
    }

    void begin(Transaction *xact) {
        auto ts = timestampGen++;
        xact->startTS = ts;
    }

    bool commit(Transaction *xact) {
        while (commitLock.test_and_set());
        xact->commitTS = timestampGen.fetch_add(1);
        commitLock.clear();
        auto dv = xact->undoBufferHead;
        while(dv){
            dv->xactId = xact->commitTS;
            dv = dv->nextInUndoBuffer;
        }
        return true;
    }

    void rollback(Transaction * xact) {
        while (xact->undoBufferHead) {
            auto dv = xact->undoBufferHead;
            xact->undoBufferHead = xact->undoBufferHead->nextInUndoBuffer;
            dv->removeFromVersionChainAndDelete(xact->threadId);
        }
    }
};


#endif /* TRANSACTIONMANAGER_H */

