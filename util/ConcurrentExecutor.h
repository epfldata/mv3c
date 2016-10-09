#ifndef CONCURRENTEXECUTOR_H
#define CONCURRENTEXECUTOR_H
#include "types.h"
#include "Program.h"
#include <functional>
#include <thread>
#include <sched.h>
#include <atomic>
#include <pthread.h>
#include <vector>
#include "Timer.h"
#include "TransactionManager.h"

struct ConcurrentExecutor {
    std::thread* workers;
    volatile bool* isReady;
    volatile bool startExecution;
    Timepoint startTime, endTime;
    uint numPrograms, numThreads;
    size_t timeMs;
    Program ** programs;
    std::atomic<uint> nextProgram;
    TransactionManager& tm;
    ConcurrentExecutor(int numThr, TransactionManager& TM): tm(TM) {
        numThreads = numThr;
        workers = new std::thread[numThreads];
        isReady = new volatile bool[numThreads];
        startExecution = false;
        for (int i = 0; i < numThreads; ++i) {
            isReady[i] = 0;
        }
    }

    void execute(Program** progs, uint numProgs) {
        nextProgram = 0;
        programs = progs;
        numPrograms = numProgs;
        for(uint i = 0; i < numThreads; ++i){
            workers[i] = std::thread(&ConcurrentExecutor::threadFunction, this, i);
        }
        bool all_ready = true;
        while(true){
            for(uint i = 0; i < numThreads; ++i){
                if(isReady[i] == false){
                    all_ready = false;
                    break;
                }
            }
            if(all_ready){
                startExecution = true;
                break;
            }
            all_ready = true;
        }
        for(uint i = 0; i < numThreads; ++i){
            workers[i].join();
        }
        timeMs = DurationMS(endTime - startTime);
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

        isReady[thread_id] = true;
        while (!startExecution);
        auto pid = nextProgram++;
        if (pid == 0) {
            startTime = Now;
        }
        while (pid < numPrograms) {
            Program *p = programs[pid];
            executeProgram(p);
            pid = nextProgram++;
        }
        if(pid == numPrograms + numThreads -1){
            endTime = Now;
            cout << "FINISHED" << endl;
        }
    }

    void executeProgram(Program *p) {
//        p->xact = tm.begin();
        p->execute();
        tm.commit(p->xact);
        p->cleanUp();
    }

    ~ConcurrentExecutor() {
        delete[] isReady;
        delete[] workers;
    }
};


#endif /* CONCURRENTEXECUTOR_H */

