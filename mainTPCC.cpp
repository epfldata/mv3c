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

int main(int argc, char** argv) {
#ifdef NB
    std::ofstream fout("out");
#else
    std::ofstream fout("out", ios::app);
#endif
    std::ofstream header("header");
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(23, &cpuset);
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

    MultiMapIndexMT<OrderKey, OrderVal, OrderPKey> OrderIndex;
    SecondaryIndex<OrderKey, OrderVal>* OrderIndexes[1];
    OrderIndexes[0] = &OrderIndex;
    ordTbl.secondaryIndexes = OrderIndexes;
    ordTbl.numIndexes = 1;

    MultiMapIndexMT<OrderLineKey, OrderLineVal, OrderLinePKey> OrderLineIndex;
    SecondaryIndex<OrderLineKey, OrderLineVal>* OrderLineIndexes[1];
    OrderLineIndexes[0] = &OrderLineIndex;
    ordLTbl.secondaryIndexes = OrderLineIndexes;
    ordLTbl.numIndexes = 1;

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
    fout << ", OMVCC, X";
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
        distNoTbl.insertVal(t0, it.first, DistrictNewOrderVal(2101));
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
    int delivery = 0;
    int orderstatus = 0;
    int stocklevel = 0;
    header << ", NumProgs, NumThreads";
    fout << ", " << numPrograms << ", " << numThreads;
    cout << "Number of programs = " << numPrograms << endl;
    cout << "Number of threads = " << numThreads << endl;

    Program ** programs = new Program*[numPrograms];
    CustomerTable = &custTbl;
    DistrictTable = &distTbl;
    WarehouseTable = &wareTbl;
    ItemTable = &itemTbl;
    StockTable = &stockTbl;
    HistoryTable = &historyTbl;
    DistrictNewOrderTable = &distNoTbl;
    OrderTable = &ordTbl;
    OrderLineTable = &ordLTbl;
    NewOrderTable = &newOrdTbl;



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
            case DELIVERY:
                newp = new MV3CDelivery(p);
                delivery++;
                break;
            case ORDERSTATUSBYID:
                newp = new MV3COrderStatus(p);
                orderstatus++;
                break;
            case STOCKLEVEL:
                newp = new MV3CStockLevel(p);
                stocklevel++;
                break;
            default:
                throw std::runtime_error("Unknown program");

        }

        programs[i] = newp;


    }
    cout << "NewOrder =" << neworder << endl;
    cout << "Payment =" << payment << endl;
    cout << "OrderStatus =" << orderstatus << endl;
    cout << "StockLevel =" << stocklevel << endl;
    cout << "Delivery =" << delivery << endl;
    ConcurrentExecutor exec(numThreads, transactionManager);
    exec.execute(programs, numPrograms);
    cout << "order success =" << orderSuccess << "  order failed =" << orderFailed << endl;
    header << ", Duration(ms), Committed, FailedEx, FailedVal, FailedExRate, FailedValRate, Throughput(ktps), ScaledTime, numValidations, numValAgainst, avgValAgainst, AvgValRound";
    cout << "Duration = " << exec.timeUs / 1000.0 << endl;
    fout << ", " << exec.timeUs / 1000.0;
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
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeUs) << " K tps" << endl;
    fout << ", " << (exec.finishedPrograms * 1000.0 / exec.timeUs);
    fout << ", " << numPrograms * exec.timeUs / (exec.finishedPrograms * 1000.0);


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
    cout << "TIMESTAMP VALUE = " << transactionManager.timestampGen << endl;
    for (uint i = 0; i < txnTypes; ++i) {
        header << ", " << prgNames[i] << " Ex time, " << prgNames[i] << " Val time, " << prgNames[i] << " Comm time, " << prgNames[i] << " Comp time";
        fout << ", " << executeTimes[i] / 1000000.0 << ", " << validateTimes[i] / 1000000.0 << ", " << commitTimes[i] / 1000000.0 << ", " << compensateTimes[i] / 1000000.0;
        cout << prgNames[i] << ":" << endl;
        cout << "\t Execution time = " << executeTimes[i] / 1000000.0 << endl;
        cout << "\t Validation time = " << validateTimes[i] / 1000000.0 << endl;
        cout << "\t Commit time = " << commitTimes[i] / 1000000.0 << endl;
        cout << "\t Compensate time = " << compensateTimes[i] / 1000000.0 << endl;

    }

    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;

    for (int i = 0; i < numThreads; ++i) {
        cout << "Thread " << i << endl;

        //        header << ", Thread " << i << ", finished NO, finished PY, failedEx NO, failedEx PY, failedVal NO, failedVal PY, maxFailedEx, maxFailedVal";
        //        fout << ", ";

        for (uint j = 0; j < txnTypes; ++j) {
            cout << "\t" << prgNames[j] << ":\n\t  finished=" << exec.finishedPerThread[j][i] << endl;
            cout << "\t  failedEx = " << exec.failedExPerThread[j][i] << endl;
            cout << " \t  failedVal = " << exec.failedValPerThread[j][i] << endl;
            header << ", T" << i << "-" << prgNames[j] << " finished" << ", T" << i << "-" << prgNames[j] << " failedEx" << ", T" << i << "-" << prgNames[j] << "failedVal";
            fout << ", " << exec.finishedPerThread[j][i];
            fout << ", " << exec.failedExPerThread[j][i];
            fout << ", " << exec.failedValPerThread[j][i];
        }
        cout << endl;
        header << ", T" << i << " maxFailedEx, T" << i << " maxFailedVal";
        cout << "\t Max failed exec = " << exec.maxFailedExSingleProgram[i] << endl;
        cout << "\t Max failed val = " << exec.maxFailedValSingleProgram[i] << endl;
        fout << ", " << exec.maxFailedExSingleProgram[i];
        fout << ", " << exec.maxFailedValSingleProgram[i];
    }
    fout << endl;
    header << endl;
    fout.close();
    header.close();
    return 0;
}
#endif
