#include "types.h"
#ifdef TRADING_TEST
#ifndef MV3CTRADING_H
#define MV3CTRADING_H
#include "Trading.h"
#include "TransactionManager.h"
#include <cstdio>
#include <chrono>
namespace trading_ns {
    typedef GETP(Security) SecGet;
    typedef DV(Security) SecDV;
    typedef GETP(Customer) CustGet;
    typedef DV(Customer) CustDV;
    typedef GETP(Trade) TradeGet;
    typedef DV(Trade) TradeDV;
    typedef GETP(TradeLine) TradeLGet;
    typedef DV(TradeLine) TradeLDV;
}
using namespace trading_ns;

struct ThreadLocal {
    CustGet custGet;
    CustomerKey custKey;
    TradeKey tradeKey;
    SecGet secGets[8];
    SecurityKey secKey;
    SecIDPrice sips[8];
    TradeLineKey tradeLkey;
};
namespace trading_ns {

    forceinline TransactionReturnStatus tradeorder_custfn(Program* p, const CustDV * cdv, uint cs = -1);
    forceinline TransactionReturnStatus tradeorder_secfn(Program* p, const SecDV * sdv, uint cs = -1);

    struct TradeOrderClosureState {
        uint8_t i;
        SecIDPrice sip;
        SecGet* sgp;
    };

    struct mv3cTradeOrder : public TradeOrder {
        static TABLE(Security) * SecurityTable;
        static TABLE(Trade) * TradeTable;
        static TABLE(TradeLine) * TradeLineTable;
        static TABLE(Customer) * CustomerTable;


        TradeRequest req;

        mykey_t key;

        mv3cTradeOrder(const Program* p) : TradeOrder(p) {
        }

        TransactionReturnStatus execute() override {
            assert(sizeof (TradeRequest) % BLOCK_SIZE == 0);
            assert(sizeof (TradeVal) % BLOCK_SIZE == 0);
            assert(sizeof (SecIDPrice) % BLOCK_SIZE == 0);
            new (&threadVar->custKey) CustomerKey(c_id);
            new (&threadVar->custGet) CustGet(CustomerTable, &xact, threadVar->custKey);
            auto cdv = threadVar->custGet.evaluateAndExecute(&xact, tradeorder_custfn);
            return tradeorder_custfn(this, cdv);
        }

    };

    forceinline TransactionReturnStatus tradeorder_custfn(Program* p, const CustDV * cdv, uint cs) {
        auto prg = (mv3cTradeOrder *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;
        prg->key = cdv->val._1.data;
        decrypt((char*) &prg->req, prg->request, prg->key, sizeof (TradeRequest), xact.threadId);
        String<sizeof (TradeValue) > enc_tradeval;
        TradeValue tv;
        tv.datetime = prg->req.datetime;
        encrypt(enc_tradeval.data, (char*) &tv, prg->key, sizeof (TradeValue), xact.threadId);
        new (&threadVar->tradeKey) TradeKey(prg->req.t_id);
        CreateValInsert(Trade, newTradeVal, prg->c_id, enc_tradeval);
        if (mv3cTradeOrder::TradeTable->insert(&xact, threadVar->tradeKey, MakeRecord(newTradeVal), &threadVar->custGet) != OP_SUCCESS)
            return WW_ABORT;

        for (uint8_t i = 0; i < prg->req.numSecs; ++i) {

            new(&threadVar->secKey) SecurityKey(prg->req.sec_ids[i]); //reused between multiple security within same txn.
            new(&threadVar->secGets[i]) SecGet(mv3cTradeOrder::SecurityTable, &xact, threadVar->secKey, &threadVar->custGet);
            threadVar->sips[i].sec_id = prg->req.sec_ids[i];
            auto sdv = threadVar->secGets[i].evaluateAndExecute(&xact, tradeorder_secfn, i);
            auto status = tradeorder_secfn(p, sdv, i);
            if (status != SUCCESS)
                return status;
        }

        return SUCCESS;
    }

    forceinline TransactionReturnStatus tradeorder_secfn(Program* p, const SecDV *sdv, uint cs) {
        auto prg = (mv3cTradeOrder *) p;
        Transaction& xact = p->xact;
        auto threadVar = p->threadVar;

        threadVar->sips[cs].price = sdv->val._1;

        String<sizeof (SecIDPrice) > enc_val;

        encrypt(enc_val.data, (char *) &threadVar->sips[cs], prg->key, sizeof (SecIDPrice), xact.threadId);
        new (&threadVar->tradeLkey) TradeLineKey(prg->req.t_id, cs);
        CreateValInsert(TradeLine, newTLval, enc_val);
        if (mv3cTradeOrder::TradeLineTable->insert(&xact, threadVar->tradeLkey, MakeRecord(newTLval), &threadVar->secGets[cs]) != OP_SUCCESS)
            return WW_ABORT;
        return SUCCESS;
    }

    struct mv3cPriceUpdate : public PriceUpdate {
        static TABLE(Security) * SecurityTable;

        mv3cPriceUpdate(const Program* p) : PriceUpdate(p) {
        }

        TransactionReturnStatus execute() override {
            new (&threadVar->secKey) SecurityKey(sec_id);
            auto sdv = SecurityTable->getReadOnly(&xact, threadVar->secKey);
            auto symbol = sdv->val._2;
            //Blind write
            CreateValInsert(Security, newSecVal, price, symbol);
            if (SecurityTable->update(&xact, sdv->entry, MakeRecord(newSecVal), nullptr, true) != OP_SUCCESS)
                return WW_ABORT;
            return SUCCESS;
        }
    };

}
#endif
#endif
