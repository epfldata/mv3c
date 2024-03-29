#include "util/types.h"
#ifdef TRADING_TEST
#include "storage/Table.h"
#include "predicate/Predicate.hpp"
#include "transaction/Transaction.h"
#include "util/Tuple.h"
#include "executor/ConcurrentExecutor.h"
#include <cstdlib>
#include "trading/mv3cTrading.h"
#include <iomanip>
#include <locale>
#include <bits/unordered_map.h>
using namespace std;
using namespace trading_ns;

DefineStore(Customer);
DefineStore(Security);
DefineStore(Trade);
DefineStore(TradeLine);


TransactionManager transactionManager;
TransactionManager& Transaction::tm(transactionManager);

int main(int argc, char** argv) {
#ifdef NB
    std::ofstream fout("out");
#else
    std::ofstream fout("out", ios::app);
#endif
    std::ofstream header("header");
    TradingDataGen trade;
    setAffinity(-1);
    TABLE(Customer) custTable(CustomerSize);
    TABLE(Security) secTable(SecuritySize);
    TABLE(Trade) tradeTable(TradeSize);
    TABLE(TradeLine) tradeLineTable(TradeLineSize);
    trade.loadCustomer();
    trade.loadPrograms();
    trade.loadSecurity();
    std::cout.imbue(std::locale(""));
    header << "BenchName,Algo,Critical Compensate,Validation level,WW allowed,Store enabled,Cuckoo enabled,Power,Malloc";
    cout << "Trading" << endl;
    fout << "Trading";
#if OMVCC
    cout << "OMVCC" << endl;
    fout << ",OMVCC,0";
#else
    cout << "MV3C" << endl;
    fout << ",MV3C";

    cout << "Critical Compensate threshold = " << CRITICAL_COMPENSATE_THRESHOLD << endl;
    fout << "," << CRITICAL_COMPENSATE_THRESHOLD;

#endif
#ifdef ATTRIB_LEVEL
    cout << "Attribute level validation " << endl;
    fout << ",A";
#else
    cout << "Record level validation" << endl;
    fout << ",R";
#endif
#if (ALLOW_WW)
    cout << "WW  handling enabled" << endl;
    fout << ",Y";
#else 
    cout << "WW handling disabled" << endl;
    fout << ",N";
#endif
#ifdef STORE_ENABLE
    cout << "Store enabled " << endl;
    fout << ",Y";
#else
    cout << "Store disabled " << endl;
    fout << ",N";
#endif
#if CUCKOO
    cout << "Cuckoo index" << endl;
    fout << ",Y";
#else
    cout << "UnorderedMap index" << endl;
    fout << ",N";
#endif
    cout << "Power factor = " << POWER << endl;
    fout << "," << POWER << "," << MALLOCTYPE;
    Transaction t;
    Transaction *t0 = &t;
    transactionManager.begin(t0);
    for (const auto&it : trade.iCustomer) {
        custTable.insertVal(t0, it.first, it.second);
    }
    for (const auto& it : trade.iSecurity) {
        secTable.insertVal(t0, it.first, it.second);
    }
    transactionManager.validateAndCommit(t0);

    Program **programs = new Program*[numPrograms];
    SecurityTable = &secTable;
    CustomerTable = &custTable;
    TradeLineTable = &tradeLineTable;
    TradeTable = &tradeTable;
    header << ",NumProgs,NumThreads";
    fout << "," << numPrograms << "," << numThreads;
    cout << "Number of programs = " << numPrograms << endl;
    cout << "Number of threads = " << numThreads << endl;
    for (uint i = 0; i < numPrograms; ++i) {
        Program *p = trade.programs[i];
        Program *newp;
        switch (p->prgType) {
            case PRICE_UPDATE:
                newp = new mv3cPriceUpdate(p);
                break;
            case TRADE_ORDER:
                newp = new mv3cTradeOrder(p);
                break;
            default:
                throw std::runtime_error("Unknown program");

        }
        programs[i] = newp;
    }
    ConcurrentExecutor exec(numThreads, transactionManager);
    exec.execute(programs, numPrograms);
    header << ",Duration(ms),Committed,FailedEx,FailedVal,FailedExRate,FailedValRate,Throughput(ktps),ScaledTime,numValidations,numValAgainst,avgValAgainst,AvgValRound";
    cout << "Duration = " << exec.timeUs / 1000 << endl;
    fout << "," << exec.timeUs / 1000;
    cout << "Committed = " << exec.finishedPrograms << endl;
    fout << "," << exec.finishedPrograms;
    cout << "FailedExecution  = " << exec.failedExecution << endl;
    fout << "," << exec.failedExecution;
    cout << "FailedValidation  = " << exec.failedValidation << endl;
    fout << "," << exec.failedValidation;
    cout << "FailedEx rate = " << 1.0 * exec.failedExecution / exec.finishedPrograms << endl;
    fout << "," << 1.0 * exec.failedExecution / exec.finishedPrograms;
    cout << "FailedVal rate = " << 1.0 * exec.failedValidation / exec.finishedPrograms << endl;
    fout << "," << 1.0 * exec.failedValidation / exec.finishedPrograms;
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeUs) << " K tps" << endl;
    fout << "," << (uint) (exec.finishedPrograms * 1000.0 / exec.timeUs);
    fout << "," << numPrograms * exec.timeUs / (exec.finishedPrograms * 1000.0);

    size_t commitTime = 0, validateTime = 0, executeTime = 0, compensateTime = 0;
    size_t commitTimes[txnTypes], validateTimes[txnTypes], executeTimes[txnTypes], compensateTimes[txnTypes];
    for (uint i = 0; i < txnTypes; ++i) {
        commitTimes[i] = validateTimes[i] = executeTimes[i] = compensateTimes[i] = 0;
    }

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
    fout << "," << numValidations;
    cout << "Num validations against = " << numXactsValidatedAgainst << endl;
    fout << "," << numXactsValidatedAgainst;
    cout << "avg validations against = " << numXactsValidatedAgainst / (1.0 * numValidations) << endl;
    fout << "," << numXactsValidatedAgainst / (1.0 * numValidations);
    cout << "avg validation rounds = " << numRounds / (1.0 * numValidations) << endl;
    fout << "," << numRounds / (1.0 * numValidations);


    header << ",Exec time(ms),Val time(ms),Commit Time(ms),Compensate Time(ms)";
    cout << "Execution time = " << executeTime / 1000000.0 << " ms" << endl;
    fout << "," << executeTime / 1000000.0;
    cout << "Validation time = " << validateTime / 1000000.0 << " ms" << endl;
    fout << "," << validateTime / 1000000.0;
    cout << "Commit time = " << commitTime / 1000000.0 << " ms" << endl;
    fout << "," << commitTime / 1000000.0;
    cout << "Compensate time = " << compensateTime / 1000000.0 << " ms" << endl;
    fout << "," << compensateTime / 1000000.0;


    //    header << ",exec TO,exec PU,val TO,val PU,comt TO,comt PU,comp TO,comp PU ";
    //    cout << "Execution times = " << executeTimes[0] / 1000000.0 << "   " << executeTimes[1] / 1000000.0 << " ms" << endl;
    //    fout << "," << executeTimes[0] / 1000000.0 << "," << executeTimes[1] / 1000000.0;
    //    cout << "Validation times = " << validateTimes[0] / 1000000.0 << "   " << validateTimes[1] / 1000000.0 << " ms" << endl;
    //    fout << "," << validateTimes[0] / 1000000.0 << "," << validateTimes[1] / 1000000.0;
    //    cout << "Commit times = " << commitTimes[0] / 1000000.0 << "   " << commitTimes[1] / 1000000.0 << " ms" << endl;
    //    fout << "," << commitTimes[0] / 1000000.0 << "," << commitTimes[1] / 1000000.0;
    //    cout << "Compensate times = " << compensateTimes[0] / 1000000.0 << "   " << compensateTimes[1] / 1000000.0 << " ms" << endl;
    //    fout << "," << compensateTimes[0] / 1000000.0 << "," << compensateTimes[1] / 1000000.0;

    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;

    for (int i = 0; i < numThreads; ++i) {
        cout << "Thread " << i << endl;
        for (uint j = 0; j < txnTypes; ++j) {
            cout << "\t" << prgNames[j] << ":\n\t  finished=" << exec.finishedPerThread[j][i] << endl;
            cout << "\t  failedEx = " << exec.failedExPerThread[j][i] << endl;
            cout << " \t  failedVal = " << exec.failedValPerThread[j][i] << endl;
            header << ",T" << i << "-" << prgNames[j] << " finished" << ",T" << i << "-" << prgNames[j] << " failedEx"; // << ",T" << i << "-" << prgNames[j] << "failedVal";
            fout << "," << exec.finishedPerThread[j][i];
            fout << "," << exec.failedExPerThread[j][i];
            fout << "," << exec.failedValPerThread[j][i];
        }
        cout << endl;
        cout << "\t Max failed exec = " << exec.maxFailedExSingleProgram[i] << endl;
        cout << "\t Max failed val = " << exec.maxFailedValSingleProgram[i] << endl;
    }
    fout << endl;
    header << endl;
    fout.close();
    header.close();
    return 0;
}

#endif