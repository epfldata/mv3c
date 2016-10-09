#ifndef TRANSACTIONMANAGER_H
#define TRANSACTIONMANAGER_H
#include <atomic>
#include "types.h"
#include "ConcurrentStore.h"

struct TransactionManager {
    std::atomic<timestamp> timestampGen;
    ConcurrentStore<Transaction> store;

    TransactionManager() : timestampGen(0), store(100000, "TransactionStore"){}
    Transaction* begin() {
        auto ts = timestampGen++;
        auto xact = new(store.add()) Transaction(*this, ts);
        return xact;
    }

    bool commit(Transaction *xact) {
        
        return true;
    }
};


#endif /* TRANSACTIONMANAGER_H */

