#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "Transaction.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;

    TransactionManager() : timestampGen(0) {
    }

    void begin(Transaction *xact) {
        auto ts = timestampGen++;
        new(xact) Transaction(ts);
    }

    bool commit(Transaction *xact) {
        xact->commitTS = timestampGen++;
        return true;
    }
};


#endif /* TRANSACTIONMANAGER_H */

