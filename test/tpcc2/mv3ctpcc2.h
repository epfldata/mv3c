#include "util/types.h"
#ifdef TPCC2_TEST
#ifndef NEWMV3CTPCC2_H
#define NEWMV3CTPCC2_H
#include "tpcc2/tpcc2.h"
#include "predicate/Predicate.hpp"

namespace tpcc2_ns {
    typedef GETP(Stock) StockGet;
    typedef DV(Stock) StockDV;
    typedef SlicePred<StockKey, StockVal, StockPKey> StockSlice;
}
using namespace tpcc2_ns;

struct ThreadLocal {
    StockKey stockKey;
    StockGet stockGets[15];
    StockSlice stockSlice;
    InventoryKey invKey;
};
namespace tpcc2_ns {
    TABLE(Inventory) * InventoryTable;
    TABLE(Stock) * StockTable;

    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV * sdv, uint cs);

    struct MV3CNewOrder : NewOrder {

        MV3CNewOrder(const Program* p) :
        NewOrder(p) {
        }

        TransactionReturnStatus execute() override {
            TransactionReturnStatus status;

            for (uint8_t ol_number = 0; ol_number < o_ol_cnt; ++ol_number) {
                new (&threadVar->stockKey) StockKey(itemid[ol_number], supware[ol_number]);
                new (&threadVar->stockGets[ol_number]) StockGet(StockTable, &xact, threadVar->stockKey, nullptr);
                auto sdv = threadVar->stockGets[ol_number].evaluateAndExecute(&xact, neworder_stockfn, ol_number);
                status = neworder_stockfn(this, sdv, ol_number);
                if (status != SUCCESS)
                    return status;
            }
            return SUCCESS;
        }
    };

    forceinline TransactionReturnStatus neworder_stockfn(Program *p, const StockDV *sdv, uint ol_number) {
        auto prg = (MV3CNewOrder*) p;
        Transaction& xact = p->xact;
        auto threadVar = prg->threadVar;
        auto ol_qty = prg->quantity[ol_number];

        CreateValUpdate(Stock, newsv, sdv);
        newsv->_1 -= ol_qty;
        if (StockTable->update(&xact, sdv->entry, MakeRecord(newsv), &threadVar->stockGets[ol_number], false) != OP_SUCCESS) {
            return WW_ABORT;
        }
        return SUCCESS;
    }

    forceinline TransactionReturnStatus stockinv_stockfn(Program *p, const StockSlice::ResultType& sdvs, uint cs = -1);

    struct MV3CStockInventory : StockInventory {

        MV3CStockInventory(const Program *p) : StockInventory(p) {

        }

        TransactionReturnStatus execute() override {

            new (&threadVar->stockSlice) StockSlice(StockTable, &xact, 0, StockPKey(w_id));
            auto sdvs = threadVar->stockSlice.evaluateAndExecute(&xact, stockinv_stockfn);
            return stockinv_stockfn(this, sdvs);
        }

    };

    forceinline TransactionReturnStatus stockinv_stockfn(Program* p, const StockSlice::ResultType& sdvs, uint cs) {
        auto prg = (MV3CStockInventory *) p;
        Transaction &xact = p->xact;
        auto threadVar = p->threadVar;
        for (auto it = sdvs.begin(); it != sdvs.end(); ++it) {
            const StockDV* sdv = *it;
            auto i_id = sdv->entry->key._1;
            if (sdv->val._1 < prg->threshold) {
                auto orderQty = (prg->threshold - sdv->val._1)* 1.5;
                CreateValInsert(Inventory, newiv, orderQty);
                new (&threadVar->invKey) InventoryKey(prg->date, i_id, prg->w_id);
                if (InventoryTable->insert(&xact, threadVar->invKey, MakeRecord(newiv), &threadVar->stockSlice) != OP_SUCCESS) {
                    cerr << "INV table insert failed" << endl;
                    exit(-1);
                }
            }
        }
        return SUCCESS;
    }
}


#endif /* NEWMV3CTPCC_H */

#endif