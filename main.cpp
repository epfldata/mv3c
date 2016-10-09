#include "Table.h"
#include "Predicate.hpp"
#include "Transaction.h"
#include "Tuple.h"
#include "ConcurrentExecutor.h"
#include <cstdlib>
#include "NewMV3CTpcc.h"
#include <stdexcept>
using namespace std;
using namespace tpcc_ns;
template<>
CustGet::StoreType CustGet::store(100, "CustomerPredicateStore");
template<>
DistGet::StoreType DistGet::store(100, "DistrictPredicateStore");
template<>
WareGet::StoreType WareGet::store(100, "WarehousePredicateStore");
template<>
OrderGet::StoreType OrderGet::store(100, "OrderPredicateStore");
template<>
ItemGet::StoreType ItemGet::store(1000, "ItemPredicateStore");
template<>
StockGet::StoreType StockGet::store(1000, "StockPredicateStore");
template<>
DistNOGet::StoreType DistNOGet::store(100, "Dist-NO PredicateStore");

NewOrderCS::StoreType NewOrderCS::store(1000, "NewOrderCS");
DeliveryDistrictNewOrderCS::StoreType DeliveryDistrictNewOrderCS::store(100, "DeliveryDistNO CS");
DeliveryOrderCS::StoreType DeliveryOrderCS::store(100, "DeliveryOrderCS");
DeliveryCustCS::StoreType DeliveryCustCS::store(100, "DeliveryCustCS");

int main(int argc, char** argv) {
    TPCCDataGen tpcc;
    TABLE(Customer) custTbl(CustSize, 3, "CustomerTable");
    TABLE(District) distTbl(DistSize, 3, "DistrictTable");
    TABLE(Warehouse) wareTbl(WareSize, 3, "WareTable");
    TABLE(NewOrder) newOrdTbl(NewOrderSize, 2, "NewOrderTable");
    TABLE(Order) ordTbl(OrderSize, 2, "OrderTable");
    TABLE(OrderLine) ordLTbl(OrdLineSize, 2, "OrderLineTable");
    TABLE(Item) itemTbl(ItemSize, 1, "ItemTable");
    TABLE(Stock) stockTbl(StockSize, 3, "StockTable");
    TABLE(History) historyTbl(HistSize, 1, "HistoryTable");
    TABLE(DistrictNewOrder) DistNoTbl(DistSize, 2, "DistrictNO Table");
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

    TransactionManager tm;
    Transaction *t0 = tm.begin();
    for (const auto&it : tpcc.iCustomer) {
        custTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iDistrict) {
        distTbl.insert(t0, it.first, it.second);
        DistNoTbl.insert(t0, it.first, DistrictNewOrderVal(3001));
    }
    for (const auto&it : tpcc.iHistory) {
        historyTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iItem) {
        itemTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iNewOrder) {
        newOrdTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iOrderLine) {
        ordLTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iOrder) {
        ordTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iStock) {
        stockTbl.insert(t0, it.first, it.second);
    }
    for (const auto&it : tpcc.iWarehouse) {
        wareTbl.insert(t0, it.first, it.second);
    }
    int neworder = 0;
    int payment = 0;
    Program ** programs = new Program*[numPrograms];

    for (uint i = 0; i < numPrograms; ++i) {
        Program *p = tpcc.programs[i];
        Program *newp;
        switch (p->prgType) {
            case NEWORDER:
                newp = new MV3CNewOrder(p, custTbl, distTbl, wareTbl, itemTbl, stockTbl, ordTbl, ordLTbl, newOrdTbl);
                neworder++;
                break;
            case PAYMENTBYID:
                newp = new MV3CPayment(p, custTbl, distTbl, wareTbl, historyTbl);
                payment++;
                break;
            default:
                throw std::runtime_error("Unknown program");

        }
        newp->xact = t0;
        programs[i] = newp;


    }
    cout << "NewOrder =" << neworder << endl;
    cout << "Payment =" << payment << endl;
    ConcurrentExecutor exec(10, tm);
    exec.execute(programs, numPrograms);
    cout << "Duration = " << exec.timeMs << endl;
    cout << "Throughput = " << numPrograms*1000.0 / exec.timeMs << " K tps" << endl;
    return 0;
}

