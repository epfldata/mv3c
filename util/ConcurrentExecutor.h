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
    uint* failedPerThread[2];
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
        failedPerThread[0] = new uint[numThreads];
        failedPerThread[1] = new uint[numThreads];
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
        uint failedex = 0, finished = 0, failedval = 0;
        uint failed2[2];
        failed2[0] = 0;
        failed2[1] = 0;
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
                failedex++;
                failed2[p->prgType]++;
                if (p->xact.failureCtr > 100) {
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!" << endl;
                    hasFinished = true;
                }
            } else {

                if (tm.validateAndCommit(&p->xact)) {
                    finished++;
                    //                cerr << "Thread "<<thread_id<< " finished " << pid << endl;
                    pid++;
                } else {
                    failedval++;
                }
            }

        }
        hasFinished = true;
        finishedPrograms += finished;
        failedExecution += failedex;
        failedValidation += failedval;
        failedPerThread[0][thread_id] = failed2[0];
        failedPerThread[1][thread_id] = failed2[1];
    }

    ~ConcurrentExecutor() {
        delete[] isReady;
        delete[] workers;
        for (uint8_t i = 0; i < numThreads; ++i) {
            delete[] programs[i];
        }
        delete[] programs;
    }
};


#endif /* CONCURRENTEXECUTOR_H */

