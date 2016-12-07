
#ifndef TPCC2PARSER_H
#define TPCC2PARSER_H
#include <cinttypes>
#include <cstring>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <cassert>
#include "util/Tuple.h"
#include "util/Program.h"
#include <iomanip>
#include "util/types.h"
#include "tpcc2/mv3ctpcc2.h"

#include <algorithm>
#include <random>
namespace tpcc2_ns {

#ifndef NUMWARE
#define NUMWARE 1
#endif
    const int numWare = NUMWARE;

#ifdef NUMPROG
    const size_t numPrograms = NUMPROG;
#else
    const size_t numPrograms = 100;
#endif
    const uint8_t numThreads = NUMTHREADS;

    const size_t StockSize = 300;
    const size_t StockEntrySize = 0;
    const size_t StockDVSize = 0;

    const size_t InventoryEntrySize = 0;
    const size_t InventoryDVSize = 0;

#define Map(x) std::unordered_map<x##Key, x##Val, CityHasher<x##Key>>
    //------------------------------------------------------------------------------
    typedef size_t datetime_t;
    const datetime_t nulldate = 0;
    typedef KeyTuple<uint32_t, uint8_t> StockKey;
    typedef ValTuple<uint32_t> StockVal;

    typedef KeyTuple<datetime_t, uint32_t, uint8_t> InventoryKey;
    typedef ValTuple<uint32_t> InventoryVal;

    //---------------------------------------------

    struct StockPKey {
        uint8_t w_id;

        StockPKey() {
            memset(this, 0, sizeof (StockPKey));
        }

        StockPKey(uint8_t w_id) :
        w_id(w_id) {
            int padding = sizeof (StockPKey) - 1;
            memset(((char *) this) + 1, 0, padding);
        }

        StockPKey(const Entry<StockKey, StockVal>* e, const StockVal& v) {
            const StockKey& k = e->key;
            w_id = k._2;
            int padding = sizeof (StockPKey) - 1;
            memset(((char *) this) + 1, 0, padding);
        }

        bool operator==(const StockPKey& right) const {
            return w_id == right.w_id;
        }

    };
    //


    //-----------------------------------------

    enum TPCC2_Programs : char {
        NEWORDER, STOCKINVENTORY
    };
    const int txnTypes = 2;
    std::string prgNames[] = {"NO", "SI"};

    struct NewOrder : public Program {
        uint8_t o_ol_cnt;
        uint32_t itemid[15];
        uint8_t quantity[15], supware[15];

        NewOrder() : Program(NEWORDER) {

        }

        NewOrder(const Program *p) : Program(p) {
            assert(p->prgType == NEWORDER);
            const NewOrder* that = (const NewOrder*) p;


            o_ol_cnt = that -> o_ol_cnt;

            for (int i = 0; i < o_ol_cnt; i++) {
                itemid[i] = that->itemid[i];
                supware[i] = that->supware[i];
                quantity[i] = that ->quantity[i];
            }
        }

        virtual std::ostream& print(std::ostream& s) {
            s << "NewOrder  " << o_ol_cnt << " ";
            for (int i = 0; i < 15; i++)
                s << " " << itemid[i];
            s << " ";
            for (int i = 0; i < 15; i++)
                s << " " << supware[i];
            s << " ";
            for (int i = 0; i < 15; i++)
                s << " " << quantity[i];
            s << std::endl;
            return s;
        }

    };

    struct StockInventory : public Program {
        uint8_t w_id;
        uint32_t threshold;
        datetime_t date;

        StockInventory() : Program(STOCKINVENTORY) {
        }

        StockInventory(const Program* p) : Program(p) {
            assert(p->prgType == STOCKINVENTORY);
            const StockInventory* that = (const StockInventory*) p;
            w_id = that->w_id;
            date = that->date;
            threshold = that->threshold;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "StockInventory  " << w_id << "  " << date << "  " << threshold << std::endl;
            return s;
        }

    };



    //---------------------------------------------------------------------------

    ////////////

    struct TPCC2DataGen {
        Program** programs;
        Map(Stock) iStock;

        TPCC2DataGen() {
            programs = new Program*[numPrograms];
        }

        ~TPCC2DataGen() {
            for (size_t i = 0; i < numPrograms; i++)
                delete programs[i];
            delete programs;
            //            cerr << "TPCC deleted" << endl;
        }

        void loadPrograms() {
            const float NO_prob = 0.9;
            uint s1 = rand(), s2 = rand(), s3 = rand(), s4 = rand(), s5 = rand(), s6 = rand();
            uint32_t datetimeGen = 0;
            std::default_random_engine g1(s1);
            std::default_random_engine g2(s2);
            std::default_random_engine g3(s3);
            std::default_random_engine g4(s4);
            std::default_random_engine g5(s5);
            std::default_random_engine g6(s6);

            std::uniform_real_distribution<float> prgGen(0.0, 1.0);
            std::uniform_int_distribution<uint8_t> numOLGen(5, 15);
            std::uniform_int_distribution<uint32_t> iidGen(0, StockSize - 1);
            std::uniform_int_distribution<uint8_t> wareGen(1, numWare);
            std::uniform_int_distribution<uint8_t> qtyGen(1, 10);

            for (uint32_t i = 0; i < numPrograms; ++i) {
                if (prgGen(g1) < NO_prob) {
                    //NewOrder
                    std::unordered_set<uint32_t> iids;
                    NewOrder *p = new NewOrder();
                    uint8_t numOL = 5; //numOLGen(g2); FIXED
                    p->o_ol_cnt = numOL;
                    for (uint j = 0; j < numOL; ++j) {
                        uint32_t iid;
                        do {
                            iid = iidGen(g3);
                        } while (iids.find(iid) != iids.end());
                        iids.insert(iid);
                        p->itemid[j] = iid;
                        p->supware[j] = wareGen(g4);
                        p->quantity[j] = qtyGen(g5);
                    }
                    programs[i] = p;
                } else {
                    //StockInventory
                    StockInventory *p = new StockInventory();
                    p->date = datetimeGen++;
                    p->w_id = wareGen(g6);
                    p->threshold = 500; //FIXED
                    programs[i] = p;
                }
            }

        }

        void loadStocks() {
            uint s1 = rand();
            StockKey key;
            std::default_random_engine g1(s1);
            std::uniform_int_distribution<uint32_t> qtyGen(1000, 2000);
            for (uint i = 0; i < StockSize; ++i) {
                for (uint8_t w = 1; w <= numWare; ++w) {
                    new(&key)StockKey(i, w);
                    iStock.insert({key, StockVal(qtyGen(g1))});
                }
            }

        }
    };
}

#endif /* TPCCPARSER_H */

