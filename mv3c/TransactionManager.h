#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "Transaction.h"
#include "DeltaVersion.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;

    TransactionManager() : timestampGen(0) {
    }

    void begin(Transaction *xact) {
        auto ts = timestampGen++;
        xact->startTS = ts;
    }

    bool commit(Transaction *xact) {
        xact->commitTS = timestampGen++;
        return true;
    }
    void rollback(Transaction *xact){
        while(xact->undoBufferHead){
            auto dv = xact->undoBufferHead;
            xact->undoBufferHead = xact->undoBufferHead->nextInUndoBuffer;
            dv->free(xact->threadId);
        }
    }
};


#endif /* TRANSACTIONMANAGER_H */

