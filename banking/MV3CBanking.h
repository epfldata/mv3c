#include "types.h"
#if defined(BANKING_TEST) || defined(RIPPLE_TEST)
#ifndef MV3CBANKING_H
#define MV3CBANKING_H
#include <thread>
#include <cstdio>
#include "Banking.h"
namespace Banking {
    typedef GETP(Account) AccountGet;
    typedef DV(Account) AccountDV;
}
using namespace Banking;

struct ThreadLocal {
    AccountKey from;
    AccountKey to;
    AccountGet getFrom;
    AccountGet getTo;
    AccountGet getFee;
};
namespace Banking {
    Table<AccountKey, AccountVal>* AccountTable;
    forceinline TransactionReturnStatus fromFunction(Program *p, const AccountDV* advFrom, uint cs = -1);
    forceinline TransactionReturnStatus toFunction(Program *p, const AccountDV* advTo, uint cs = -1);
    forceinline TransactionReturnStatus feeFunction(Program *p, const AccountDV* advFee, uint cs = -1);

    struct mv3cTransfer : Program {
        //        const AccountKey from, to;
        const double amount;
        double fee;

        mv3cTransfer() : Program((char) 0), amount(200) {
        }

        mv3cTransfer(const Transfer& t) : Program((char) 0), amount(t.amount) {

        }

        TransactionReturnStatus execute() override {
            fee = 1.0;
            if (amount > 100)
                fee = amount * 0.01;

            new(&threadVar->getFrom) AccountGet(AccountTable, &xact, threadVar->from);
            auto advFrom = threadVar->getFrom.evaluateAndExecute(&xact, fromFunction);
            return fromFunction(this, advFrom);
        }
    };

    forceinline TransactionReturnStatus fromFunction(Program *p, const AccountDV* advFrom, uint cs) {
        double fromVal = advFrom->val._1;
        auto prg = (mv3cTransfer *) p;
        Transaction &xact = p->xact;
        auto threadVar = prg->threadVar;
        if (fromVal > prg->amount + prg->fee) {

            CreateValUpdate(Account, newFromVal, advFrom);
            newFromVal->_1 -= (prg->amount + prg->fee);
            if (AccountTable->update(&xact, advFrom->entry, MakeRecord(newFromVal), &threadVar->getFrom) != OP_SUCCESS)
                throw std::logic_error("W-W conflict for from account");


            new(&threadVar->getTo) AccountGet(AccountTable, &xact, threadVar->to, &threadVar->getFrom);
            auto advTo = threadVar->getTo.evaluateAndExecute(&xact, toFunction);
            auto status = toFunction(prg, advTo);
            if (status != SUCCESS)
                return status;

            new(&threadVar->getFee) AccountGet(AccountTable, &xact, FeeAccount, &threadVar->getFrom);
            auto advFee = threadVar->getFee.evaluateAndExecute(&xact, feeFunction);
            return feeFunction(p, advFee);
        }
        return ABORT;
    }

    forceinline TransactionReturnStatus toFunction(Program* p, const AccountDV* advTo, uint cs) {
        auto prg = (mv3cTransfer *) p;
        Transaction& xact = prg->xact;
        auto threadVar = prg->threadVar;
        CreateValUpdate(Account, newToVal, advTo);
        newToVal->_1 += prg->amount;
        if (AccountTable->update(&xact, advTo->entry, MakeRecord(newToVal), &threadVar->getTo) != OP_SUCCESS)
            throw std::logic_error("W-W conflict for To account");
        return SUCCESS;
    }

    forceinline TransactionReturnStatus feeFunction(Program* p, const AccountDV* advFee, uint cs) {
        auto prg = (mv3cTransfer *) p;
        Transaction& xact = prg->xact;
        auto threadVar = prg->threadVar;

        CreateValUpdate(Account, newFeeVal, advFee);
        newFeeVal->_1 += prg->fee;
        if (AccountTable->update(&xact, advFee->entry, MakeRecord(newFeeVal), &threadVar->getFee, ALLOW_WW) != OP_SUCCESS)
            return WW_ABORT;
        //            throw std::logic_error("W-W conflict for Fee account");
        return SUCCESS;

    }

    //--------------------------------------------------------------------------
    forceinline TransactionReturnStatus fromFunctionNC(Program *p, const AccountDV* advFrom, uint cs = -1);
    forceinline TransactionReturnStatus toFunctionNC(Program *p, const AccountDV* advTo, uint cs = -1);

    struct mv3cTransferNoConflict : Program {
        //        const AccountKey from, to;
        const double amount;
        

        mv3cTransferNoConflict() : Program((char) 1), amount(200) {

        }

        mv3cTransferNoConflict(const Transfer& t) : Program((char) 1), amount(t.amount) {

        }

        TransactionReturnStatus execute() override {
            new(&threadVar->getFrom) AccountGet(AccountTable, &xact, threadVar->from);
            auto advFrom = threadVar->getFrom.evaluateAndExecute(&xact, fromFunctionNC);
            return fromFunctionNC(this, advFrom);
        }
    };

    forceinline TransactionReturnStatus fromFunctionNC(Program *p, const AccountDV* advFrom, uint cs) {
        double fromVal = advFrom->val._1;
        auto prg = (mv3cTransferNoConflict *) p;
        Transaction &xact = p->xact;
        auto threadVar = prg->threadVar;
        if (fromVal > prg->amount) {

            CreateValUpdate(Account, newFromVal, advFrom);
            newFromVal->_1 -= (prg->amount);
            if (AccountTable->update(&xact, advFrom->entry, MakeRecord(newFromVal), &threadVar->getFrom) != OP_SUCCESS)
                throw std::logic_error("W-W conflict for from account");


            new(&threadVar->getTo) AccountGet(AccountTable, &xact, threadVar->to, &threadVar->getFrom);
            auto advTo = threadVar->getTo.evaluateAndExecute(&xact, toFunctionNC);
            return toFunctionNC(prg, advTo);

        }
        return ABORT;
    }

    forceinline TransactionReturnStatus toFunctionNC(Program* p, const AccountDV* advTo, uint cs) {
        auto prg = (mv3cTransferNoConflict *) p;
        Transaction& xact = prg->xact;
        auto threadVar = prg->threadVar;
        CreateValUpdate(Account, newToVal, advTo);
        newToVal->_1 += prg->amount;
        if (AccountTable->update(&xact, advTo->entry, MakeRecord(newToVal), &threadVar->getTo) != OP_SUCCESS)
            throw std::logic_error("W-W conflict for To account");
        return SUCCESS;
    }

}
#endif
#endif