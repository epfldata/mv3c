#include "types.h"
#ifdef TPCC_TEST
#ifndef NEWMV3CTPCC_H
#define NEWMV3CTPCC_H
#include "TPCC.h"
#include "Predicate.hpp"

namespace tpcc_ns {
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
    typedef DV(OrderLine) OrderLineDV;
    typedef GETP(DistrictNewOrder) DistNOGet;
    typedef DV(DistrictNewOrder) DistNODV;
    typedef SLICEP(Customer) CustSlice;
}
using namespace tpcc_ns;

struct ThreadLocal {
    DistGet dist;
    CustGet cust;
    WareGet ware;
    CustSlice custSl;
    StockGet MV3CNewOrderStock[15];
    ItemGet MV3CNewOrderItem[15];
    float MV3CNewOrderOl_amt[15];


    //        DistGet dist;
    //        CustGet cust;
    //        WareGet ware;
    HistoryKey histKey;
    WarehouseKey wareKey;
    DistrictKey distKey;
    CustomerKey custKey;
    CustomerPKey custPKey;
    ItemKey itemKey;
    StockKey stockKey;
    OrderKey orderKey;
    OrderLineKey orderLineKey;
    NewOrderKey newOrderKey;


    DistNOGet distNOs[10];
    OrderGet orders[10];
    CustGet custs[10];
    DistrictNewOrderKey distNOkey;

    StockGet stockGet;
};
namespace tpcc_ns {
    TABLE(Customer) * CustomerTable;
    TABLE(District) * DistrictTable;
    TABLE(Warehouse) * WarehouseTable;
    TABLE(Item) * ItemTable;
    TABLE(Stock) * StockTable;
    TABLE(History)* HistoryTable;
    TABLE(Order) * OrderTable;
    TABLE(OrderLine) * OrderLineTable;
    TABLE(NewOrder) * NewOrderTable;
    TABLE(DistrictNewOrder)* DistrictNewOrderTable;


    forceinline TransactionReturnStatus neworder_itemfn(Program *p, const ItemDV * idv, uint cs);
    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV * sdv, uint cs);
    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV * ddv, uint cs = -1);
    forceinline TransactionReturnStatus neworder_warefn(Program *p, const WareDV * wdv, uint cs = -1);
    forceinline TransactionReturnStatus neworder_custfn(Program *p, const CustDV * cdv, uint cs = -1);

    struct MV3CNewOrder : NewOrder {
        //NOT INPUT PARAMATERS  
        float total;
        float w_tax, d_tax, c_disc;
        uint32_t o_id;

        MV3CNewOrder(const Program* p) :
        NewOrder(p) {
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status;

            new (&threadVar->distKey) DistrictKey(d_id, w_id);
            new (&threadVar->dist) DistGet(DistrictTable, &xact, threadVar->distKey, nullptr, col_type(1 << 7 || 1 << 9));
            auto ddv = threadVar->dist.evaluateAndExecute(&xact, neworder_distfn);
            status = neworder_distfn(this, ddv);
            if (status != SUCCESS) {
                //                throw std::logic_error("NewOrder dist");
                return status;
            }

            new (&threadVar->wareKey) WarehouseKey(w_id);
            new (&threadVar->ware) WareGet(WarehouseTable, &xact, threadVar->wareKey, nullptr, col_type(1 << 7));
            auto wdv = threadVar->ware.evaluateAndExecute(&xact, neworder_warefn);
            status = neworder_warefn(this, wdv);
            if (status != SUCCESS) {
                //                throw std::logic_error("New order ware");
                return status;
            }

            new (&threadVar->custKey) CustomerKey(c_id, d_id, w_id);
            new (&threadVar->cust) CustGet(CustomerTable, &xact, threadVar->custKey, nullptr, col_type(1 << 13));
            auto cdv = threadVar->cust.evaluateAndExecute(&xact, neworder_custfn);
            status = neworder_custfn(this, cdv);
            if (status != SUCCESS) {
                //                throw std::logic_error("New order cust");
                return status;
            }
            total = 0;
            for (uint8_t i = 0; i < o_ol_cnt; ++i) {
                total += threadVar->MV3CNewOrderOl_amt[i];
            }
            total *= (1 + w_tax + d_tax)*(1 - c_disc);
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
        new (&threadVar->MV3CNewOrderStock[ol_number]) StockGet(StockTable, &xact, threadVar->stockKey, &threadVar->MV3CNewOrderItem[ol_number], col_type((1 << 15) - 1));
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

        String<24>* dist_info = &newsv->_2 + (prg->d_id - 1);
        if (StockTable->update(&xact, sdv->entry, MakeRecord(newsv), &threadVar->MV3CNewOrderStock[ol_number], false, col_type(1 << 1 | 1 << 12 | 1 << 13 | 1 << 14)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //            throw std::logic_error("NewOrder stock");
            //            else
            return WW_ABORT;
        }

        new (&threadVar->orderLineKey) OrderLineKey(prg->o_id, prg->d_id, prg->w_id, ol_number + 1);
        CreateValInsert(OrderLine, newolv, prg->itemid[ol_number], prg->supware[ol_number], nulldate, prg->quantity[ol_number], threadVar->MV3CNewOrderOl_amt[ol_number], *dist_info);
        if (OrderLineTable->insert(&xact, threadVar->orderLineKey, MakeRecord(newolv), &threadVar->MV3CNewOrderStock[ol_number]) != OP_SUCCESS) {
            //                throw std::logic_error("NewORder orderline");
            return WW_ABORT;
        }

        return SUCCESS;
    }

    forceinline TransactionReturnStatus neworder_distfn(Program *p, const DistDV *ddv, uint cs) {
        auto prg = (MV3CNewOrder *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        const DistrictVal& oldVal = ddv->val;
        prg->d_tax = oldVal._7;
        auto o_id = oldVal._9;
        prg->o_id = o_id;


        new(&threadVar->orderKey) OrderKey(o_id, prg->d_id, prg->w_id);
        CreateValInsert(Order, newov, prg->c_id, prg->datetime, 0, prg->o_ol_cnt, prg->o_all_local);
        if (OrderTable->insert(&xact, threadVar->orderKey, MakeRecord(newov), &threadVar->dist) != OP_SUCCESS) {
            //                        throw std::logic_error("NewOrder order");
            xact.failureCtr++;
            return WW_ABORT;
        }
        CreateValUpdate(District, newdv, ddv);
        newdv->_9++;
        if (DistrictTable->update(&xact, ddv->entry, MakeRecord(newdv), &threadVar->dist, ALLOW_WW, col_type(1 << 9)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("NewOrder stock");
            //            else
            return WW_ABORT;
        }

        new(&threadVar->newOrderKey) NewOrderKey(o_id, prg->d_id, prg->w_id);
        CreateValInsert(NewOrder, newnov, true);
        if (NewOrderTable->insert(&xact, threadVar->newOrderKey, MakeRecord(newnov), &threadVar->dist) != OP_SUCCESS) {
            //            throw std::logic_error("NewOrder neworder");
            return WW_ABORT;
        }
        TransactionReturnStatus status;
        for (uint8_t ol_number = 0; ol_number < prg->o_ol_cnt; ol_number++) {
            uint32_t ol_i_id = prg->itemid[ol_number];
            new (&threadVar->itemKey) ItemKey(ol_i_id);
            new (&threadVar->MV3CNewOrderItem[ol_number]) ItemGet(ItemTable, &xact, threadVar->itemKey, &threadVar->dist, col_type(1 << 3));
            auto idv = threadVar->MV3CNewOrderItem[ol_number].evaluateAndExecute(&xact, neworder_itemfn, ol_number);
            status = neworder_itemfn(p, idv, ol_number);
            if (status != SUCCESS) {
                //                    cerr << "NewOrder Item" << endl;
                //                    throw std::logic_error("New order item fn");
                return status;
            }
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus neworder_custfn(Program* p, const CustDV* cdv, uint cs) {
        auto prg = (MV3CNewOrder *) p;
        prg->c_disc = cdv->val._13;
        return SUCCESS;
    }

    forceinline TransactionReturnStatus neworder_warefn(Program* p, const WareDV* wdv, uint cs) {
        auto prg = (MV3CNewOrder *) p;
        prg->w_tax = wdv->val._7;
        return SUCCESS;
    }
    forceinline TransactionReturnStatus payment_distfn(Program *p, const DistDV * ddv, uint cs = -1);
    forceinline TransactionReturnStatus payment_warefn(Program *p, const WareDV * wdv, uint cs = -1);
    forceinline TransactionReturnStatus payment_custfn(Program *p, const CustDV * cdv, uint cs = -1);
    forceinline TransactionReturnStatus payment_custfn2(Program *p, const CustSlice::ResultType& cdvs, uint cs = -1);

    struct MV3CPayment : Payment {

        MV3CPayment(const Program* p) :
        Payment(p) {
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status = SUCCESS;
            if (c_id != -1) {
                new(&threadVar->custKey)CustomerKey(c_id, c_d_id, c_w_id);
                new (&threadVar->cust) CustGet(CustomerTable, &xact, threadVar->custKey, nullptr, col_type(1 << 14 | 1 << 15 | 1 << 16));
                auto cdv = threadVar->cust.evaluateAndExecute(&xact, payment_custfn);
                status = payment_custfn(this, cdv);
                if (status != SUCCESS) {
                    return status;
                }
            } else {
                new(&threadVar -> custPKey) CustomerPKey(c_d_id, c_w_id, c_last);
                new(&threadVar -> custSl) CustSlice(CustomerTable, &xact, 0, threadVar->custPKey, nullptr, col_type(1 << 14 | 1 << 15 | 1 << 16));
                auto cdvs = threadVar->custSl.evaluateAndExecute(&xact, payment_custfn2);
                status = payment_custfn2(this, cdvs);
                if (status != SUCCESS)
                    return status;
            }

            new (&threadVar->distKey) DistrictKey(d_id, w_id);
            new (&threadVar->dist) DistGet(DistrictTable, &xact, threadVar->distKey, nullptr, col_type(1 << 8));
            auto ddv = threadVar->dist.evaluateAndExecute(&xact, payment_distfn);
            status = payment_distfn(this, ddv);
            if (status != SUCCESS) {
                return status;
            }
            std::string dname = to_string(h_amount) + to_string(c_id) + to_string(d_id);

            new(&threadVar->wareKey) WarehouseKey(w_id);

            new (&threadVar->ware) WareGet(WarehouseTable, &xact, threadVar->wareKey, nullptr, col_type(1 << 8));
            auto wdv = threadVar->ware.evaluateAndExecute(&xact, payment_warefn);
            status = payment_warefn(this, wdv);
            if (status != SUCCESS) {
                return status;
            }
            std::string wname = to_string(datetime);

            std::string hdata = wname.substr(0, 10) + "    " + dname.substr(0, 10);
            String<24> hval;
            hval.insertAt(0, hdata.c_str(), 24);

            new (&threadVar->histKey) HistoryKey(c_id, c_d_id, c_w_id, d_id, w_id, datetime, h_amount, hval);
            CreateValInsert(History, newhv, true);
            if (HistoryTable->insert(&xact, threadVar->histKey, MakeRecord(newhv)) != OP_SUCCESS) {
                cerr << "History Insertion failed" << endl;
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
        if (DistrictTable->update(&xact, ddv->entry, MakeRecord(newdv), &prg->threadVar->dist, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //            throw std::logic_error("payment dist");
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
        //
        std::string newDATA = to_string(prg->c_id) + " " + to_string(prg->c_d_id) + " " + to_string(prg->c_w_id) + " " + to_string(prg->d_id) + " " + to_string(prg->w_id) + " " + to_string(prg->h_amount) + " " + to_string(prg->datetime) + " | ";
        std::string olddata = cdv->val._18.c_str();
        std::string newdata2 = newDATA + olddata.substr(0, 500 - newDATA.size());
        String<500> newdata;
        newdata.insertAt(0, newdata2.c_str(), 500);
        newcv->_18 = newdata;


        if (CustomerTable->update(&xact, cdv->entry, MakeRecord(newcv), &prg->threadVar->cust, CWW, col_type(1 << 14 | 1 << 15 | 1 << 16)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment cust");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus payment_custfn2(Program* p, const CustSlice::ResultType& cdvs, uint cs) {
        auto prg = (MV3CPayment *) p;
        Transaction& xact = p->xact;
        const CustDV * cdvarray[cdvs.size()];
        int n = 0;
        for (auto it : cdvs) {
            cdvarray[n++] = it;
        }
        std::sort(cdvarray, cdvarray + n, [](const CustDV *c1, const CustDV * c2) {
            return c1->val._1 < c2->val._1;
        });
        n = (n - 1) / 2;
        auto cdv = cdvarray[n];


        CreateValUpdate(Customer, newcv, cdv);
        newcv->_14 -= prg->h_amount;
        newcv->_15 += prg->h_amount;
        newcv->_16++;
        //
        std::string newDATA = to_string(prg->c_id) + " " + to_string(prg->c_d_id) + " " + to_string(prg->c_w_id) + " " + to_string(prg->d_id) + " " + to_string(prg->w_id) + " " + to_string(prg->h_amount) + " " + to_string(prg->datetime) + " | ";
        std::string olddata = cdv->val._18.c_str();
        std::string newdata2 = newDATA + olddata.substr(0, 500 - newDATA.size());
        String<500> newdata;
        newdata.insertAt(0, newdata2.c_str(), 500);
        newcv->_18 = newdata;


        if (CustomerTable->update(&xact, cdv->entry, MakeRecord(newcv), &prg->threadVar->cust, CWW, col_type(1 << 14 | 1 << 15 | 1 << 16)) != OP_SUCCESS) {
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
        if (WarehouseTable->update(&xact, wdv->entry, MakeRecord(newwv), &prg->threadVar->ware, ALLOW_WW, col_type(1 << 8)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("payment ware");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }
    uint deliveryDistrictFailed = 0;
    forceinline TransactionReturnStatus delivery_distneworderfn(Program *p, const DistNODV* dnodv, uint cs);
    forceinline TransactionReturnStatus delivery_orderfn(Program *p, const OrderDV* odv, uint cs);
    forceinline TransactionReturnStatus delivery_custfn(Program *p, const CustDV* cdv, uint cs);

    struct MV3CDelivery : Delivery {
        uint8_t distFailed;

        MV3CDelivery(Program *p) : Delivery(p) {
        }

        TransactionReturnStatus execute() override {
            distFailed = 0;
            for (uint8_t d_id = 0; d_id < 10; ++d_id) {
                new (&threadVar->distNOkey) DistrictNewOrderKey(d_id + 1, w_id); //Reusing
                new (&threadVar->distNOs[d_id]) DistNOGet(DistrictNewOrderTable, &xact, threadVar->distNOkey, nullptr, col_type(1 << 1));

                auto dnodv = threadVar->distNOs[d_id].evaluateAndExecute(&xact, delivery_distneworderfn, d_id);
                auto status = delivery_distneworderfn(this, dnodv, d_id);
                if (status != SUCCESS) {
                    return status;
                }
            }
            return SUCCESS;
        }

    };

    forceinline TransactionReturnStatus delivery_distneworderfn(Program* p, const DistNODV* dnodv, uint d_id) {
        auto prg = (MV3CDelivery *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;

        CreateValUpdate(DistrictNewOrder, newdnov, dnodv);
        auto o_id = newdnov->_1++;
        if (DistrictNewOrderTable->update(&xact, dnodv->entry, MakeRecord(newdnov), &threadVar->distNOs[d_id], false, col_type(1 << 1)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //                throw std::logic_error("delivery dist-neworder");
            //            else
            return WW_ABORT;
        }
        //        prg->newOrdTable.remove(.............)
        new (&threadVar->orderKey) OrderKey(o_id, d_id + 1, prg->w_id);
        new (&threadVar->orders[d_id]) OrderGet(OrderTable, &xact, threadVar->orderKey, &threadVar->distNOs[d_id], col_type(1 << 1 | 1 << 3));

        auto odv = threadVar->orders[d_id].evaluateAndExecute(&xact, delivery_orderfn, d_id);
        return delivery_orderfn(p, odv, d_id);

    }

    forceinline TransactionReturnStatus delivery_orderfn(Program* p, const OrderDV* odv, uint d_id) {
        auto prg = (MV3CDelivery *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        CreateValUpdate(Order, newov, odv);
        auto c_id = newov->_1;
        newov->_3 = prg->o_carrier_id;
        if (OrderTable->update(&xact, odv->entry, MakeRecord(newov), &threadVar->orders[d_id], false, col_type(1 << 3)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //            throw std::logic_error("delivery order");
            //            else
            return WW_ABORT;
        }
        new (&threadVar->custKey)CustomerKey(c_id, d_id + 1, prg->w_id);
        new (&threadVar->custs[d_id]) CustGet(CustomerTable, &xact, threadVar->custKey, &threadVar->orders[d_id], col_type(1 << 14 | 1 << 17));

        auto cdv = threadVar->custs[d_id].evaluateAndExecute(&xact, delivery_custfn, d_id);
        return delivery_custfn(p, cdv, d_id);
    }

    forceinline TransactionReturnStatus delivery_custfn(Program* p, const CustDV* cdv, uint d_id) {
        auto prg = (MV3CDelivery *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        CreateValUpdate(Customer, newcv, cdv);

        float ol_total = 1; //SHOULD BE OBTAINED from OrderlIne instead;
        newcv->_14 += ol_total;
        newcv->_17++;
        if (CustomerTable->update(&xact, cdv->entry, MakeRecord(newcv), &threadVar->custs[d_id], CWW, col_type(1 << 14 | 1 << 17)) != OP_SUCCESS) {
            //            if (ALLOW_WW)
            //            throw std::logic_error("delivery cust");
            //            else
            return WW_ABORT;
        }
        return SUCCESS;
    }
    uint orderSuccess = 0, orderFailed = 0;

    struct MV3COrderStatus : OrderStatusById {

        MV3COrderStatus(const Program* p) :
        OrderStatusById(p) {
        }

        TransactionReturnStatus execute() override {

            auto osdv = OrderTable->getFirst<OrderPKey>(&xact, OrderPKey(d_id, w_id, c_id), 0);
            if (osdv == nullptr) {
                orderFailed++;
                return SUCCESS;
            }
            orderSuccess++;
            //FIND MAX with given cid
            auto o_id = osdv->val._1;
            OrderLineDV * sliceResults[15];
            auto count = OrderLineTable->sliceReadOnly<OrderLinePKey>(&xact, OrderLinePKey(o_id, d_id, w_id), 0, sliceResults);
            for (uint i = 0; i < count; ++i) {
                auto dv = sliceResults[i];
                auto e = dv->entry;
                auto i_id = e->key._4;
            }
            return SUCCESS;
        }
    };

    struct MV3CStockLevel : StockLevel {
        size_t totalCount;

        MV3CStockLevel(const Program *p) :
        StockLevel(p) {
        }

        TransactionReturnStatus execute() override {

            new (&threadVar->distKey) DistrictKey(d_id, w_id);

            auto ddv = DistrictTable->getReadOnly(&xact, threadVar->distKey);

            auto d_next_o_id = ddv->val._9;
            totalCount = 0;
            for (uint o_id = d_next_o_id - 5; o_id < d_next_o_id; ++o_id) {
                OrderLineDV * sliceResults[15];
                auto count = OrderLineTable->sliceReadOnly<OrderLinePKey>(&xact, OrderLinePKey(o_id, d_id, w_id), 0, sliceResults);
                totalCount += count;
            }
            return SUCCESS;
        }

    };


    forceinline TransactionReturnStatus stockupdate_stockfn(Program *p, const StockDV* sdv, uint cs = -1);

    struct MV3CStockUpdate : StockUpdate {

        MV3CStockUpdate(const Program *p) : StockUpdate(p) {

        }

        TransactionReturnStatus execute() override {
            throw std::logic_error("DISABLED");
            auto items = ItemTable->getAll(&xact);
            //FIX GET ALL
            std::sort(items.begin(), items.end(), [](std::pair<const ItemKey*, const ItemVal*> p1, std::pair<const ItemKey*, const ItemVal*> p2) {
                return p1.second->_3 > p2.second->_3;
            });
            auto i_id = items[k].first->_1;
            new (&threadVar->stockKey) StockKey(i_id, w_id);
            new (&threadVar->stockGet) StockGet(StockTable, &xact, threadVar->stockKey, nullptr, col_type(1 << 1));
            auto sdv = threadVar->stockGet.evaluateAndExecute(&xact, stockupdate_stockfn);

            return stockupdate_stockfn(this, sdv);
        }

    };

    forceinline TransactionReturnStatus stockupdate_stockfn(Program* p, const StockDV* sdv, uint cs) {
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        CreateValUpdate(Stock, newsv, sdv);
        newsv->_1 *= 1.1;
        if (StockTable->update(&xact, sdv->entry, MakeRecord(newsv), &threadVar->stockGet, ALLOW_WW, col_type(1 << 1)) != OP_SUCCESS) {
            return WW_ABORT;
        }
        return SUCCESS;
    }
}


#endif /* NEWMV3CTPCC_H */

#endif