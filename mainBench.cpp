#include "types.h"
#ifdef BENCH
#include "Table.h"
#include <cstdlib>
#include "Tuple.h"
#include "Timer.h"
#include "CuckooSecondaryIndex.h"
#include "ConcurrentCuckooSecondaryIndex.h"
#include "MultiMapIndexMT.h"
#include <thread>
#include <sched.h>
#include <pthread.h>
#include <iomanip>
#include <locale>
using namespace std;

#ifndef NUMTHREADS
#define NUMTHREADS 4;
#endif

#ifndef SCALE
#define SCALE 10
#endif
const int numThreads = NUMTHREADS;
const int initRows = 100000;
const double insertProb = 0.60;
const int iMax = initRows + 100000;
const int jMax = SCALE;

typedef KeyTuple<int, int> TKey;
typedef ValTuple<double> TVal;
typedef Entry<TKey, TVal> TEntry;
int TEntrySize = 0;
int TDVSize = 0;
DefineStore(T);


struct PKey {
    int _1;

    PKey(int _1) :
    _1(_1) {

    }

    PKey(const TEntry* e, const TVal& val) {
        _1 = e->key._1;

    }

    bool operator<(const PKey& right) const {
        return _1 < right._1;
    }

    bool operator==(const PKey& right) const {
        return _1 == right._1;
    }

};
ConcurrentCuckooSecondaryIndex<TKey, TVal, PKey> concCuckooIdx(iMax * 2);
CuckooSecondaryIndex<TKey, TVal, PKey> cuckooIdx(iMax * 2);
MultiMapIndexMT<TKey, TVal, PKey> mmapIdx;

#ifdef CPI
cuckoohash_map<TKey, TEntry*, CityHasher<TKey>> primaryIndex(iMax * jMax * 2);
#endif

#ifdef MPI
UnorderedIndex<TKey, TEntry*, CityHasher<TKey>> primaryIndex(iMax * jMax * 2);
#endif


volatile bool startExecution = false, hasFinished = false;
volatile bool isReady[numThreads];
int numOps[numThreads];
int numIns[numThreads];
int numDel[numThreads];
std::thread workers[numThreads];

void reset() {
    startExecution = false;
    hasFinished = false;
    for (int i = 0; i < numThreads; ++i) {
        isReady[i] = false;
        numOps[i] = 0;
    }
}

void insert(SecondaryIndex<TKey, TVal>* index, int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id, &cpuset);
    auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);
    if (s != 0) {
        throw std::runtime_error("Cannot set affinity");
    }

    double probs[100];
    for (int i = 0; i < 100; ++i) {
        probs[i] = (1.0 * rand()) / RAND_MAX;
    }
    int probCounter = 0;
    auto start = Now, end = Now;
    do {
        end = Now;
    } while (DurationMS(end - start) <= 1000);
    isReady[thread_id] = true;

    while (!startExecution);

    int numOP = 0, numI = 0, numD = 0;
    for (int i = initRows + thread_id; i < iMax && !hasFinished && numOP < 10000000;) {

        for (int j = 0; j < jMax; ++j) {
            if (probs[probCounter] < insertProb) {
                index->insert(new(TEntry::store.add(0)) TEntry(nullptr, TKey(i, j)), TVal(0));
                numOP++;
                numI++;
                i += numThreads;
            } else {
                i -= numThreads;
                TEntry tempEntry(nullptr, TKey(i, j));
                index->erase(&tempEntry, TVal(0));
                numOP++;
                numD++;
                
            }
            probCounter++;
            if (probCounter == 100)
                probCounter = 0;
        }
    }
    hasFinished = true;
    numOps[thread_id] = numOP;
    numIns[thread_id] = numI;
    numDel[thread_id] = numD;

}

void insertPrimary(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id, &cpuset);
    auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);
    if (s != 0) {
        throw std::runtime_error("Cannot set affinity");
    }
    double probs[100];
    for (int i = 0; i < 100; ++i) {
        probs[i] = (1.0 * rand()) / RAND_MAX;
    }
    int probCounter = 0;
    auto start = Now, end = Now;
    do {
        end = Now;
    } while (DurationMS(end - start) <= 1000);
    isReady[thread_id] = true;

    while (!startExecution);

    int numOp = 0, numI = 0, numD = 0;

#if defined(CPI) || defined(MPI)
    for (int i = initRows + thread_id; i < iMax && !hasFinished && numOp < 10000000;) {
        for (int j = 0; j < jMax; ++j) {
            if (probs[probCounter] < insertProb) {
                TKey key(i, j);
                primaryIndex.insert(key, new(TEntry::store.add(0)) TEntry(nullptr, key));
                numOp++;
                numI++;
                i += numThreads;
            } else {
                i -= numThreads;
                TKey key(i, j);
                primaryIndex.erase(key);
                numOp++;
                numD++;
            }
            probCounter++;
            if (probCounter == 100) {
                probCounter = 0;
            }
        }
    }
#endif
    hasFinished = true;
    numOps[thread_id] = numOp;
    numIns[thread_id] = numI;
    numDel[thread_id] = numD;
}

int sumOps() {
    int sum = 0;
    for (int i = 0; i < numThreads; ++i)
        sum += numOps[i];
    return sum;
}

double Caller(SecondaryIndex<TKey, TVal>* index) {
    reset();
    for (int i = 0; i < initRows; ++i) {
        for (int j = 0; j < jMax; ++j) {
            index->insert(new(TEntry::store.add(0)) TEntry(nullptr, TKey(i, j)), TVal(0));
        }
    }
    auto startTime = Now, endTime = startTime;
    for (uint8_t i = 0; i < numThreads; ++i) {
        workers[i] = std::thread(insert, index, i);
    }
    bool all_ready = true;
    while (true) {
        for (uint8_t i = 0; i < numThreads; ++i) {
            if (isReady[i] == false) {
                all_ready = false;
                break;
            }
        }
        if (all_ready) {
            startTime = Now;
            startExecution = true;
            break;
        }

        all_ready = true;
    }
    for (uint8_t i = 0; i < numThreads; ++i) {
        workers[i].join();
    }
    endTime = Now;
    return DurationUS(endTime - startTime) / 1000.0;
}

double CallerPrimary() {
    reset();
    auto startTime = Now, endTime = startTime;

#if defined(CPI) || defined(MPI)
    for (int i = 0; i < initRows; ++i) {
        for (int j = 0; j < jMax; ++j) {
            TKey key(i, j);
            primaryIndex.insert(key, new(TEntry::store.add(0)) TEntry(nullptr, key));
        }
    }
#endif

    for (uint8_t i = 0; i < numThreads; ++i) {
        workers[i] = std::thread(insertPrimary, i);
    }
    bool all_ready = true;
    while (true) {
        for (uint8_t i = 0; i < numThreads; ++i) {
            if (isReady[i] == false) {
                all_ready = false;
                break;
            }
        }
        if (all_ready) {
            startTime = Now;
            startExecution = true;
            break;
        }

        all_ready = true;
    }
    for (uint8_t i = 0; i < numThreads; ++i) {
        workers[i].join();
    }
    endTime = Now;
    return DurationUS(endTime - startTime) / 1000.0;
}

int main(int argc, char** argv) {

    std::cout.imbue(std::locale(""));
    ofstream fout("out", ios::app), header("header");
    header << "SI,numThreads,jMax,numOps,Throughput";
    int total;
#ifdef CSI

    double cuckooInsertTime = Caller(&cuckooIdx);
    total = sumOps();
    //    cout << "Cuckoo SI insert time = " << cuckooInsertTime << " ms " << endl;    
    //    cout << "Total rows inserted = " << total << endl;
    cout << "Cuckoo SI throughput = " << total / cuckooInsertTime << " ktps" << " for " << numThreads << " threads " << endl;
    fout << "CSI," << numThreads << "," << jMax << "," << total << "," << (total / cuckooInsertTime);
#endif
#ifdef MMSI
    double mmapInsertTime = Caller(&mmapIdx);
    total = sumOps();
    //    cout << "MMAP SI insert time = " << mmapInsertTime << " ms " << endl;    
    //    cout << "Total rows inserted = " << total << endl;
    cout << "MMAP SI throughput = " << total / mmapInsertTime << " ktps" << " for " << numThreads << " threads " << endl;
    fout << "MMSI," << numThreads << "," << jMax << "," << total << "," << (total / mmapInsertTime);
#endif
#ifdef CCSI   
    double concCuckooInsertTime = Caller(&concCuckooIdx);
    total = sumOps();
    //    cout << "Concurrent Cuckoo SI insert time = " << concCuckooInsertTime << " ms " << endl;    
    //    cout << "Total rows inserted = " << total << endl;
    cout << "Concurrent Cuckoo SI throughput = " << total / concCuckooInsertTime << " ktps" << " for " << numThreads << " threads " << endl;
    fout << "CCSI," << numThreads << "," << jMax << "," << total << "," << (total / concCuckooInsertTime);
#endif

#ifdef CPI   
    double itime = CallerPrimary();
    total = sumOps();
    cout << "Cuckoo PI throughput = " << total / itime << " ktps" << " for " << numThreads << " threads " << endl;
    fout << "CPI," << numThreads << "," << jMax << "," << total << "," << (total / itime);
#endif

#ifdef MPI   
    double itime = CallerPrimary();
    total = sumOps();
    cout << "Map PI throughput = " << total / itime << " ktps" << " for " << numThreads << " threads " << endl;
    fout << "MPI," << numThreads << "," << jMax << "," << total << "," << (total / itime);
#endif
    for (int i = 0; i < numThreads; ++i) {
        cout << "t" << i << ":  i=" << numIns[i] << "  d=" << numDel[i] << endl;
    }

    header << endl;
    fout << endl;
    header.close();
    fout.close();

}

#endif