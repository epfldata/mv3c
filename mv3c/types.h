#ifndef TYPES_H
#define TYPES_H
//ALLOW_WW=true CRITICAL_COMPENSATE=true NUMPROG=5000000 NUMWARE=5 OMVCC=true POWER=2 TRADING_TEST
#ifdef NB  //Netbeans
#define ALLOW_WW true
#define CRITICAL_COMPENSATE false
#define NUMPROG 1000000
#define NUMWARE 1
#define OMVCC false
//#define POWER 2
#define DTIMER 1
//#define TRADING_TEST 1   
//#define BANKING_TEST 1
#define BENCH 1
//#define TPCC_TEST 1
//#define TPCC2_TEST 1
//#define RIPPLE_TEST 1
#define NUMTHREADS 6
//Tuned for yper server. Should change for IC server
#define CONFLICT_FRACTION 1.0
#define ATTRIB_LEVEL 1
//#define STORE_ENABLE 1
#define CUCKOO true
#define CUCKOO_SI true
#define CCSI true
#define CWW false
#define VERIFY true
//#define PERF_STAT true
#endif
#include <type_traits>
#include <utility>
#include <stdint.h>
#include "MemoryPool.h"
#include<iostream>
#include<sstream>
#include<fstream>
#include "Tuple.h"
#include <atomic>
#include <sched.h>
#include <pthread.h>
using std::cout;
using std::endl;
using std::cerr;
struct Transaction;
struct TransactionManager;
#include "Store.h"
#include <string>
using namespace std;
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
#ifdef NDEBUG
#define  forceinline  __attribute__((always_inline))
#define dontinline __attribute__ ((noinline))
#else
#define dontinline __attribute__ ((noinline))
#define forceinline  
#endif 
typedef uint64_t timestamp;
const timestamp mask = 1LL << 63;
const timestamp nonAccessibleMemory = mask + 100;
const timestamp initCommitTS = mask + 5;
#define isTempTS(ts) (ts&mask)
#define PTRtoTS(t) ((timestamp) t ^ mask)
#define TStoPTR(ts) ((Transaction*) (ts ^ mask))

template<typename T>
forceinline bool isMarked(T t) {
    return ((size_t) t & mask);
}

template<typename T>
forceinline T mark(T t) {
    return (T) ((size_t) t | mask);
}

template<typename T>
forceinline T unmark(T t) {
    return (T) ((size_t) t & ~mask);
}
#ifndef OMVCC
#define OMVCC false
#endif

#ifndef ALLOW_WW
#define ALLOW_WW false
#endif

#if OMVCC
#undef ALLOW_WW
#define ALLOW_WW false
#endif

#if NUMTHREADS==1
#undef ALLOW_WW
#define ALLOW_WW false
#endif

#define EXPAND(x) #x
#define STRINGIFY(x) EXPAND(x)

#ifndef CUCKOO
#define CUCKOO true
#endif

#ifndef CRITICAL_COMPENSATE
#define CRITICAL_COMPENSATE false
#endif

#define  TABLE(x) Table<x##Key, x##Val>
#define GETP(x) GetPred<x##Key,x##Val>
#define DV(x) DeltaVersion<x##Key,x##Val>

#define DefineStore(type)\
  template<>\
  DeltaVersion<type##Key, type##Val>::StoreType DeltaVersion<type##Key, type##Val>::store(type##DVSize, STRINGIFY(type)"DV", numThreads);\
  template<>\
  Entry<type##Key, type##Val>::StoreType Entry<type##Key, type##Val>::store(type##EntrySize, STRINGIFY(type)"Entry", numThreads)

#define setAffinity(thread_id)\
    cpu_set_t cpuset;\
    CPU_ZERO(&cpuset);\
    CPU_SET(thread_id+1, &cpuset);\
    auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);\
    if (s != 0)\
        throw std::runtime_error("Cannot set affinity");

#define setSched(type)\
    sched_param param;\
    param.__sched_priority =  sched_get_priority_max(type);\
    s = sched_setscheduler(0, type, &param);\
    if (s != 0)\
        throw std::runtime_error("Cannot set scheduler");
#endif /* TYPES_H */

