#include "types.h"
#ifdef TPCC_TEST
#include "Table.h"
#include "Predicate.hpp"
#include "Transaction.h"
#include "Tuple.h"
#include "ConcurrentExecutor.h"
#include <cstdlib>
#include "NewMV3CTpcc.h"
#include <iomanip>
#include <locale>
using namespace std;
using namespace tpcc_ns;

DefineStore(Customer);
DefineStore(District);
DefineStore(Warehouse);
DefineStore(Item);
DefineStore(Stock);
DefineStore(History);
DefineStore(NewOrder);
DefineStore(Order);
DefineStore(OrderLine);
DefineStore(DistrictNewOrder);

TransactionManager transactionManager;
TransactionManager& Transaction::tm(transactionManager);
TABLE(Customer)* MV3CNewOrder::custTable;
TABLE(District)* MV3CNewOrder::distTable;
TABLE(Warehouse)* MV3CNewOrder::wareTable;
TABLE(Item)* MV3CNewOrder::itemTable;
TABLE(Stock)* MV3CNewOrder::stockTable;
TABLE(Order) * MV3CNewOrder::orderTable;
TABLE(OrderLine)* MV3CNewOrder::ordLTable;
TABLE(NewOrder) *MV3CNewOrder::newOrdTable;

TABLE(Customer)* MV3CPayment::custTable;
TABLE(District)* MV3CPayment::distTable;
TABLE(Warehouse)* MV3CPayment::wareTable;
TABLE(History)* MV3CPayment::histTable;

TABLE(DistrictNewOrder) * MV3CDelivery::distNewOrdTable;
TABLE(NewOrder) * MV3CDelivery::newOrdTable;
TABLE(Order) * MV3CDelivery::orderTable;
TABLE(Customer) * MV3CDelivery::custTable;

int main(int argc, char** argv) {
#ifdef NB
    std::ofstream fout("out");
#else
    std::ofstream fout("out", ios::app);
#endif
    std::ofstream header("header");
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2 * numThreads, &cpuset);
    auto s = sched_setaffinity(0, sizeof (cpu_set_t), &cpuset);
    if (s != 0) {
        throw std::runtime_error("Cannot set affinity");
    }
    TPCCDataGen tpcc;
    TABLE(Customer) custTbl(CustomerIndexSize);
    TABLE(District) distTbl(DistrictIndexSize);
    TABLE(Warehouse) wareTbl(WarehouseIndexSize);
    TABLE(NewOrder) newOrdTbl(NewOrderIndexSize);
    TABLE(Order) ordTbl(OrderIndexSize);
    TABLE(OrderLine) ordLTbl(OrderLineIndexSize);
    TABLE(Item) itemTbl(ItemIndexSize);
    TABLE(Stock) stockTbl(StockIndexSize);
    TABLE(History) historyTbl(HistoryIndexSize);
    TABLE(DistrictNewOrder) distNoTbl(DistrictIndexSize);
    tpcc.loadPrograms();
    tpcc.loadCust();
    tpcc.loadDist();
    tpcc.loadHist();
    tpcc.loadItem();
    tpcc.loadNewOrd();
    tpcc.loadOrdLine();
    tpcc.loadOrders();
    tpcc.loadStocks();
    tpcc.loadWare();
    std::cout.imbue(std::locale(""));
    header << "BenchName, Algo, Critical Compensate, Validation level, WW allowed, Store enabled, Cuckoo enabled, NumWare";
    cout << "TPCC" << endl;
    fout << "TPCC";
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
#if CUCKOO
    cout << "Cuckoo index" << endl;
    fout << ", Y";
#else
    cout << "UnorderedMap index" << endl;
    fout << ", N";
#endif
    cout << "Number of warehouse = " << numWare << endl;
    fout << ", " << numWare;
    Transaction t;
    Transaction *t0 = &t;
    transactionManager.begin(t0);
    for (const auto&it : tpcc.iCustomer) {
        custTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iDistrict) {
        distTbl.insertVal(t0, it.first, it.second);
        distNoTbl.insertVal(t0, it.first, DistrictNewOrderVal(3001));
    }
    for (const auto&it : tpcc.iHistory) {
        historyTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iItem) {
        itemTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iNewOrder) {
        newOrdTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iOrderLine) {
        ordLTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iOrder) {
        ordTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iStock) {
        stockTbl.insertVal(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iWarehouse) {
        wareTbl.insertVal(t0, it.first, it.second);
    }
    transactionManager.validateAndCommit(t0);
    int neworder = 0;
    int payment = 0;

    header << ", NumProgs, NumThreads";
    fout << ", " << numPrograms << ", " << numThreads;
    cout << "Number of programs = " << numPrograms << endl;
    cout << "Number of threads = " << numThreads << endl;

    Program ** programs = new Program*[numPrograms];
    MV3CNewOrder::custTable = &custTbl;
    MV3CNewOrder::distTable = &distTbl;
    MV3CNewOrder::wareTable = &wareTbl;
    MV3CNewOrder::itemTable = &itemTbl;
    MV3CNewOrder::stockTable = &stockTbl;
    MV3CNewOrder::orderTable = &ordTbl;
    MV3CNewOrder::ordLTable = &ordLTbl;
    MV3CNewOrder::newOrdTable = &newOrdTbl;

    MV3CPayment::custTable = &custTbl;
    MV3CPayment::distTable = &distTbl;
    MV3CPayment::wareTable = &wareTbl;
    MV3CPayment::histTable = &historyTbl;

    MV3CDelivery::distNewOrdTable = &distNoTbl;
    MV3CDelivery::newOrdTable = &newOrdTbl;
    MV3CDelivery::orderTable = &ordTbl;
    MV3CDelivery::custTable = &custTbl;

    for (uint i = 0; i < numPrograms; ++i) {
        Program *p = tpcc.programs[i];
        Program *newp;
        switch (p->prgType) {
            case NEWORDER:
                newp = new MV3CNewOrder(p);
                neworder++;
                break;
            case PAYMENTBYID:
                newp = new MV3CPayment(p);
                payment++;
                break;
            default:
                throw std::runtime_error("Unknown program");

        }

        programs[i] = newp;


    }
    cout << "NewOrder =" << neworder << endl;
    cout << "Payment =" << payment << endl;
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
    header << ", exec NO, exec PY, val NO, val PY, comt NO, comt PY, comp NO, comp TNC ";
    cout << "Execution times = " << executeTimes[0] / 1000000.0 << "   " << executeTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << executeTimes[0] / 1000000.0 << ", " << executeTimes[1] / 1000000.0;
    cout << "Validation times = " << validateTimes[0] / 1000000.0 << "   " << validateTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << validateTimes[0] / 1000000.0 << ", " << validateTimes[1] / 1000000.0;
    cout << "Commit times = " << commitTimes[0] / 1000000.0 << "   " << commitTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << commitTimes[0] / 1000000.0 << ", " << commitTimes[1] / 1000000.0;
    cout << "Compensate times = " << compensateTimes[0] / 1000000.0 << "   " << compensateTimes[1] / 1000000.0 << " ms" << endl;
    fout << ", " << compensateTimes[0] / 1000000.0 << ", " << compensateTimes[1] / 1000000.0;
    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;

    for (int i = 0; i < numThreads; ++i) {
        cout << "Thread " << i << endl;
        header << ", Thread " << i << ", finished NO, finished PY, failedEx NO, failedEx PY, failedVal NO, failedVal PY, maxFailedEx, maxFailedVal";
        fout << ", ";
        cout << "\t Finished     NO:" << exec.finishedPerThread[0][i] << "  PY:" << exec.finishedPerThread[1][i] << endl;
        fout << ", " << exec.finishedPerThread[0][i] << ", " << exec.finishedPerThread[1][i];
        cout << "\t FailedEx     NO:" << exec.failedExPerThread[0][i] << "  PY:" << exec.failedExPerThread[1][i] << endl;
        fout << ", " << exec.failedExPerThread[0][i] << ", " << exec.failedExPerThread[1][i];
        cout << "\t FailedVal    NO:" << exec.failedValPerThread[0][i] << "  PY:" << exec.failedValPerThread[1][i] << endl;
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
