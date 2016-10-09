#ifndef NEWMV3CTPCC_H
#define NEWMV3CTPCC_H
#include "TPCC.h"
#include "Predicate.hpp"

namespace tpcc_ns {
#define TABLE(x) Table<x##Key, x##Val>
#define GETP(x) GetPred<x##Key,x##Val>
#define DV(x) DeltaVersion<x##Key,x##Val>
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

    struct NewOrderCS : ClosureState {
        typedef ConcurrentStore<NewOrderCS> StoreType;
        static StoreType  store;
        const uint8_t ol_number;

        NewOrderCS(uint8_t n) : ol_number(n) {
        }

        void free() override {
            store.remove(this);
        }

    };

  
    forceinline TransactionReturnStatus neworder_itemfn(Program *p, const ItemDV * idv, ClosureState* cs);
    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV * idv, ClosureState* cs);
    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV * idv, ClosureState* cs);

    struct MV3CNewOrder : NewOrder {
        TABLE(Customer) &custTable;
        TABLE(District) &distTable;
        TABLE(Warehouse) &wareTable;
        TABLE(Item) &itemTable;
        TABLE(Stock) &stockTable;
        TABLE(Order) &orderTable;
        TABLE(OrderLine) &ordLTable;
        TABLE(NewOrder) &newOrdTable;

        float total;
        float d_tax;


        DistGet* dist;
        ItemGet* item[15];
        StockGet* stock[15];
        CustGet *cust;
        WareGet *ware;
        
        MV3CNewOrder(Program *p, TABLE(Customer) & c, TABLE(District) & d, TABLE(Warehouse) & w, TABLE(Item) & i, TABLE(Stock) & s, TABLE(Order) & o, TABLE(OrderLine) & ol, TABLE(NewOrder) & no) : NewOrder(p), custTable(c), distTable(d), wareTable(w), itemTable(i), stockTable(s), orderTable(o), ordLTable(ol), newOrdTable(no) {
        }
        void cleanUp() override{
            dist->free();
            cust->free();
            ware->free();
            for(int i = 0; i< o_ol_cnt; ++i){
                item[i]->free();
                stock[i]->free();
            }
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status;
            for (uint8_t ol_number = 0; ol_number < o_ol_cnt; ol_number++) {
                uint32_t ol_i_id = itemid[ol_number];
                item[ol_number] = new (ItemGet::store.add()) ItemGet(&itemTable, xact, ItemKey(ol_i_id), nullptr, col_type(1 << 3));
                auto itemcs = new (NewOrderCS::store.add()) NewOrderCS(ol_number);
                auto idv = item[ol_number]->evaluateAndExecute(xact, neworder_itemfn, itemcs);
                status = neworder_itemfn(this, idv, itemcs);
                if (status != SUCCESS) {
                    throw std::logic_error("NewOrder Item");
                    return status;
                }
            }

            dist = new (DistGet::store.add()) DistGet(&distTable, xact, DistrictKey(d_id, w_id), nullptr, col_type(1 << 7 || 1 << 9));
            auto ddv = dist->evaluateAndExecute(xact, neworder_distfn, nullptr);
            status = neworder_distfn(this, ddv, nullptr);
            if (status != SUCCESS) {
                throw std::logic_error("NewOrder dist");
                return status;
            }

            ware = new (WareGet::store.add()) WareGet(&wareTable, xact, WarehouseKey(w_id), nullptr, col_type(1 << 7));
            auto wdv = ware->evaluateAndExecute(xact, nullptr);
            auto w_tax = wdv->val._7;

            cust = new (CustGet::store.add()) CustGet(&custTable, xact, CustomerKey(c_id, d_id, w_id), nullptr, col_type(1 << 13));
            auto cdv = cust->evaluateAndExecute(xact, nullptr);
            auto c_disc = cdv->val._13;

            for (uint8_t i = 0; i < o_ol_cnt; ++i) {
                total += ol_amt[i];
            }
            total = total * (1 + w_tax + d_tax)*(1 - c_disc);
        }
    };

    forceinline TransactionReturnStatus neworder_itemfn(Program *p, const ItemDV *idv, ClosureState* cs) {
        auto prg = (MV3CNewOrder *) p;
        auto ics = (NewOrderCS *) cs;
        auto ol_number = ics->ol_number;
        if (idv == nullptr) {
            //            throw std::logic_error("NewOrder Item missing");
            return ABORT;
        }
        prg->price[ol_number] = idv->val._3;
        prg->ol_amt[ol_number] = idv->val._3 * prg->quantity[ol_number];
        auto ol_i_id = prg->itemid[ol_number];
        auto ol_supply_w_id = prg->supware[ol_number];
        prg->stock[ol_number] = new (StockGet::store.add()) StockGet(&prg->stockTable, prg->xact, StockKey(ol_i_id, ol_supply_w_id), prg->item[ol_number], col_type((1 << 15) - 1));
        auto scs = new (NewOrderCS::store.add()) NewOrderCS(ol_number);
        auto sdv = prg->stock[ol_number]->evaluateAndExecute(prg->xact, neworder_stockfn, scs);
        return neworder_stockfn(p, sdv, scs);
    }

    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV *sdv, ClosureState *cs) {
        auto prg = (MV3CNewOrder*) p;
        auto scs = (NewOrderCS *) cs;
        auto ol_number = scs->ol_number;
        auto ol_qty = prg->quantity[ol_number];
        StockVal newsv = sdv->val;
        if (newsv._1 >= ol_qty + 10) {
            newsv._1 -= ol_qty;
        } else {
            newsv._1 += (91 - ol_qty);
        }
        newsv._12 += prg->quantity[ol_number];
        newsv._13++;
        if (prg->w_id != prg->supware[ol_number]) {
            newsv._14++;
        }
        switch (prg->d_id) {
            case 1: prg->dist_info[ol_number] = &newsv._2;
                break;
            case 2: prg->dist_info[ol_number] = &newsv._3;
                break;
            case 3: prg->dist_info[ol_number] = &newsv._4;
                break;
            case 4: prg->dist_info[ol_number] = &newsv._5;
                break;
            case 5: prg->dist_info[ol_number] = &newsv._6;
                break;
            case 6: prg->dist_info[ol_number] = &newsv._7;
                break;
            case 7: prg->dist_info[ol_number] = &newsv._8;
                break;
            case 8: prg->dist_info[ol_number] = &newsv._9;
                break;
            case 9: prg->dist_info[ol_number] = &newsv._10;
                break;
            case 10: prg->dist_info[ol_number] = &newsv._11;
                break;
        }
        if (prg->stockTable.update(prg->xact, sdv->entry, newsv, prg->stock[ol_number], ALLOW_WW, col_type(1 << 1 | 1 << 12 | 1 << 13 | 1 << 14)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("NewOrder stock");
            //            else
            //                return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV *ddv, ClosureState* cs) {
        auto prg = (MV3CNewOrder *) p;
        DistrictVal newdv = ddv->val;
        prg->d_tax = newdv._7;
        auto o_id = newdv._9;
        newdv._9++;
        if (prg->distTable.update(prg->xact, ddv->entry, newdv, prg->dist, ALLOW_WW, col_type(1 << 9)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("NewOrder stock");
            //            else
            //                return WW_ABORT;
        }
        if (prg->orderTable.insert(prg->xact, OrderKey(o_id, prg->d_id, prg->w_id), OrderVal(prg->c_id, prg->datetime, 0, prg->o_ol_cnt, prg->o_all_local), prg->dist) != OP_SUCCESS) {
            //            throw std::logic_error("NewOrder order");
            //            return WW_ABORT;
        }

        if (prg->newOrdTable.insert(prg->xact, NewOrderKey(o_id, prg->d_id, prg->w_id), NewOrderVal(true), prg->dist) != OP_SUCCESS) {
            //            throw std::logic_error("NewOrder neworder");
            //            return WW_ABORT;
        }
        for (uint8_t ol_number = 0; ol_number < prg->o_ol_cnt; ol_number++) {
            if (prg->ordLTable.insert(prg->xact, OrderLineKey(o_id, prg->d_id, prg->w_id, ol_number + 1), OrderLineVal(prg->itemid[ol_number], prg->supware[ol_number], nulldate, prg->quantity[ol_number], prg->ol_amt[ol_number], *prg->dist_info[ol_number]), prg->dist) != OP_SUCCESS) {
                //throw std::logic_error("NewORder orderline");
                //return WW_ABORTM
            }
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus payment_distfn(Program *p, const DistDV * ddv, ClosureState* cs);
    forceinline TransactionReturnStatus payment_warefn(Program *p, const WareDV * wdv, ClosureState* cs);
    forceinline TransactionReturnStatus payment_custfn(Program *p, const CustDV * cdv, ClosureState* cs);

    struct MV3CPayment : PaymentById {
        TABLE(Customer) &custTable;
        TABLE(District) &distTable;
        TABLE(Warehouse) &wareTable;
        TABLE(History) &histTable;
        DistGet * dist;
        WareGet * ware;
        CustGet *cust;

        MV3CPayment(Program *p, TABLE(Customer) & c, TABLE(District) & d, TABLE(Warehouse) & w, TABLE(History) & h) : PaymentById(p), custTable(c), distTable(d), wareTable(w), histTable(h) {
        }
        void cleanUp() override{
            dist->free();
            ware->free();
            cust->free(); 
        }

        TransactionReturnStatus execute() override {
            dist = new (DistGet::store.add()) DistGet(&distTable, xact, DistrictKey(d_id, w_id), nullptr, col_type(1 << 8));
            auto ddv = dist->evaluateAndExecute(xact, payment_distfn);
            auto status = payment_distfn(this, ddv, nullptr);
            if (status != SUCCESS) {
                return status;
            }
            ware = new (WareGet::store.add()) WareGet(&wareTable, xact, WarehouseKey(w_id), nullptr, col_type(1 << 8));
            auto wdv = ware -> evaluateAndExecute(xact, payment_warefn);
            status = payment_warefn(this, wdv, nullptr);
            if (status != SUCCESS) {
                return status;
            }

            cust = new (CustGet::store.add()) CustGet(&custTable, xact, CustomerKey(c_id, c_d_id, c_w_id), nullptr, (1 << 14 | 1 << 15 | 1 << 16));
            auto cdv = cust -> evaluateAndExecute(xact, payment_custfn);
            status = payment_custfn(this, cdv, nullptr);
            if (status != SUCCESS) {
                return status;
            }

            if (histTable.insert(xact, HistoryKey(c_id, c_d_id, c_w_id, d_id, w_id, datetime, h_amount, String<24>()), HistoryVal(true)) != OP_SUCCESS) {
                //                throw std::logic_error("payment hist");
                //                return WW_ABORT;
            }
            return SUCCESS;

        }

    };

    forceinline TransactionReturnStatus payment_distfn(Program* p, const DistDV* ddv, ClosureState* cs) {
        auto prg = (MV3CPayment *) p;
        DistrictVal newdv = ddv->val;
        newdv._8 += prg->h_amount;
        if (prg->distTable.update(prg->xact, ddv->entry, newdv, prg->dist, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment dist");
            //            else
            //                return WW_ABORT;
        }
        return SUCCESS;

    }

    forceinline TransactionReturnStatus payment_custfn(Program* p, const CustDV* cdv, ClosureState* cs) {
        auto prg = (MV3CPayment *) p;
        CustomerVal newcv = cdv->val;
        newcv._14 -= prg->h_amount;
        newcv._15 += prg->h_amount;
        newcv._16++;
        if (prg->custTable.update(prg->xact, cdv->entry, newcv, prg->cust, ALLOW_WW, col_type(1 << 14 | 1 << 15 | 1 << 16)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment cust");
            //            else
            //                return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus payment_warefn(Program* p, const WareDV* wdv, ClosureState* cs) {
        auto prg = (MV3CPayment *) p;
        WarehouseVal newwv = wdv->val;
        newwv._8 += prg->h_amount;
        if (prg->wareTable.update(prg->xact, wdv->entry, newwv, prg->ware, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment ware");
            //            else
            //                return WW_ABORT;
        }
        return SUCCESS;
    }
    forceinline TransactionReturnStatus delivery_distneworderfn(Program *p, const DistNODV* dnodv, ClosureState* cs);
    forceinline TransactionReturnStatus delivery_orderfn(Program *p, const OrderDV* odv, ClosureState* cs);
    forceinline TransactionReturnStatus delivery_custfn(Program *p, const CustDV* cdv, ClosureState* cs);

    struct DeliveryDistrictNewOrderCS : ClosureState {
        DistNOGet * const distno;
        typedef ConcurrentStore<DeliveryDistrictNewOrderCS> StoreType;
        static  StoreType store;

        void free() override {
            store.remove(this);
        }

        DeliveryDistrictNewOrderCS(DistNOGet* distno) :
        distno(distno) {
        }


    };

    struct DeliveryOrderCS : ClosureState {
        OrderGet * const order;
        const uint8_t d_id;
        typedef  ConcurrentStore<DeliveryOrderCS> StoreType;
        static StoreType store;

        void free() override {
            store.remove(this);
        }

        DeliveryOrderCS(OrderGet* order, uint8_t d_id) :
        order(order), d_id(d_id) {
        }


    };

    struct DeliveryCustCS : ClosureState {
        CustGet * const cust;
        typedef  ConcurrentStore<DeliveryCustCS> StoreType;
        static StoreType store;

        DeliveryCustCS(CustGet* cust) : cust(cust) {
        }

        void free() override {
            store.remove(this);
        }

    };

    struct MV3CDelivery : Delivery {
        //        TABLE(OrderLine) &ordLTable;
        TABLE(DistrictNewOrder) & distNewOrdTable;
        TABLE(NewOrder) & newOrdTable;
        TABLE(Order) &orderTable;
        TABLE(Customer) &custTable;

        MV3CDelivery(Program *p, Table<DistrictNewOrderKey, DistrictNewOrderVal>& distNewOrdTable, Table<NewOrderKey, NewOrderVal>& newOrdTable, Table<OrderKey, OrderVal>& orderTable, Table<CustomerKey, CustomerVal>& custTable) :
        Delivery(p), distNewOrdTable(distNewOrdTable), newOrdTable(newOrdTable), orderTable(orderTable), custTable(custTable) {
        }

        TransactionReturnStatus execute() override {
            for (uint8_t d_id = 1; d_id <= 10; ++d_id) {
                auto distno = new (DistNOGet::store.add()) DistNOGet(&distNewOrdTable, xact, DistrictNewOrderKey(d_id, w_id), nullptr, col_type(1 << 1));
                auto dnocs = new (DeliveryDistrictNewOrderCS::store.add()) DeliveryDistrictNewOrderCS(distno);
                auto dnodv = distno->evaluateAndExecute(xact, delivery_distneworderfn, dnocs);
                auto status = delivery_distneworderfn(this, dnodv, dnocs);
                if (status != SUCCESS) {
                    return status;
                }
            }
        }

    };

    forceinline TransactionReturnStatus delivery_distneworderfn(Program* p, const DistNODV* dnodv, ClosureState* cs) {
        auto prg = (MV3CDelivery *) p;
        auto dnocs = (DeliveryDistrictNewOrderCS *) cs;
        auto d_id = dnocs->distno->key._1;
        DistrictNewOrderVal newdnov = dnodv->val;
        auto o_id = newdnov._1++;
        if (prg->distNewOrdTable.update(prg->xact, dnodv->entry, newdnov, dnocs->distno, ALLOW_WW, col_type(1 << 1)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("delivery dist-neworder");
            //            else
            //                return WW_ABORT;
        }
        //        prg->newOrdTable.remove(.............)
        auto order = new (OrderGet::store.add()) OrderGet(&prg->orderTable, prg->xact, OrderKey(o_id, d_id, prg->w_id), dnocs->distno, col_type(1 << 1 | 1 << 3));
        auto ocs = new (DeliveryOrderCS::store.add()) DeliveryOrderCS(order, d_id);
        auto odv = order->evaluateAndExecute(prg->xact, delivery_orderfn, ocs);
        return delivery_orderfn(p, odv, ocs);

    }

    forceinline TransactionReturnStatus delivery_orderfn(Program* p, const OrderDV* odv, ClosureState* cs) {
        auto prg = (MV3CDelivery *) p;
        auto ocs = (DeliveryOrderCS *) cs;
        OrderVal newov = odv->val;
        auto c_id = newov._1;
        newov._3 = prg->o_carrier_id;
        if (prg->orderTable.update(prg->xact, odv->entry, newov, ocs->order, ALLOW_WW, col_type(1 << 3)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("delivery order");
            //            else
            //                return WW_ABORT;
        }
        auto cust = new (CustGet::store.add()) CustGet(&prg->custTable, prg->xact, CustomerKey(c_id, ocs->d_id, prg->w_id), ocs->order, col_type(1 << 14 | 1 << 17));
        auto ccs = new (DeliveryCustCS::store.add()) DeliveryCustCS(cust);
        auto cdv = cust->evaluateAndExecute(prg->xact, delivery_custfn, ccs);
        return delivery_custfn(p, cdv, ccs);
    }

    forceinline TransactionReturnStatus delivery_custfn(Program* p, const CustDV* cdv, ClosureState* cs) {
        auto prg = (MV3CDelivery *) p;
        auto ccs = (DeliveryCustCS *) cs;
        CustomerVal newcv = cdv->val;
        float ol_total = 1; //SHOULD BE OBTAINED from OrderlIne instead;
        newcv._14 += ol_total;
        newcv._17++;
        if (prg->custTable.update(prg->xact, cdv->entry, newcv, ccs->cust, ALLOW_WW, col_type(1 << 14 | 1 << 17))) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("delivery cust");
            //            else
            //                return WW_ABORT;
        }
        return SUCCESS;
    }
    struct MV3COrderStatus: OrderStatusById{
        TABLE(Order) &orderTable;
        TransactionReturnStatus execute() override{
            auto osdv = orderTable.getReadOnly(xact, OrderKey(-1, d_id, w_id)); // && cid = cid
            //FIND MAX with given cid
            auto o_id = osdv->val._1;
            
        }

    };
}
#endif /* NEWMV3CTPCC_H */

