#ifdef TRADING_TEST
#include "Table.h"
#include "Predicate.hpp"
#include "Transaction.h"
#include "Tuple.h"
#include "ConcurrentExecutor.h"
#include <cstdlib>
#include "mv3cTrading.h"
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

TABLE(Customer)* mv3cTradeOrder::CustomerTable;
TABLE(Security)* mv3cTradeOrder::SecurityTable;
TABLE(Trade)* mv3cTradeOrder::TradeTable;
TABLE(TradeLine)* mv3cTradeOrder::TradeLineTable;

TABLE(Security)* mv3cPriceUpdate::SecurityTable;

int main(int argc, char** argv) {
    TradingDataGen trade;

    TABLE(Customer) custTable(CustomerSize);
    TABLE(Security) secTable(SecuritySize);
    TABLE(Trade) tradeTable(TradeSize);
    TABLE(TradeLine) tradeLineTable(TradeLineSize);
    trade.loadCustomer();
    trade.loadPrograms();
    trade.loadSecurity();

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
    cout << "Power factor = " <<POWER << endl;
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
    mv3cPriceUpdate::SecurityTable = &secTable;
    mv3cTradeOrder::CustomerTable = &custTable;
    mv3cTradeOrder::SecurityTable = &secTable;
    mv3cTradeOrder::TradeLineTable = &tradeLineTable;
    mv3cTradeOrder::TradeTable = &tradeTable;

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
    cout << "Duration = " << exec.timeMs << endl;
    cout << "Committed = " << exec.finishedPrograms << endl;
    cout << "FailedExecution  = " << exec.failedExecution << endl;
    cout << "FailedValidation  = " << exec.failedValidation << endl;
    std::cout.imbue(std::locale(""));
    cout << "FailedEx rate = " << 1.0 * exec.failedExecution / exec.finishedPrograms << endl;
    cout << "FailedVal rate = " << 1.0 * exec.failedValidation / exec.finishedPrograms << endl;
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeMs) << " K tps" << endl;
    cout << "Num validations  = " << transactionManager.numValidations << endl;
    cout << "Num validations against = " << transactionManager.numXactsValidatedAgainst << endl;
    cout << "avg validations against = " << transactionManager.numXactsValidatedAgainst / (1.0 * transactionManager.numValidations) << endl;
    cout << "avg validation rounds = " << transactionManager.numRounds / (1.0 * transactionManager.numValidations) << endl;
    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;

    for (int i = 0; i < numThreads; ++i) {
        cout << "Thread " << i << endl;
        cout << "\t Finished     TO:" << exec.finishedPerThread[0][i] << "  PU:" << exec.finishedPerThread[1][i] << endl;
        cout << "\t FailedEx     TO:" << exec.failedExPerThread[0][i] << "  PU:" << exec.failedExPerThread[1][i] << endl;
        cout << "\t FailedVal    TO:" << exec.failedValPerThread[0][i] << "  PU:" << exec.failedValPerThread[1][i] << endl;
        cout << "\t Max failed exec = " << exec.maxFailedExSingleProgram[i] << endl;
        cout << "\t Max failed val = " << exec.maxFailedValSingleProgram[i] << endl;
    }


    return 0;
}

#endif