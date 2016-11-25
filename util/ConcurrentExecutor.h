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
#include "mv3ctpcc2.h"

struct ConcurrentExecutor {
    std::thread* workers;
    volatile bool* isReady;
    volatile bool startExecution, hasFinished;
    std::atomic<uint> finishedPrograms;
    std::atomic<uint> failedExecution;
    std::atomic<uint> failedValidation;
    uint8_t numThreads;
    uint* failedExPerThread[txnTypes];
    uint* failedValPerThread[txnTypes];
    uint* finishedPerThread[txnTypes];
    uint* maxFailedExSingleProgram;
    uint* maxFailedValSingleProgram;
    Timepoint startTime, endTime;
    uint progPerThread;
    size_t timeUs;
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
        for (uint i = 0; i < txnTypes; ++i) {
            finishedPerThread[i] = new uint[numThreads];
            failedExPerThread[i] = new uint[numThreads];
            failedValPerThread[i] = new uint[numThreads];
        }

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
        timeUs = DurationUS(endTime - startTime);
    }

    void threadFunction(uint8_t thread_id) {
        //        const int CPUS[] = {0, 1, 2, 3};
        setAffinity(thread_id);
        setSched(SCHED_FIFO);
        ThreadLocal threadVar;
#ifdef BANKING_TEST
        threadVar.from = AccountKey(thread_id * 2);
        threadVar.to = AccountKey(thread_id * 2 + 1);
#endif
        Program ** threadPrgs = programs[thread_id];
        Program *p;
        auto ppt = progPerThread;
        uint pid = 0;

        uint finishedPerTxn[txnTypes];
        uint failedExPerTxn[txnTypes];
        uint failedValPerTxn[txnTypes];
        for (uint i = 0; i < txnTypes; ++i) {
            failedValPerTxn[i] = 0;
            finishedPerTxn[i] = 0;
        }
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
        uint thisPrgFailedVal = 0;
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
                if (thisPrgFailedExec > numPrograms) {
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!";
                    cout << p->xact.failureCtr << "  " << thisPrgFailedExec << "  " << p->prgType << endl;
                    hasFinished = true;
                }
            } else {

#if OMVCC
                if (!tm.validateAndCommit(&p->xact, p)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > maxFailedValidationProgram) {
                        maxFailedValidationProgram = thisPrgFailedVal;
                    }
                    if (thisPrgFailedVal > 10000) {
                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
                        hasFinished = true;
                    }
                    continue;
                }
#else
                const int critical_compensate_threshold = CRITICAL_COMPENSATE_THRESHOLD;
                bool critical_compensate = false;
                while (!tm.validateAndCommit(&p->xact, p, critical_compensate)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > maxFailedValidationProgram) {
                        maxFailedValidationProgram = thisPrgFailedVal;
                    }
                    if(critical_compensate)
                        break;
//                    if (thisPrgFailedVal > 10000) {
//                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
//                        hasFinished = true;
//                    }
                    auto status = tm.reexecute(&p->xact, p);
                    if (status != SUCCESS) {
                        cerr << "Cannot fail execution during reexecution" << endl;
                        exit(-5);
                    }
                    if(thisPrgFailedVal > critical_compensate_threshold)
                        critical_compensate = true;
                }

#endif
                finishedPerTxn[p->prgType]++;
                pid++;
                thisPrgFailedExec = 0;
                thisPrgFailedVal = 0;
            }
        }
        hasFinished = true;
        for (uint i = 0; i < txnTypes; ++i) {
            finishedPrograms += finishedPerTxn[i];
            failedExecution += failedExPerTxn[i];
            failedValidation += failedValPerTxn[i];
            finishedPerThread[i][thread_id] = finishedPerTxn[i];
            failedExPerThread[i][thread_id] = failedExPerTxn[i];
            failedValPerThread[i][thread_id] = failedValPerTxn[i];
        }
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

