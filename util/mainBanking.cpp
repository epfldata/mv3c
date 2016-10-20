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
    cout << "Banking" << endl;
#if OMVCC
    cout << "OMVCC" << endl;
#else
    cout << "MV3C" << endl;
    if (CRITICAL_COMPENSATE)
        cout << "Compensate done inside critical section" << endl;
    else
        cout << "Compensate done outside critical section" << endl;
#endif
#ifdef ATTRIB_LEVEL
    cout << "Attribute level validation " << endl;
#else
    cout << "Record level validation" << endl;
#endif
    if (ALLOW_WW)
        cout << "WW  handling enabled" << endl;
    else cout << "WW handling disabled" << endl;

#ifdef STORE_ENABLE
    cout << "Store enabled " << endl;
#else
    cout << "Store disabled " << endl;
#endif
    cout << "Fraction of conflict = " << CONFLICT_FRACTION << endl;
    Transaction t;
    Transaction *t0 = &t;
    transactionManager.begin(t0);

    for (const auto &it : bank.iAccounts) {
        accountTable.insertVal(t0, it.first, it.second);
    }

    transactionManager.validateAndCommit(t0);
    std::cout.imbue(std::locale(""));
    std::cerr.imbue(std::locale(""));
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
    cout << "Duration = " << exec.timeMs << endl;
    cout << "Committed = " << exec.finishedPrograms << endl;
    cout << "FailedExecution  = " << exec.failedExecution << endl;
    cout << "FailedValidation  = " << exec.failedValidation << endl;

    cout << "FailedEx rate = " << 1.0 * exec.failedExecution / exec.finishedPrograms << endl;
    cout << "FailedVal rate = " << 1.0 * exec.failedValidation / exec.finishedPrograms << endl;
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeMs) << " K tps" << endl;
    cout << "Num validations  = " << transactionManager.numValidations << endl;
    cout << "Num validations against = " << transactionManager.numXactsValidatedAgainst << endl;
    cout << "avg validations against = " << transactionManager.numXactsValidatedAgainst / (1.0 * transactionManager.numValidations) << endl;
    cout << "avg validation rounds = " << transactionManager.numRounds / (1.0 * transactionManager.numValidations) << endl;
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
        cout << "\t Finished     NO:" << exec.finishedPerThread[0][i] << "  PY:" << exec.finishedPerThread[1][i] << endl;
        cout << "\t FailedEx     NO:" << exec.failedExPerThread[0][i] << "  PY:" << exec.failedExPerThread[1][i] << endl;
        cout << "\t FailedVal    NO:" << exec.failedValPerThread[0][i] << "  PY:" << exec.failedValPerThread[1][i] << endl;
        cout << "\t Max failed exec = " << exec.maxFailedExSingleProgram[i] << endl;
        cout << "\t Max failed val = " << exec.maxFailedValSingleProgram[i] << endl;
    }
    return 0;
}
#endif