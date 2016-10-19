#include "types.h"
#ifdef BANKING_TEST
#ifndef MV3CBANKING_H
#define	MV3CBANKING_H
#include <thread>
#include <cstdio>
#include "Banking.h"
namespace Banking {
    typedef GETP(Account) AccountGet;
    typedef DV(Account) AccountDV;
}
using namespace Banking;

struct ThreadLocal {
    AccountGet getFrom;
    AccountGet getTo;
    AccountGet getFee;
};
namespace Banking {
    forceinline TransactionReturnStatus fromFunction(Program *p, const AccountDV* advFrom, uint cs = -1);
    forceinline TransactionReturnStatus toFunction(Program *p, const AccountDV* advTo, uint cs = -1);
    forceinline TransactionReturnStatus feeFunction(Program *p, const AccountDV* advFee, uint cs = -1);

    struct mv3cTransfer : Program {
        const AccountKey from, to;
        const double amount;
        double fee;
        static Table<AccountKey, AccountVal>* AccountTable;

        mv3cTransfer(const Transfer& t) : Program((char)0), from(t.from), to(t.to), amount(t.amount) {

        }

        TransactionReturnStatus execute() override {
            fee = 1.0;
            if (amount > 100)
                fee = amount * 0.01;

            new(&threadVar->getFrom) AccountGet(AccountTable, &xact, from);
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
            if (mv3cTransfer::AccountTable->update(&xact, advFrom->entry, MakeRecord(newFromVal), &threadVar->getFrom) != OP_SUCCESS)
                throw std::logic_error("W-W conflict for from account");


            new(&threadVar->getTo) AccountGet(mv3cTransfer::AccountTable, &xact, prg->to, &threadVar->getFrom);
            auto advTo = threadVar->getTo.evaluateAndExecute(&xact, toFunction);
            auto status = toFunction(prg, advTo);
            if (status != SUCCESS)
                return status;

            new(&threadVar->getFee) AccountGet(mv3cTransfer::AccountTable, &xact, FeeAccount, &threadVar->getFrom);
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
        if (mv3cTransfer::AccountTable->update(&xact, advTo->entry, MakeRecord(newToVal), &threadVar->getTo) != OP_SUCCESS)
            throw std::logic_error("W-W conflict for To account");
        return SUCCESS;
    }

    forceinline TransactionReturnStatus feeFunction(Program* p, const AccountDV* advFee, uint cs) {
        auto prg = (mv3cTransfer *) p;
        Transaction& xact = prg->xact;
        auto threadVar = prg->threadVar;

        CreateValUpdate(Account, newFeeVal, advFee);
        newFeeVal->_1 += prg->fee;
        if (mv3cTransfer::AccountTable->update(&xact, advFee->entry, MakeRecord(newFeeVal), &threadVar->getFee, ALLOW_WW) != OP_SUCCESS)
            return WW_ABORT;
//            throw std::logic_error("W-W conflict for Fee account");
        return SUCCESS;

    }

}
#endif
#endif