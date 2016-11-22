
#ifndef BANKING_H
#define BANKING_H
#include <cinttypes>
#include <cstring>
#include <functional>

#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <cassert>
#include "Tuple.h"
#include "types.h"

#ifndef FEEACCOUNTS
#define FEEACCOUNTS 1
#endif

namespace Banking {
    typedef KeyTuple<uint32_t> AccountKey;
    typedef ValTuple<double> AccountVal;

#ifdef NUMPROG
    const uint numPrograms = NUMPROG;
#else
    const uint numPrograms = 5040;
#endif
    const uint numThreads = NUMTHREADS;
    const uint numFeeAccounts = FEEACCOUNTS;
    const uint numUserAccounts = 2 * numThreads;
    const uint AccountSize = numUserAccounts + numFeeAccounts;
    const uint AccountEntrySize = AccountSize;
    const uint AccountDVSize = AccountSize + numPrograms;

    //    const AccountKey FeeAccount(AccountSize - 1);
    const int txnTypes = 2;
    std::string prgNames[] = {"T", "TNC"};

    struct Transfer {
        uint32_t from, to;
        double amount;

        Transfer() {
        }

        Transfer(uint32_t f, uint32_t t, double a) {
            from = f;
            to = t;
            amount = a;
        }
    };

    struct BankDataGenerator {
        Transfer *transfers;
        std::unordered_map<AccountKey, AccountVal, CityHasher<AccountKey>> iAccounts, oAccounts, fAccounts;

        BankDataGenerator() {
            //            transfers = new Transfer[numPrograms];
        }

        void loadPrograms() {
            //        transfers = new Transfer[numPrograms];
            //            for (uint i = 0; i < AccountSize - 1; i += 2) {
            //                transfers[i / 2] = Transfer(i, i + 1, 200);
            //            }
        }

        void loadData() {
            for (uint i = 0; i < numUserAccounts; i += 2) {
                iAccounts.insert({AccountKey(i), AccountVal(300 * numPrograms)});
                iAccounts.insert({AccountKey(i + 1), AccountVal(300 * numPrograms)});
                //                oAccounts.insert({AccountKey(i), AccountVal(798)});
                //                oAccounts.insert({AccountKey(i + 1), AccountVal(1200)});
            }
            for (uint i = numUserAccounts; i < AccountSize; ++i) {
                iAccounts.insert({AccountKey(i), AccountVal(0)});
            }
            //            oAccounts.insert({FeeAccount, AccountVal(2 * numPrograms)});
        }

        void checkData() {
            bool isOkay = true;
            for (const auto& it : oAccounts) {
                auto val = fAccounts.at(it.first)._1;
                if (val != it.second._1 && val != it.second._1 + 2) {
                    isOkay = false;
                    std::cerr << "Account " << it.first << " contains " << val << "  which should be " << it.second._1 << std::endl;
                }
            }
            if (isOkay)
                std::cout << "Account Table is correct" << std::endl;
        }

        ~BankDataGenerator() {
            //            delete transfers;
        }
    };
}
#endif /* BANKING_H */

