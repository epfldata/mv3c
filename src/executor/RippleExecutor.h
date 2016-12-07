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

//INCOMPLETE

struct ScheduledProgram {
    size_t offset;
    Program *p;
};

struct RippleExecutor {
    std::thread workers[numThreads];
    volatile bool isReady[numThreads];
    volatile bool startExecution, hasFinished;
    std::atomic<uint> finishedPrograms;
    std::atomic<uint> failedExecution;
    std::atomic<uint> failedValidation;
    Program* currentProgram[numThreads];
    uint failedExPerThread[txnTypes][numThreads];
    uint failedValPerThread[txnTypes][numThreads];
    uint finishedPerThread[txnTypes][numThreads];
    uint maxFailedExSingleProgram[numThreads];
    uint maxFailedValSingleProgram[numThreads];
    Timepoint startTime, endTime;
    size_t timeMs;
    TransactionManager& tm;

    RippleExecutor(TransactionManager& TM) : tm(TM) {
        hasFinished = false;
        finishedPrograms = 0;
        failedExecution = 0;
        failedValidation = 0;

        startExecution = false;

        for (uint8_t i = 0; i < numThreads; ++i) {
            isReady[i] = 0;
            currentProgram[i] = nullptr;
        }
    }

    void execute(ScheduledProgram* progs, uint numProgs) {

        for (uint8_t i = 0; i < numThreads; ++i) {
            workers[i] = std::thread(&RippleExecutor::threadFunction, this, i);
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
        uint pid = 0;
        while (pid < numProgs) {
            //TODO !!!!!!!!!!!
        }


        for (uint8_t i = 0; i < numThreads; ++i) {
            workers[i].join();
        }
        endTime = Now;
        timeMs = DurationUS(endTime - startTime);
    }

    void threadFunction(uint8_t thread_id) {
        //        const int CPUS[] = {0, 1, 2, 3};
        setAffinity(thread_id);
        ThreadLocal threadVar;
        //#ifdef BANKING_TEST
        threadVar.from = AccountKey(thread_id * 2);
        threadVar.to = AccountKey(thread_id * 2 + 1);
        //#endif

        Program *cur, *old = nullptr;



        uint finishedPerTxn[txnTypes];
        uint failedExPerTxn[txnTypes];
        uint failedValPerTxn[txnTypes];
        for (uint i = 0; i < txnTypes; ++i) {
            failedValPerTxn[i] = 0;
            finishedPerTxn[i] = 0;
        }
        uint maxFailedExecutionProgram = 0;
        uint maxFailedValidationProgram = 0;

        isReady[thread_id] = true;
        while (!startExecution);

        uint thisPrgFailedExec = 0;
        while (!hasFinished) {
            while ((cur = currentProgram[thread_id]) == old);
            cur->threadVar = &threadVar;
            cur->xact.threadId = thread_id + 1;


            tm.begin(&cur->xact);
            auto dstart = DNow;
            auto status = cur->execute();
            auto dend = DNow;
            cur->xact.executeTime += DDurationNS(dend - dstart);
            if (status != SUCCESS) {
                //                 cerr << "Thread "<<thread_id<< " aborted " << pid << endl;
                tm.rollback(&cur->xact);
                thisPrgFailedExec++;
                failedExPerTxn[cur->prgType]++;
                if (thisPrgFailedExec > maxFailedExecutionProgram) {
                    maxFailedExecutionProgram = thisPrgFailedExec;
                }
                if (cur->xact.failureCtr > 1000 || thisPrgFailedExec > numPrograms) {
                    cout << "TOO MANY FAILURES !!!!!!!!!!!!!!!!!!!";
                    cout << cur->xact.failureCtr << "  " << thisPrgFailedExec << "  " << cur->prgType << endl;
                    hasFinished = true;
                }
            } else {
                uint thisPrgFailedVal = 0;
#if OMVCC
                if (!tm.validateAndCommit(&cur->xact, cur)) {
                    failedValPerTxn[cur->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > 100) {
                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
                        hasFinished = true;
                    }
                    continue;
                }
#else
                const int critical_compensate_threshold = CRITICAL_COMPENSATE_THRESHOLD;
                bool critical_compensate = false;
                while (!tm.validateAndCommit(&cur->xact, cur, critical_compensate)) {
                    failedValPerTxn[cur->prgType]++;
                    thisPrgFailedVal++;
                    if (thisPrgFailedVal > maxFailedValidationProgram) {
                        maxFailedValidationProgram = thisPrgFailedVal;
                    }
                    if (thisPrgFailedVal > 100) {
                        cout << " !!!!!!!!!!!!!!!!!!!!!!!!!!!TOO MANY VALIDATION FAILURE !!!!!!!" << endl;
                        hasFinished = true;
                    }
                    if (critical_compensate)
                        break;

                    auto status = tm.reexecute(&cur->xact, cur);
                    if (status != SUCCESS) {
                        cerr << "Cannot fail execution during reexecution" << endl;
                        exit(-5);
                    }
                    if (thisPrgFailedVal > critical_compensate_threshold)
                        critical_compensate = true;
                }
#endif
                finishedPerTxn[cur->prgType]++;
                thisPrgFailedExec = 0;
            }
        }

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

};


#endif /* CONCURRENTEXECUTOR_H */

