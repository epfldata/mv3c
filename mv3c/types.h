#ifndef TYPES_H
#define TYPES_H
#include<iostream>
#include "Tuple.h"
using std::cout;
using std::endl;
using std::cerr;
struct Transaction;
struct TransactionManager;
#include "Store.h"
template<typename K, typename V>
struct Table;
template<typename K, typename V>
struct Entry;
template<typename K, typename V>
struct DeltaVersion;
struct DELTA;
struct PRED;
struct Program;
template<typename T>
struct Hasher;
template<typename T>
struct Equals;
template<typename T>
struct ConcurrentStore;

struct ThreadLocal;

enum TransactionReturnStatus : char {
    SUCCESS, ABORT, WW_ABORT, COMMIT_FAILURE
};

enum Operation : char {
    NOOP, INSERT, DELETE, UPDATE, INVALID
};

enum OperationReturnStatus : char {
    OP_SUCCESS, NO_KEY, DUPLICATE_KEY, WW_VALUE
};
#define  forceinline  __attribute__((always_inline))
typedef uint64_t timestamp;
const timestamp mask = 1LL << 63;
const timestamp initCommitTS = mask + 5;
#define isTempTS(ts) (ts&mask)
#define PTRtoTS(t) ((timestamp) t ^ mask)
#define TStoPTR(ts) ((Transaction*) (ts ^ mask))

#ifndef ALLOW_WW
#define ALLOW_WW false
#endif
#endif /* TYPES_H */

