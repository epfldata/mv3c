#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "util/types.h"

struct Transaction {
    DELTA* undoBufferHead;
    PRED* predicateHead;
    static TransactionManager& tm;
    timestamp startTS;
    volatile timestamp commitTS;
    Transaction * prevCommitted;
    uint8_t threadId;

    /* DEBUGGING INFO*/
    std::stringstream ss;
    uint failureCtr;
    size_t numValidations, numXactsValidatedAgainst, numRounds;
    size_t executeTime, commitTime, validateTime, compensateTime;

    Transaction() {
        executeTime = commitTime = validateTime = compensateTime = 0;
        numValidations = 0;
        numXactsValidatedAgainst = 0;
        numRounds = 0;
        threadId = 0;
        failureCtr = 0;
        commitTS = initCommitTS;
        undoBufferHead = nullptr;
        predicateHead = nullptr;
    }
};

#endif /* TRANSACTION_H */

