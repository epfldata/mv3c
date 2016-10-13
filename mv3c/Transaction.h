#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "types.h"

struct Transaction {
    DELTA* undoBufferHead;
    PRED* predicateHead;
    static TransactionManager& tm;
    timestamp startTS, commitTS;
    uint8_t threadId;

    Transaction() {
        threadId = 0;
        commitTS = initCommitTS;
        undoBufferHead = nullptr;
        predicateHead = nullptr;
    }
};

#endif /* TRANSACTION_H */

