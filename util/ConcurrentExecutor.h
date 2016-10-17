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
        failedExPerThread[0] = new uint[numThreads];
        failedExPerThread[1] = new uint[numThreads];
        failedValPerThread[0] = new uint[numThreads];
        failedValPerThread[1] = new uint[numThreads];
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
        uint finished = 0;
        uint failedExPerTxn[2];
        uint failedValPerTxn[2];
        failedExPerTxn[0] = 0;
        failedExPerTxn[1] = 0;
        failedValPerTxn[0] = 0;
        failedValPerTxn[1] = 0;
        while (pid < ppt && (p = threadPrgs[pid])) {
            threadPrgs[pid]->threadVar = &threadVar;
            threadPrgs[pid]->xact.threadId = thread_id + 1;
            pid++;
        }
        isReady[thread_id] = true;
        while (!startExecution);
        pid = 0;

        while (pid < ppt && (p = threadPrgs[pid]) && !hasFinished) {
            tm.begin(&p->xact);
            auto status = p->execute();
            if (status != SUCCESS) {
                //                 cerr << "Thread "<<thread_id<< " aborted " << pid << endl;
                tm.rollback(&p->xact);
                failedExPerTxn[p->prgType]++;
                if (p->xact.failureCtr > 100) {
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!" << endl;
                    hasFinished = true;
                }
            } else {
                uint thisPrgFailedVal = 0;
#if OMVCC
                if(!tm.validateAndCommit(&p->xact, p)){
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    continue;
                }
#else
                while (!tm.validateAndCommit(&p->xact, p)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > 20) {
                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
                        hasFinished = true;
                    }
                    auto status = tm.reexecute(&p->xact, p);
                    if (status != SUCCESS) {
                        cerr << "Cannot fail execution during reexecution" << endl;
                        exit(-5);
                    }
                }
#endif
                finished++;
                pid++;
            }
        }
        hasFinished = true;
        finishedPrograms += finished;
        failedExecution += failedExPerTxn[0] + failedExPerTxn[1];
        failedValidation += failedValPerTxn[0] + failedValPerTxn[1];
        failedExPerThread[0][thread_id] = failedExPerTxn[0];
        failedExPerThread[1][thread_id] = failedExPerTxn[1];
        failedValPerThread[0][thread_id] = failedValPerTxn[0];
        failedValPerThread[1][thread_id] = failedValPerTxn[1];
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
    }
};


#endif /* CONCURRENTEXECUTOR_H */

