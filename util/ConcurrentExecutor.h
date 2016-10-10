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
    volatile bool startExecution;
    Timepoint startTime, endTime;
    uint numThreads, progPerThread;
    size_t timeMs;
    Program *** programs;
    TransactionManager& tm;

    ConcurrentExecutor(int numThr, TransactionManager& TM) : tm(TM) {
        numThreads = numThr;
        programs = new Program**[numThreads];
        workers = new std::thread[numThreads];
        isReady = new volatile bool[numThreads];
        startExecution = false;
        for (int i = 0; i < numThreads; ++i) {
            isReady[i] = 0;
        }
    }

    void execute(Program** progs, uint numProgs) {
        int prog = 0;
        progPerThread = numProgs / numThreads + 1;
        for (uint i = 0; i < numThreads; ++i) {
            programs[i] = new Program*[numProgs / numThreads + 1];
        }
        for (uint i = 0; i < numThreads; ++i) {
            for (uint j = 0; j < progPerThread; ++j) {
                programs[i][j] = (prog < numProgs) ? progs[prog] : nullptr;
                prog++;
            }
        }

        for (uint i = 0; i < numThreads; ++i) {
            workers[i] = std::thread(&ConcurrentExecutor::threadFunction, this, i);
        }
        bool all_ready = true;
        while (true) {
            for (uint i = 0; i < numThreads; ++i) {
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
        for (uint i = 0; i < numThreads; ++i) {
            workers[i].join();
        }
        endTime = Now;
        timeMs = DurationUS(endTime - startTime);
    }

    void threadFunction(int thread_id) {
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

        while (pid < ppt && (p = threadPrgs[pid])) {
            threadPrgs[pid]->threadVar = &threadVar;
            pid++;
        }
        isReady[thread_id] = true;
        while (!startExecution);
        pid = 0;

        while (pid < ppt && (p = threadPrgs[pid])) {
            tm.begin(&p->xact);
            executeProgram(p);
            pid++;
            tm.commit(&p->xact);
        }
    }

    forceinline void executeProgram(Program *p) {
        //        p->xact = tm.begin();
        p->execute();
        //        tm.commit(p->xact);
        //        p->cleanUp();
    }

    ~ConcurrentExecutor() {
        delete[] isReady;
        delete[] workers;
    }
};


#endif /* CONCURRENTEXECUTOR_H */

