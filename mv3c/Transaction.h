#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "types.h"

struct Transaction {
    DELTA* undoBufferHead;
    PRED* predicateHead;
    static TransactionManager& tm;
    timestamp startTS;
    Transaction(){}
    Transaction(timestamp st) : undoBufferHead(nullptr), predicateHead(nullptr), startTS(st) {
    }
};

#endif /* TRANSACTION_H */

