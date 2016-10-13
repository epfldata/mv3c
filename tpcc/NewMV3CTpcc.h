#ifndef NEWMV3CTPCC_H
#define NEWMV3CTPCC_H
#include "TPCC.h"
#include "Predicate.hpp"

namespace tpcc_ns {
#define  TABLE(x) Table<x##Key, x##Val>
#define GETP(x) GetPred<x##Key,x##Val>
#define DV(x) DeltaVersion<x##Key,x##Val>

#define DefineStore(type)\
  template<>\
  DeltaVersion<type##Key, type##Val>::StoreType DeltaVersion<type##Key, type##Val>::store(type##DVSize, STRINGIFY(type)"DV", numThreads);\
  template<>\
  Entry<type##Key, type##Val>::StoreType Entry<type##Key, type##Val>::store(type##EntrySize, STRINGIFY(type)"Entry", numThreads)


    typedef GETP(Item) ItemGet;
    typedef DV(Item) ItemDV;
    typedef GETP(Stock) StockGet;
    typedef DV(Stock) StockDV;
    typedef GETP(District) DistGet;
    typedef DV(District) DistDV;
    typedef GETP(Warehouse) WareGet;
    typedef DV(Warehouse) WareDV;
    typedef GETP(Customer) CustGet;
    typedef DV(Customer) CustDV;
    typedef GETP(Order) OrderGet;
    typedef DV(Order) OrderDV;
    typedef GETP(DistrictNewOrder) DistNOGet;
    typedef DV(DistrictNewOrder) DistNODV;
}
using namespace tpcc_ns;

struct ThreadLocal {
    DistGet dist;
    CustGet cust;
    WareGet ware;
    String<24>* MV3CNewOrderDist_info[15];
    StockGet MV3CNewOrderStock[15];
    ItemGet MV3CNewOrderItem[15];
    float MV3CNewOrderOl_amt[15];
    float MV3CNewOrderTotal;
    float MV3CNewOrderD_tax;


    //        DistGet dist;
    //        CustGet cust;
    //        WareGet ware;
    HistoryKey histKey;
    WarehouseKey wareKey;
    DistrictKey distKey;
    CustomerKey custKey;
    ItemKey itemKey;
    StockKey stockKey;
    OrderKey orderKey;
    OrderLineKey orderLineKey;
    NewOrderKey newOrderKey;
};
namespace tpcc_ns {
    forceinline TransactionReturnStatus neworder_itemfn(Program *p, const ItemDV * idv, uint cs);
    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV * idv, uint cs);
    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV * idv, uint cs = -1);

    struct MV3CNewOrder : NewOrder {
        //Tables are not defined static because we can reuse program for verification by just changing tables
        //EDIT:  Can use static table pointers instead of refs
        static TABLE(Customer) * custTable;
        static TABLE(District) * distTable;
        static TABLE(Warehouse) * wareTable;
        static TABLE(Item) * itemTable;
        static TABLE(Stock) * stockTable;
        static TABLE(Order) * orderTable;
        static TABLE(OrderLine) * ordLTable;
        static TABLE(NewOrder) * newOrdTable;
        //NOT INPUT PARAMATERS  

        MV3CNewOrder(const Program* p) :
        NewOrder(p) {
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status;
            for (uint8_t ol_number = 0; ol_number < o_ol_cnt; ol_number++) {
                uint32_t ol_i_id = itemid[ol_number];

                new (&threadVar->itemKey) ItemKey(ol_i_id);
                new (&threadVar->MV3CNewOrderItem[ol_number]) ItemGet(itemTable, &xact, threadVar->itemKey, nullptr, col_type(1 << 3));

                auto idv = threadVar->MV3CNewOrderItem[ol_number].evaluateAndExecute(&xact, neworder_itemfn, ol_number);
                status = neworder_itemfn(this, idv, ol_number);
                if (status != SUCCESS) {
                    //                    cerr << "NewOrder Item" << endl;
                    return status;
                }
            }

            new (&threadVar->distKey) DistrictKey(d_id, w_id);
            new (&threadVar->dist) DistGet(distTable, &xact, threadVar->distKey, nullptr, col_type(1 << 7 || 1 << 9));
            auto ddv = threadVar->dist.evaluateAndExecute(&xact, neworder_distfn);
            status = neworder_distfn(this, ddv);
            if (status != SUCCESS) {
                //                throw std::logic_error("NewOrder dist");
                return status;
            }

            new (&threadVar->wareKey) WarehouseKey(w_id);
            new (&threadVar->ware) WareGet(wareTable, &xact, threadVar->wareKey, nullptr, col_type(1 << 7));
            auto wdv = threadVar->ware.evaluateAndExecute(&xact, nullptr); //TOFIX : No function associated
            auto w_tax = wdv->val._7;


            new (&threadVar->custKey) CustomerKey(c_id, d_id, w_id);
            new (&threadVar->cust) CustGet(custTable, &xact, threadVar->custKey, nullptr, col_type(1 << 13));
            auto cdv = threadVar->cust.evaluateAndExecute(&xact, nullptr); //TOFIX : No function associated
            auto c_disc = cdv->val._13;
            threadVar->MV3CNewOrderTotal = 0;
            for (uint8_t i = 0; i < o_ol_cnt; ++i) {
                threadVar->MV3CNewOrderTotal += threadVar->MV3CNewOrderOl_amt[i];
            }
            threadVar->MV3CNewOrderTotal *= (1 + w_tax + threadVar->MV3CNewOrderD_tax)*(1 - c_disc);
            return SUCCESS;
        }
    };

    forceinline TransactionReturnStatus neworder_itemfn(Program *p, const ItemDV *idv, uint ol_number) {
        auto prg = (MV3CNewOrder *) p;
        Transaction& xact = p->xact;
        auto threadVar = prg->threadVar;
        if (idv == nullptr) {
            cerr << "NewOrder Item missing" << endl;
            return ABORT;
        }
        threadVar->MV3CNewOrderOl_amt[ol_number] = idv->val._3 * prg->quantity[ol_number];
        auto ol_i_id = prg->itemid[ol_number];
        auto ol_supply_w_id = prg->supware[ol_number];

        new (&threadVar->stockKey) StockKey(ol_i_id, ol_supply_w_id); //reusing multiple times within same transaction
        new (&threadVar->MV3CNewOrderStock[ol_number]) StockGet(MV3CNewOrder::stockTable, &xact, threadVar->stockKey, &threadVar->MV3CNewOrderItem[ol_number], col_type((1 << 15) - 1));
        auto sdv = threadVar->MV3CNewOrderStock[ol_number].evaluateAndExecute(&xact, neworder_stockfn, ol_number);
        return neworder_stockfn(p, sdv, ol_number);
    }

    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV *sdv, uint ol_number) {
        auto prg = (MV3CNewOrder*) p;
        Transaction& xact = p->xact;
        auto threadVar = prg->threadVar;
        auto ol_qty = prg->quantity[ol_number];

        CreateValUpdate(Stock, newsv, sdv);

        if (newsv->_1 >= ol_qty + 10) {
            newsv->_1 -= ol_qty;
        } else {
            newsv->_1 += (91 - ol_qty);
        }
        newsv->_12 += prg->quantity[ol_number];
        newsv->_13++;
        if (prg->w_id != prg->supware[ol_number]) {
            newsv->_14++;
        }

        threadVar->MV3CNewOrderDist_info[ol_number] = &newsv->_2 + (prg->d_id - 1);

        if (MV3CNewOrder::stockTable->update(&xact, sdv->entry, MakeRecord(newsv), &threadVar->MV3CNewOrderStock[ol_number], ALLOW_WW, col_type(1 << 1 | 1 << 12 | 1 << 13 | 1 << 14)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("NewOrder stock");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV *ddv, uint cs) {
        auto prg = (MV3CNewOrder *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        CreateValUpdate(District, newdv, ddv);
        threadVar->MV3CNewOrderD_tax = newdv->_7;
        auto o_id = newdv->_9;
        newdv->_9++;

        if (MV3CNewOrder::distTable->update(&xact, ddv->entry, MakeRecord(newdv), &threadVar->dist, ALLOW_WW, col_type(1 << 9)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("NewOrder stock");
            //            else
            return WW_ABORT;
        }

        new(&threadVar->orderKey) OrderKey(o_id, prg->d_id, prg->w_id);
        CreateValInsert(Order, newov, prg->c_id, prg->datetime, 0, prg->o_ol_cnt, prg->o_all_local);
        if (MV3CNewOrder::orderTable->insert(&xact, threadVar->orderKey, MakeRecord(newov), &threadVar->dist) != OP_SUCCESS) {
            //            throw std::logic_error("NewOrder order");
            return WW_ABORT;
        }

        new(&threadVar->newOrderKey) NewOrderKey(o_id, prg->d_id, prg->w_id);
        CreateValInsert(NewOrder, newnov, true);
        if (MV3CNewOrder::newOrdTable->insert(&xact, threadVar->newOrderKey, MakeRecord(newnov), &threadVar->dist) != OP_SUCCESS) {
            //            throw std::logic_error("NewOrder neworder");
            return WW_ABORT;
        }

        for (uint8_t ol_number = 0; ol_number < prg->o_ol_cnt; ol_number++) {

            new (&threadVar->orderLineKey) OrderLineKey(o_id, prg->d_id, prg->w_id, ol_number + 1);
            CreateValInsert(OrderLine, newolv, prg->itemid[ol_number], prg->supware[ol_number], nulldate, prg->quantity[ol_number], threadVar->MV3CNewOrderOl_amt[ol_number], *threadVar->MV3CNewOrderDist_info[ol_number]);

            if (MV3CNewOrder::ordLTable->insert(&xact, threadVar->orderLineKey, MakeRecord(newolv), &threadVar->dist) != OP_SUCCESS) {
                //throw std::logic_error("NewORder orderline");
                return WW_ABORT;
            }
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus payment_distfn(Program *p, const DistDV * ddv, uint cs = -1);
    forceinline TransactionReturnStatus payment_warefn(Program *p, const WareDV * wdv, uint cs = -1);
    forceinline TransactionReturnStatus payment_custfn(Program *p, const CustDV * cdv, uint cs = -1);

    struct MV3CPayment : PaymentById {
        static TABLE(Customer) * custTable;
        static TABLE(District) * distTable;
        static TABLE(Warehouse) * wareTable;
        static TABLE(History) * histTable;

        MV3CPayment(const Program* p) :
        PaymentById(p) {
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status = SUCCESS;
            
            new (&threadVar->distKey) DistrictKey(d_id, w_id);
            new (&threadVar->dist) DistGet(distTable, &xact, threadVar->distKey, nullptr, col_type(1 << 8));
            auto ddv = threadVar->dist.evaluateAndExecute(&xact, payment_distfn);
            status = payment_distfn(this, ddv);
            if (status != SUCCESS) {
                return status;
            }

            new(&threadVar->wareKey) WarehouseKey(w_id);

            new (&threadVar->ware) WareGet(wareTable, &xact, threadVar->wareKey, nullptr, col_type(1 << 8));
            auto wdv = threadVar->ware.evaluateAndExecute(&xact, payment_warefn);
            status = payment_warefn(this, wdv);
            if (status != SUCCESS) {
                return status;
            }
            //

            new(&threadVar->custKey)CustomerKey(c_id, c_d_id, c_w_id);
            new (&threadVar->cust) CustGet(custTable, &xact, threadVar->custKey, nullptr, (1 << 14 | 1 << 15 | 1 << 16));
            auto cdv = threadVar->cust.evaluateAndExecute(&xact, payment_custfn);
            status = payment_custfn(this, cdv);
            if (status != SUCCESS) {
                return status;
            }

            new (&threadVar->histKey) HistoryKey(c_id, c_d_id, c_w_id, d_id, w_id, datetime, h_amount, String<24>());
            CreateValInsert(History, newhv, true);
            if (histTable->insert(&xact, threadVar->histKey, MakeRecord(newhv)) != OP_SUCCESS) {
                //                throw std::logic_error("payment hist");
                return WW_ABORT;
            }
            return SUCCESS;

        }

    };

    forceinline TransactionReturnStatus payment_distfn(Program* p, const DistDV* ddv, uint cs) {
        auto prg = (MV3CPayment *) p;
        Transaction& xact = p->xact;
        CreateValUpdate(District, newdv, ddv);
        newdv->_8 += prg->h_amount;
        if (MV3CPayment::distTable->update(&xact, ddv->entry, MakeRecord(newdv), &prg->threadVar->dist, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment dist");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;

    }

    forceinline TransactionReturnStatus payment_custfn(Program* p, const CustDV* cdv, uint cs) {
        auto prg = (MV3CPayment *) p;
        Transaction& xact = p->xact;
        CreateValUpdate(Customer, newcv, cdv);
        newcv->_14 -= prg->h_amount;
        newcv->_15 += prg->h_amount;
        newcv->_16++;
        if (MV3CPayment::custTable->update(&xact, cdv->entry, MakeRecord(newcv), &prg->threadVar->cust, ALLOW_WW, col_type(1 << 14 | 1 << 15 | 1 << 16)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment cust");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus payment_warefn(Program* p, const WareDV* wdv, uint cs) {
        auto prg = (MV3CPayment *) p;
        Transaction& xact = p->xact;
        CreateValUpdate(Warehouse, newwv, wdv);
        newwv->_8 += prg->h_amount;
        if (MV3CPayment::wareTable->update(&xact, wdv->entry, MakeRecord(newwv), &prg->threadVar->ware, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment ware");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }

    //    forceinline TransactionReturnStatus delivery_distneworderfn(Program *p, const DistNODV* dnodv, ClosureState* cs);
    //    forceinline TransactionReturnStatus delivery_orderfn(Program *p, const OrderDV* odv, ClosureState* cs);
    //    forceinline TransactionReturnStatus delivery_custfn(Program *p, const CustDV* cdv, ClosureState* cs);
    //
    //
    //    struct MV3CDelivery : Delivery {
    //        //        static TABLE(OrderLine) &ordLTable;
    //        static TABLE(DistrictNewOrder) & distNewOrdTable;
    //        static TABLE(NewOrder) & newOrdTable;
    //        static TABLE(Order) &orderTable;
    //        static TABLE(Customer) &custTable;
    //
    //        MV3CDelivery(Program *p, Table<DistrictNewOrderKey, DistrictNewOrderVal>& distNewOrdTable, Table<NewOrderKey, NewOrderVal>& newOrdTable, Table<OrderKey, OrderVal>& orderTable, Table<CustomerKey, CustomerVal>& custTable) :
    //        Delivery(p), distNewOrdTable(distNewOrdTable), newOrdTable(newOrdTable), orderTable(orderTable), custTable(custTable) {
    //        }
    //
    //        TransactionReturnStatus execute() override {
    //            for (uint8_t d_id = 1; d_id <= 10; ++d_id) {
    //                auto distno = new (DistNOGet::store.add()) DistNOGet(&distNewOrdTable, &xact, DistrictNewOrderKey(d_id, w_id), nullptr, col_type(1 << 1));
    //                auto dnocs = new (DeliveryDistrictNewOrderCS::store.add()) DeliveryDistrictNewOrderCS(distno);
    //                auto dnodv = distno->evaluateAndExecute(&xact, delivery_distneworderfn, dnocs);
    //                auto status = delivery_distneworderfn(this, dnodv, dnocs);
    //                if (status != SUCCESS) {
    //                    return status;
    //                }
    //            }
    //        }
    //
    //    };
    //
    //    forceinline TransactionReturnStatus delivery_distneworderfn(Program* p, const DistNODV* dnodv, ClosureState* cs) {
    //        auto prg = (MV3CDelivery *) p;
    //        auto dnocs = (DeliveryDistrictNewOrderCS *) cs;
    //        auto d_id = dnocs->distno->key._1;
    //        DistrictNewOrderVal newdnov = dnodv->val;
    //        auto o_id = newdnov._1++;
    //        if (prg->distNewOrdTable.update(&xact, dnodv->entry, newdnov, dnocs->distno, ALLOW_WW, col_type(1 << 1)) != OP_SUCCESS) {
    //            //            if (ALLOW_WW)
    //            //                throw std::logic_error("delivery dist-neworder");
    //            //            else
    //            //                return WW_ABORT;
    //        }
    //        //        prg->newOrdTable.remove(.............)
    //        auto order = new (OrderGet::store.add()) OrderGet(&prg->orderTable, &xact, OrderKey(o_id, d_id, prg->w_id), dnocs->distno, col_type(1 << 1 | 1 << 3));
    //        auto ocs = new (DeliveryOrderCS::store.add()) DeliveryOrderCS(order, d_id);
    //        auto odv = order->evaluateAndExecute(&xact, delivery_orderfn, ocs);
    //        return delivery_orderfn(p, odv, ocs);
    //
    //    }
    //
    //    forceinline TransactionReturnStatus delivery_orderfn(Program* p, const OrderDV* odv, ClosureState* cs) {
    //        auto prg = (MV3CDelivery *) p;
    //        auto ocs = (DeliveryOrderCS *) cs;
    //        OrderVal newov = odv->val;
    //        auto c_id = newov._1;
    //        newov._3 = prg->o_carrier_id;
    //        if (prg->orderTable.update(&xact, odv->entry, newov, ocs->order, ALLOW_WW, col_type(1 << 3)) != OP_SUCCESS) {
    //            //            if (ALLOW_WW)
    //            //                throw std::logic_error("delivery order");
    //            //            else
    //            //                return WW_ABORT;
    //        }
    //        auto cust = new (CustGet::store.add()) CustGet(&prg->custTable, &xact, CustomerKey(c_id, ocs->d_id, prg->w_id), ocs->order, col_type(1 << 14 | 1 << 17));
    //        auto ccs = new (DeliveryCustCS::store.add()) DeliveryCustCS(cust);
    //        auto cdv = cust->evaluateAndExecute(&xact, delivery_custfn, ccs);
    //        return delivery_custfn(p, cdv, ccs);
    //    }
    //
    //    forceinline TransactionReturnStatus delivery_custfn(Program* p, const CustDV* cdv, ClosureState* cs) {
    //        auto prg = (MV3CDelivery *) p;
    //        auto ccs = (DeliveryCustCS *) cs;
    //        CustomerVal newcv = cdv->val;
    //        float ol_total = 1; //SHOULD BE OBTAINED from OrderlIne instead;
    //        newcv._14 += ol_total;
    //        newcv._17++;
    //        if (prg->custTable.update(&xact, cdv->entry, newcv, ccs->cust, ALLOW_WW, col_type(1 << 14 | 1 << 17))) {
    //            //            if (ALLOW_WW)
    //            //                throw std::logic_error("delivery cust");
    //            //            else
    //            //                return WW_ABORT;
    //        }
    //        return SUCCESS;
    //    }
    //
    //    struct MV3COrderStatus : OrderStatusById {
    //        static TABLE(Order) &orderTable;
    //
    //        TransactionReturnStatus execute() override {
    //            auto osdv = orderTable.getReadOnly(&xact, OrderKey(-1, d_id, w_id)); // && cid = cid
    //            //FIND MAX with given cid
    //            auto o_id = osdv->val._1;
    //
    //        }
    //
    //    };


}


#endif /* NEWMV3CTPCC_H */

