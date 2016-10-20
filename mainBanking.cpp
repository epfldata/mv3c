#include "types.h"
#ifdef BANKING_TEST
#include "Table.h"
#include "Predicate.hpp"
#include "Transaction.h"
#include "Tuple.h"
#include "ConcurrentExecutor.h"
#include <cstdlib>
#include "MV3CBanking.h"
#include "Banking.h"
#include <iomanip>
#include <locale>
#include <bits/unordered_map.h>
using namespace std;
using namespace Banking;
DefineStore(Account);

TransactionManager transactionManager;
TransactionManager& Transaction::tm(transactionManager);

TABLE(Account)* mv3cTransfer::AccountTable;
TABLE(Account)* mv3cTransferNoConflict::AccountTable;

int main(int argc, char** argv) {
    std::ofstream fout("out", ios::app);
    std::ofstream header("header");
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2 * numThreads, &cpuset);
    auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);
    if (s != 0) {
        throw std::runtime_error("Cannot set affinity");
    }
    BankDataGenerator bank;
    TABLE(Account) accountTable(AccountSize);

    bank.loadPrograms();
    bank.loadData();
    header << "BenchName, Algo, Critical Compensate, Validation level, WW allowed, Store enabled, ConflictFraction";
    cout << "Banking" << endl;
    fout << "Banking";
#if OMVCC
    cout << "OMVCC" << endl;
    fout << ", OMVCC, ";
#else
    cout << "MV3C" << endl;
    fout << ", MV3C";
#if(CRITICAL_COMPENSATE)
    cout << "Compensate done inside critical section" << endl;
    fout << ", Y";
#else
    cout << "Compensate done outside critical section" << endl;
    fout << ", N";
#endif
#endif
#ifdef ATTRIB_LEVEL
    cout << "Attribute level validation " << endl;
    fout << ", A";
#else
    cout << "Record level validation" << endl;
    fout << ", R";
#endif
#if (ALLOW_WW)
    cout << "WW  handling enabled" << endl;
    fout << ", Y";
#else 
    cout << "WW handling disabled" << endl;
    fout << ", N";
#endif
#ifdef STORE_ENABLE
    cout << "Store enabled " << endl;
    fout << ", Y";
#else
    cout << "Store disabled " << endl;
    fout << ", N";
#endif
    cout << "Fraction of conflict = " << CONFLICT_FRACTION << endl;
    fout << ", " << CONFLICT_FRACTION;
    Transaction t;
    Transaction *t0 = &t;
    transactionManager.begin(t0);

    for (const auto &it : bank.iAccounts) {
        accountTable.insertVal(t0, it.first, it.second);
    }

    transactionManager.validateAndCommit(t0);
    std::cout.imbue(std::locale(""));
    std::cerr.imbue(std::locale(""));
    header << ", NumProgs, NumThreads";
    fout << ", " << numPrograms << ", " << numThreads;
    cout << "Number of programs = " << numPrograms << endl;
    cout << "Number of threads = " << numThreads << endl;

    Program **programs = new Program*[numPrograms];
    mv3cTransfer::AccountTable = &accountTable;
    mv3cTransferNoConflict::AccountTable = &accountTable;


    for (uint i = 0; i < numPrograms; ++i) {
        Program * p = new mv3cTransfer(bank.transfers[i]);
        programs[i] = p;
    }

    for (uint i = 0; i < numPrograms; i++) {
        float prob = ((float) rand()) / RAND_MAX;
        if (prob < CONFLICT_FRACTION)
            programs[i] = new mv3cTransfer(bank.transfers[i]);
        else
            programs[i] = new mv3cTransferNoConflict(bank.transfers[i]);

    }
    ConcurrentExecutor exec(numThreads, transactionManager);
    exec.execute(programs, numPrograms);
    header << ", Duration(ms), Committed, FailedEx, FailedVal, FailedExRate, FailedValRate, Throughput(ktps), numValidations, numValAgainst, avgValAgainst, AvgValRound";
    cout << "Duration = " << exec.timeMs << endl;
    fout << ", " << exec.timeMs;
    cout << "Committed = " << exec.finishedPrograms << endl;
    fout << ", " << exec.finishedPrograms;
    cout << "FailedExecution  = " << exec.failedExecution << endl;
    fout << ", " << exec.failedExecution;
    cout << "FailedValidation  = " << exec.failedValidation << endl;
    fout << ", " << exec.failedValidation;
    cout << "FailedEx rate = " << 1.0 * exec.failedExecution / exec.finishedPrograms << endl;
    fout << ", " << 1.0 * exec.failedExecution / exec.finishedPrograms;
    cout << "FailedVal rate = " << 1.0 * exec.failedValidation / exec.finishedPrograms << endl;
    fout << ", " << 1.0 * exec.failedValidation / exec.finishedPrograms;
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeMs) << " K tps" << endl;
    fout << ", " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeMs);

    size_t commitTime = 0, validateTime = 0, executeTime = 0, compensateTime = 0;
    size_t commitTimes[2], validateTimes[2], executeTimes[2], compensateTimes[2];
    commitTimes[0] = validateTimes[0] = executeTimes[0] = compensateTimes[0] = 0;
    commitTimes[1] = validateTimes[1] = executeTimes[1] = compensateTimes[1] = 0;
    size_t numValidations = 0;
    size_t numXactsValidatedAgainst = 0;
    size_t numRounds = 0;
    for (uint i = 0; i < numPrograms; ++i) {
        Transaction& x = programs[i]->xact;
        numValidations += x.numValidations;
        numXactsValidatedAgainst += x.numXactsValidatedAgainst;
        numRounds += x.numRounds;
        int id = programs[i]->prgType;
        commitTime += x.commitTime;
        executeTime += x.executeTime;
        validateTime += x.validateTime;
        compensateTime += x.compensateTime;
        commitTimes[id] += x.commitTime;
        executeTimes[id] += x.executeTime;
        validateTimes[id] += x.validateTime;
        compensateTimes[id] += x.compensateTime;
    }   
    cout << "Num validations  = " << numValidations << endl;
    fout << ", " << numValidations;
    cout << "Num validations against = " << numXactsValidatedAgainst << endl;
    fout << ", " << numXactsValidatedAgainst;
    cout << "avg validations against = " << numXactsValidatedAgainst / (1.0 * numValidations) << endl;
    fout << ", " << numXactsValidatedAgainst / (1.0 * numValidations);
    cout << "avg validation rounds = " << numRounds / (1.0 * numValidations) << endl;
    fout << ", " << numRounds / (1.0 * numValidations);
    
    
    header << ", Exec time(ms), Val time(ms), Commit Time(ms), Compensate Time(ms)";
    cout << "Execution time = " << executeTime / 1000000.0 << " ms" << endl;
    fout << ", " << executeTime / 1000000.0;
    cout << "Validation time = " << validateTime / 1000000.0 << " ms" << endl;
    fout << ", " << validateTime / 1000000.0;
    cout << "Commit time = " << commitTime / 1000000.0 << " ms" << endl;
    fout << ", " << commitTime / 1000000.0;
    cout << "Compensate time = " << compensateTime / 1000000.0 << " ms" << endl;
    fout << ", " << compensateTime / 1000000.0;
    header << ", exec T, exec TNC, val T, val TNC, comt T, comt TNC, comp T, comp TNC ";
    cout << "Execution times = " << executeTimes[0] / 1000000.0 << "   " << executeTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << executeTimes[0] / 1000000.0 << ", " << executeTimes[1] / 1000000.0;
    cout << "Validation times = " << validateTimes[0] / 1000000.0 << "   " << validateTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << validateTimes[0] / 1000000.0 << ", " << validateTimes[1] / 1000000.0;
    cout << "Commit times = " << commitTimes[0] / 1000000.0 << "   " << commitTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << commitTimes[0] / 1000000.0 << ", " << commitTimes[1] / 1000000.0;
    cout << "Compensate times = " << compensateTimes[0] / 1000000.0 << "   " << compensateTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << compensateTimes[0] / 1000000.0 << ", " << compensateTimes[1] / 1000000.0;

    Transaction tf;
    transactionManager.begin(&tf);
    for (uint i = 0; i < AccountSize; ++i) {
        AccountKey key(i);
        auto adv = accountTable.getReadOnly(&tf, key);
        bank.fAccounts.insert({key, adv->val});
    }
    //    bank.checkData();
    auto val = bank.fAccounts.at(FeeAccount)._1;
    size_t total = 0;
    for (uint i = 0; i < numThreads; ++i)
        total += exec.finishedPerThread[0][i];
    if (val != total * 2) {
        cerr << "Fee account is " << (size_t) val << "  instead of " << exec.finishedPrograms * 2 << endl;
    }
    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;

    for (int i = 0; i < numThreads; ++i) {
        cout << "Thread " << i << endl;
        header << ", Thread " << i << ", finished T, finished TNC, failedEx T, failedEx TNC, failedVal T, failedVal TNC, maxFailedEx, maxFailedVal";
        fout << ", ";
        cout << "\t Finished     T:" << exec.finishedPerThread[0][i] << "  TNC:" << exec.finishedPerThread[1][i] << endl;
        fout << ", " << exec.finishedPerThread[0][i] << ", " << exec.finishedPerThread[1][i];
        cout << "\t FailedEx     T:" << exec.failedExPerThread[0][i] << "  TNC:" << exec.failedExPerThread[1][i] << endl;
        fout << ", " << exec.failedExPerThread[0][i] << ", " << exec.failedExPerThread[1][i];
        cout << "\t FailedVal    T:" << exec.failedValPerThread[0][i] << "  TNC:" << exec.failedValPerThread[1][i] << endl;
        fout << ", " << exec.failedValPerThread[0][i] << ", " << exec.failedValPerThread[1][i];
        cout << "\t Max failed exec = " << exec.maxFailedExSingleProgram[i] << endl;
        fout << ", " << exec.maxFailedExSingleProgram[i];
        cout << "\t Max failed val = " << exec.maxFailedValSingleProgram[i] << endl;
        fout << ", " << exec.maxFailedValSingleProgram[i];
    }
    fout << endl;
    header << endl;
    fout.close();
    header.close();
    return 0;
}
#endif