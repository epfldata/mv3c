#ifndef CONCURRENTEXECUTOR_H
#define CONCURRENTEXECUTOR_H
#include "types.h"
#include "Program.h"
#include <functional>
#include <thread>
#include <sched.h>
#include <pthread.h>
#include <vector>
#include "Timer.h"
#include "TransactionManager.h"
#include "NewMV3CTpcc.h"
#include "MV3CBanking.h"
#include "mv3cTrading.h"

struct ConcurrentExecutor {
    std::thread* workers;
    volatile bool* isReady;
    volatile bool startExecution, hasFinished;
    std::atomic<uint> finishedPrograms;
    std::atomic<uint> failedExecution;
    std::atomic<uint> failedValidation;
    uint8_t numThreads;
    uint* failedExPerThread[2];
    uint* failedValPerThread[2];
    uint* finishedPerThread[2];
    uint* maxFailedExSingleProgram;
    uint* maxFailedValSingleProgram;
    Timepoint startTime, endTime;
    uint progPerThread;
    size_t timeMs;
    Program *** programs;
    TransactionManager& tm;

    ConcurrentExecutor(uint8_t numThr, TransactionManager& TM) : tm(TM) {
        numThreads = numThr;
        hasFinished = false;
        finishedPrograms = 0;
        failedExecution = 0;
        failedValidation = 0;
        programs = new Program**[numThreads];
        workers = new std::thread[numThreads];
        isReady = new volatile bool[numThreads];
        startExecution = false;
        finishedPerThread[0] = new uint[numThreads];
        finishedPerThread[1] = new uint[numThreads];
        failedExPerThread[0] = new uint[numThreads];
        failedExPerThread[1] = new uint[numThreads];
        failedValPerThread[0] = new uint[numThreads];
        failedValPerThread[1] = new uint[numThreads];
        maxFailedExSingleProgram = new uint[numThreads];
        maxFailedValSingleProgram = new uint[numThreads];
        for (uint8_t i = 0; i < numThreads; ++i) {
            isReady[i] = 0;
        }
    }

    void execute(Program** progs, uint numProgs) {
        int prog = 0;
        progPerThread = numProgs / numThreads + 1;
        for (uint8_t i = 0; i < numThreads; ++i) {
            programs[i] = new Program*[numProgs / numThreads + 1];
        }
        for (uint8_t i = 0; i < numThreads; ++i) {
            for (uint j = 0; j < progPerThread; ++j) {
                programs[i][j] = (prog < numProgs) ? progs[prog] : nullptr;
                prog++;
            }
        }

        for (uint8_t i = 0; i < numThreads; ++i) {
            workers[i] = std::thread(&ConcurrentExecutor::threadFunction, this, i);
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
        timeMs = DurationUS(endTime - startTime);
    }

    void threadFunction(uint8_t thread_id) {
        //        const int CPUS[] = {0, 1, 2, 3};
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2 * thread_id, &cpuset);
        auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);
        if (s != 0) {
            throw std::runtime_error("Cannot set affinity");
        }
        ThreadLocal threadVar;
        Program ** threadPrgs = programs[thread_id];
        Program *p;
        auto ppt = progPerThread;
        uint pid = 0;
        uint finishedPerTxn[2];
        uint failedExPerTxn[2];
        uint failedValPerTxn[2];
        failedExPerTxn[0] = 0;
        failedExPerTxn[1] = 0;
        failedValPerTxn[0] = 0;
        failedValPerTxn[1] = 0;
        finishedPerTxn[0] = 0;
        finishedPerTxn[1] = 0;
        uint maxFailedExecutionProgram = 0;
        uint maxFailedValidationProgram = 0;
        while (pid < ppt && (p = threadPrgs[pid])) {
            threadPrgs[pid]->threadVar = &threadVar;
            threadPrgs[pid]->xact.threadId = thread_id + 1;
            pid++;
        }
        auto start = Now, end = Now;
        do {
            end = Now;
        } while (DurationMS(end - start) <= 2000);
        isReady[thread_id] = true;
        while (!startExecution);
        pid = 0;
        uint thisPrgFailedExec = 0;
        while (pid < ppt && (p = threadPrgs[pid]) && !hasFinished) {
            tm.begin(&p->xact);
            auto dstart = DNow;
            auto status = p->execute();
            auto dend = DNow;
            p->xact.executeTime += DDurationNS(dend - dstart);
            if (status != SUCCESS) {
                //                 cerr << "Thread "<<thread_id<< " aborted " << pid << endl;
                tm.rollback(&p->xact);
                thisPrgFailedExec++;
                failedExPerTxn[p->prgType]++;
                if (thisPrgFailedExec > maxFailedExecutionProgram) {
                    maxFailedExecutionProgram = thisPrgFailedExec;
                }
                if (p->xact.failureCtr > 1000) {
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!" << endl;
                    hasFinished = true;
                }
            } else {
                uint thisPrgFailedVal = 0;
#if OMVCC
                if (!tm.validateAndCommit(&p->xact, p)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    continue;
                }
#else
                while (!tm.validateAndCommit(&p->xact, p)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > maxFailedValidationProgram) {
                        maxFailedValidationProgram = thisPrgFailedVal;
                    }
                    if (thisPrgFailedVal > 100) {
                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
                        hasFinished = true;
                    }
#if CRITICAL_COMPENSATE
                    break;
#endif
                    auto status = tm.reexecute(&p->xact, p);
                    if (status != SUCCESS) {
                        cerr << "Cannot fail execution during reexecution" << endl;
                        exit(-5);
                    }
                }
#endif
                finishedPerTxn[p->prgType]++;
                pid++;
                thisPrgFailedExec = 0;
            }
        }
        hasFinished = true;
        finishedPrograms += finishedPerTxn[0] + finishedPerTxn[1];
        failedExecution += failedExPerTxn[0] + failedExPerTxn[1];
        failedValidation += failedValPerTxn[0] + failedValPerTxn[1];
        finishedPerThread[0][thread_id] = finishedPerTxn[0];
        finishedPerThread[1][thread_id] = finishedPerTxn[1];
        failedExPerThread[0][thread_id] = failedExPerTxn[0];
        failedExPerThread[1][thread_id] = failedExPerTxn[1];
        failedValPerThread[0][thread_id] = failedValPerTxn[0];
        failedValPerThread[1][thread_id] = failedValPerTxn[1];
        maxFailedExSingleProgram[thread_id] = maxFailedExecutionProgram;
        maxFailedValSingleProgram[thread_id] = maxFailedValidationProgram;
    }

    ~ConcurrentExecutor() {
        delete[] isReady;
        delete[] workers;
        for (uint8_t i = 0; i < numThreads; ++i) {
            delete[] programs[i];
        }
        delete[] programs;
        delete[] failedExPerThread[0];
        delete[] failedExPerThread[1];
        delete[] failedValPerThread[0];
        delete[] failedValPerThread[1];
        delete[] finishedPerThread[0];
        delete[] finishedPerThread[1];
        delete[] maxFailedExSingleProgram;
        delete[] maxFailedValSingleProgram;
    }
};


#endif /* CONCURRENTEXECUTOR_H */

