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



template<>
TABLE(Customer)::dvStoreType TABLE(Customer)::dvStore(CustSize * 3, "CustomerDV");
template<>
TABLE(Customer)::entryStoreType TABLE(Customer)::entryStore(CustSize, "CustomerEntry");
template<>
TABLE(District)::dvStoreType TABLE(District)::dvStore(DistSize * 3, "DistrictDV");
template<>
TABLE(District)::entryStoreType TABLE(District)::entryStore(DistSize, "DistrictEntry");
template<>
TABLE(Warehouse)::dvStoreType TABLE(Warehouse)::dvStore(WareSize * 3, "WarehouseDV");
template<>
TABLE(Warehouse)::entryStoreType TABLE(Warehouse)::entryStore(WareSize, "WarehouseEntry");
template<>
TABLE(NewOrder)::dvStoreType TABLE(NewOrder)::dvStore(NewOrderSize * 3, "NewOrderDV");
template<>
TABLE(NewOrder)::entryStoreType TABLE(NewOrder)::entryStore(NewOrderSize, "NewOrderEntry");
template<>
TABLE(Order)::dvStoreType TABLE(Order)::dvStore(OrderSize * 3, "OrderDV");
template<>
TABLE(Order)::entryStoreType TABLE(Order)::entryStore(OrderSize, "OrderEntry");
template<>
TABLE(OrderLine)::dvStoreType TABLE(OrderLine)::dvStore(OrdLineSize * 3, "OrderLineDV");
template<>
TABLE(OrderLine)::entryStoreType TABLE(OrderLine)::entryStore(OrdLineSize, "OrderLineEntry");
template<>
TABLE(Stock)::dvStoreType TABLE(Stock)::dvStore(StockSize * 3, "StockDV");
template<>
TABLE(Stock)::entryStoreType TABLE(Stock)::entryStore(StockSize, "StockEntry");
template<>
TABLE(Item)::dvStoreType TABLE(Item)::dvStore(ItemSize * 3, "ItemDV");
template<>
TABLE(Item)::entryStoreType TABLE(Item)::entryStore(ItemSize, "ItemEntry");
template<>
TABLE(History)::dvStoreType TABLE(History)::dvStore(HistSize * 3, "HistDV");
template<>
TABLE(History)::entryStoreType TABLE(History)::entryStore(HistSize, "HistEntry");
template<>
TABLE(DistrictNewOrder)::dvStoreType TABLE(DistrictNewOrder)::dvStore(DistSize * 3, "DistNODV");
template<>
TABLE(DistrictNewOrder)::entryStoreType TABLE(DistrictNewOrder)::entryStore(DistSize, "DistNOEntry");
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
    TABLE(Customer) custTbl;
    TABLE(District) distTbl;
    TABLE(Warehouse) wareTbl;
    TABLE(NewOrder) newOrdTbl;
    TABLE(Order) ordTbl;
    TABLE(OrderLine) ordLTbl;
    TABLE(Item) itemTbl;
    TABLE(Stock) stockTbl;
    TABLE(History) historyTbl;
    TABLE(DistrictNewOrder) distNoTbl;
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
    int neworder = 0;
    int payment = 0;
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
    ConcurrentExecutor exec(10, transactionManager);
    exec.execute(programs, numPrograms);
    cout << "Duration = " << exec.timeMs << endl;
    std::cout.imbue(std::locale(""));
    cout << "Throughput = " << (uint)(numPrograms * 1000.0 / exec.timeMs) << " K tps" << endl;
    return 0;
}

