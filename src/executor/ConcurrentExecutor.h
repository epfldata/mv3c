#ifndef CONCURRENTEXECUTOR_H
#define CONCURRENTEXECUTOR_H
#include "util/types.h"
#include "util/Program.h"
#include <functional>
#include <thread>
#include <sched.h>
#include <pthread.h>
#include <vector>
#include "profile/Timer.h"
#include "transaction/TransactionManager.h"
#include "tpcc/NewMV3CTpcc.h"
#include "banking/MV3CBanking.h"
#include "trading/mv3cTrading.h"
#include "tpcc2/mv3ctpcc2.h"

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
        //Assign programs to threads
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
        //check if all worker threads are ready. Execution can be started once all threads finish startup procedure
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

    //Function executed by each thread.
    //It performs some initialization operations, wait for other threads and then starts execution

    void threadFunction(uint8_t thread_id) {
        //        const int CPUS[] = {0, 1, 2, 3};
        setAffinity(thread_id);
        setSched(SCHED_FIFO);
        ThreadLocal threadVar; //Program specific variables that are local to each thread
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

        //Signal that this thread is ready and wait for others
        isReady[thread_id] = true;
        while (!startExecution);

        //Start execution
        pid = 0;
        uint thisPrgFailedExec = 0;
        uint thisPrgFailedVal = 0;

        //Execution is stopped as soon as the first thread finishes its programs
        while (pid < ppt && (p = threadPrgs[pid]) && !hasFinished) {
            tm.begin(&p->xact);
            auto dstart = DNow;
            auto status = p->execute();
            auto dend = DNow;
            p->xact.executeTime += DDurationNS(dend - dstart);
            if (status != SUCCESS) { //if execution fails, rollback and restart
                //                 cerr << "Thread "<<thread_id<< " aborted " << pid << endl;
                tm.rollback(&p->xact);
                thisPrgFailedExec++;
                failedExPerTxn[p->prgType]++;
                if (thisPrgFailedExec > maxFailedExecutionProgram) {
                    maxFailedExecutionProgram = thisPrgFailedExec;
                }
                if (thisPrgFailedExec > numPrograms) {
                    //Debug info. We really dont want a single program to fail more than the total number of programs
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!";
                    cout << p->xact.failureCtr << "  " << thisPrgFailedExec << "  " << p->prgType << endl;
                    hasFinished = true;
                }
            } else {
                //successful execution, now do validate and commit.
#if OMVCC
                if (!tm.validateAndCommit(&p->xact, p)) {
                    //validation unsucessful. rollback and restart
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
                //validate and if it fails do compensate externally
                while (!tm.validateAndCommit(&p->xact, p, critical_compensate)) {
                    failedValPerTxn[p->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > maxFailedValidationProgram) {
                        maxFailedValidationProgram = thisPrgFailedVal;
                    }
                    if (critical_compensate)
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
                    //If a transaction fails validation more than a threshold, next time, we repair it within the critical section
                    if (thisPrgFailedVal > critical_compensate_threshold)
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

