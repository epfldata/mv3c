#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "types.h"

struct Transaction {
    DELTA* undoBufferHead;
    PRED* predicateHead;
    TransactionManager& tm;
    timestamp startTS;

    Transaction(TransactionManager& TM, timestamp st) : undoBufferHead(nullptr), predicateHead(nullptr), tm(TM), startTS(st) {
    }
};

#endif /* TRANSACTION_H */

