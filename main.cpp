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

int main(int argc, char** argv) {
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
    transactionManager.commit(t0);
    int neworder = 0;
    int payment = 0;
    cout << "Number of programs = "<<numPrograms << endl;
    cout << "Number of warehouse = "<<numWare << endl;
    cout << "Number of threads = "<<numThreads << endl;
    
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
    cout << "Duration = " << exec.timeMs << endl;
    cout << "Committed = " << exec.finishedPrograms << endl;
    cout << "Aborted  = " << exec.abortedPrograms << endl;
    std::cout.imbue(std::locale(""));
    cout << "Abort rate = " << 1.0 * exec.abortedPrograms / exec.finishedPrograms << endl;
    cout << "Throughput = " << (uint) (exec.finishedPrograms * 1000.0 / exec.timeMs) << " K tps" << endl;
    for (uint i = 0; i < numPrograms; ++i) {
        delete programs[i];
    }
    delete[] programs;
    return 0;
}

