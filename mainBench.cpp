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

#include <iomanip>
#include <locale>
#include "System.hpp"
using namespace std;

#ifndef NUMTHREADS
#define NUMTHREADS 4;
#endif

#ifndef SCALE
#define SCALE 1
#endif
const int numThreads = NUMTHREADS;
const int initRows = 100000;
const double insertProb = 1.1;
const int iMax = initRows + 10000000;
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
TEntry** initSource;
TEntry** inputSource[numThreads];
MemoryPool<TEntry, iMax * sizeof (TEntry) * 2 > entryPool;

void reset() {
    startExecution = false;
    hasFinished = false;
    for (int i = 0; i < numThreads; ++i) {
        isReady[i] = false;
        numOps[i] = 0;
    }
}

void insert(SecondaryIndex<TKey, TVal>* index, int thread_id) {
    setAffinity(thread_id);
    setSched(SCHED_FIFO);
#ifdef CCSI
    ConcurrentCuckooSecondaryIndex<TKey, TVal, PKey>::store = new ConcurrentCuckooSecondaryIndex<TKey, TVal, PKey>::PoolType();
#else
    CuckooSecondaryIndex<TKey, TVal, PKey>::store = new CuckooSecondaryIndex<TKey, TVal, PKey>::PoolType();
#endif
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
    int pos = 0;
    for (int i = initRows + thread_id; i < iMax && !hasFinished && numOP < 10000000;) {

        for (int j = 0; j < jMax; ++j) {
            if (probs[probCounter] < insertProb) {
                index->insert(inputSource[thread_id][pos++], TVal(0));
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
    setAffinity(thread_id);
    setSched(SCHED_FIFO);
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
    int pos = 0;
#if defined(CPI) || defined(MPI)
    for (int i = initRows + thread_id; i < iMax && !hasFinished && numOp < 10000000;) {
        for (int j = 0; j < jMax; ++j) {
            if (probs[probCounter] < insertProb) {
                TKey key(i, j);
                primaryIndex.insert(key, inputSource[thread_id][pos++]);
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
    setAffinity(-1);
    //    setSched(SCHED_FIFO);
    //    cerr << "scheduler set "<<endl;
    std::string name;
#ifdef CCSI
    name = "CCSI-";
    ConcurrentCuckooSecondaryIndex<TKey, TVal, PKey>::store = new ConcurrentCuckooSecondaryIndex<TKey, TVal, PKey>::PoolType();
#else
    name = "CSI-";
    CuckooSecondaryIndex<TKey, TVal, PKey>::store = new CuckooSecondaryIndex<TKey, TVal, PKey>::PoolType();
#endif
    name += std::to_string(numThreads) + "-" + std::to_string(jMax);
    for (int i = 0; i < initRows; ++i) {
        for (int j = 0; j < jMax; ++j) {
            index->insert(initSource[i * jMax + j], TVal(0));
        }
    }
    auto startTime = Now, endTime = startTime;
    auto callerBody = [&]() {

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
    };

#ifdef PERF_STAT
    System::perfStat(name, callerBody);
#elif defined(PERF_RECORD)
    System::perfRecord(name, callerBody);
#else
    callerBody();
#endif
    return DurationUS(endTime - startTime) / 1000.0;
}

double CallerPrimary() {
    reset();
    auto startTime = Now, endTime = startTime;
    setAffinity(-1);

#if defined(CPI) || defined(MPI)
    for (int i = 0; i < initRows; ++i) {
        for (int j = 0; j < jMax; ++j) {
            TKey key(i, j);
            primaryIndex.insert(key, initSource[i * jMax + j]);
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
    cout << "PID  = " << ::getpid() << endl;
    std::cout.imbue(std::locale(""));
    ofstream fout("out", ios::app), header("header");
    header << "SI,numThreads,jMax,numOps,Throughput";
    int total;
    initSource = new TEntry* [initRows * jMax];
    for (uint i = 0; i < initRows; ++i) {
        for (uint j = 0; j < jMax; ++j) {
            initSource[i * jMax + j] = new (entryPool.allocate()) TEntry(nullptr, TKey(i, j));
        }
    }
    const uint entryPerThread = ((iMax - initRows) / numThreads + 1) * jMax;
    int sourceCounter[numThreads];
    for (uint t = 0; t < numThreads; ++t) {
        inputSource[t] = new TEntry*[entryPerThread];
        sourceCounter[t] = 0;
    }

    for (uint i = initRows; i < iMax; ++i) {
        int tid = (i - initRows) % numThreads;
        for (uint j = 0; j < jMax; ++j) {
            int pos = sourceCounter[tid]++;
            inputSource[ tid ][pos] = new(entryPool.allocate()) TEntry(nullptr, TKey(i, j));
            if (pos >= entryPerThread) {
                cerr << "overflow" << endl;
                exit(1);
            }
        }
    }
    cout << "Number of input entries per thread : ";
    for (int t = 0; t < numThreads; ++t) {
        cout << "   " << t << ": " << sourceCounter[t];
    }
    cout << endl;

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