
#ifndef TPCCPARSER_H
#define TPCCPARSER_H
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
#include "Tuple.h"
#include "CustomString.h"
#include "Program.h"
#include <iomanip>
#include "types.h"

#include <algorithm>

namespace tpcc_ns {

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

    const size_t WarehouseEntrySize = numWare;
    const size_t WarehouseDVSize = numPrograms;

    const size_t DistrictEntrySize = numWare * 10;
    const size_t DistrictDVSize = 2 * numPrograms;

    const size_t DistrictNewOrderEntrySize = numWare * 10;
    const size_t DistrictNewOrderDVSize = numPrograms;

    const size_t CustomerEntrySize = DistrictEntrySize * 3000;
    const size_t CustomerDVSize = std::max(numPrograms, CustomerEntrySize);

    const size_t ItemEntrySize = 100000;
    const size_t ItemDVSize = 100000;

    const size_t StockEntrySize = numWare * ItemEntrySize;
    const size_t StockDVSize = std::max(30 * numPrograms, StockEntrySize);

    const size_t OrderEntrySize = std::max(CustomerEntrySize, numPrograms);
    const size_t OrderDVSize = OrderEntrySize;

    const size_t OrderLineEntrySize = OrderEntrySize * 12;
    const size_t OrderLineDVSize = OrderLineEntrySize;

    const size_t NewOrderEntrySize = OrderEntrySize;
    const size_t NewOrderDVSize = OrderEntrySize;

    const size_t HistoryEntrySize = OrderEntrySize;
    const size_t HistoryDVSize = OrderEntrySize;

    const size_t WarehouseIndexSize = 8 * (numWare / 8 + 1);
    const size_t ItemIndexSize = 100000;
    const size_t DistrictIndexSize = 8 * ((numWare * 10) / 8 + 1);
    const size_t CustomerIndexSize = DistrictIndexSize * 3000;
    const size_t OrderIndexSize = CustomerIndexSize * 1.5 + 0.5 * numPrograms;
    const size_t NewOrderIndexSize = OrderIndexSize * 0.3 + 0.5 * numPrograms;
    const size_t OrderLineIndexSize = OrderIndexSize * 12;
    const size_t StockIndexSize = numWare * ItemIndexSize;
    const size_t HistoryIndexSize = OrderIndexSize;

#ifdef PROJECT_ROOT
    const std::string TStore = PROJECT_ROOT;
#else
    const std::string TStore = "/home/sachin/TStore/";
#endif
    const std::string commandfile = TStore + "CavCommands" STRINGIFY(NUMWARE) ".txt";
    //const std::string inputTableDir = "/home/sachin/sem3/Project/test/input/";
    //const std::string outputTableDir = "/home/sachin/sem3/Project/test/output/";
    const std::string inputTableDir = TStore + "bench/systems/tpcc/mysql/db" STRINGIFY(NUMWARE) "innodb/";
    const std::string outputTableDir = TStore + "bench/systems/tpcc/mysql/results_db" STRINGIFY(NUMWARE) "innodb/";

#define Map(x) std::unordered_map<x##Key, x##Val, CityHasher<x##Key>>


    //------------------------------------------------------------------------------
    typedef size_t datetime_t;
    const datetime_t nulldate = 0;
    typedef KeyTuple<uint32_t, uint8_t, uint8_t> NewOrderKey;
    typedef ValTuple<bool> NewOrderVal;

    typedef KeyTuple<uint8_t, uint8_t> DistrictNewOrderKey;
    typedef ValTuple<uint32_t> DistrictNewOrderVal;

    typedef KeyTuple<uint32_t, uint8_t, uint8_t, uint8_t, uint32_t, datetime_t, float, String < 24 >> HistoryKey;
    typedef ValTuple<bool> HistoryVal;

    typedef KeyTuple<uint8_t> WarehouseKey;
    typedef ValTuple<String<10>, String<20>, String<20>, String<20>, String<2>, String<9>, float, double> WarehouseVal;

    typedef KeyTuple<uint32_t> ItemKey;
    typedef ValTuple<uint32_t, String<24>, float, String < 50 >> ItemVal;


    typedef KeyTuple<uint32_t, uint8_t, uint8_t, uint8_t> OrderLineKey;
    typedef ValTuple<uint32_t, uint8_t, datetime_t, uint8_t, float, String < 24 >> OrderLineVal;

    typedef KeyTuple<uint32_t, uint8_t, uint8_t> OrderKey;
    typedef ValTuple<uint32_t, datetime_t, uint8_t, u_int8_t, bool> OrderVal;


    typedef KeyTuple<uint8_t, uint8_t> DistrictKey;
    typedef ValTuple<String<10>, String<20>, String<20>, String<20>, String<2>, String<9>, float, double, uint32_t> DistrictVal;


    typedef KeyTuple<uint32_t, uint8_t, uint8_t> CustomerKey;
    typedef ValTuple<String<16>, String<2>, String<16>, String<20>, String<20>, String<20>, String<2>, String<9>, String<16>, datetime_t, String<2>, double, float, double, double, uint16_t, uint16_t, String < 500 >> CustomerVal;


    typedef KeyTuple<uint32_t, u_int8_t> StockKey;
    typedef ValTuple<uint8_t, String<24>, String<24>, String<24>, String<24>, String<24>, String<24>, String<24>, String<24>, String<24>, String<24>, uint32_t, u_int16_t, uint16_t, String<50> > StockVal;
    //-----------------------------------------------------------------------------------------

    //float rnd2(float f) {
    //    float f1 = f * 100;
    //    int i = f1;
    //    f1 -= i;
    //    if (f1 < 0.5)
    //        return i / 100.0;
    //    if (f1 > 0.5)
    //        return (i + 1) / 100.0;
    //    if (i % 2)
    //        return (i + 1) / 100.0;
    //    return i / 100.0;
    //}

    inline bool OLVequals(const OrderLineVal& t1, const OrderLineVal& t2) {
        if (!(t1.isNotNull && t2.isNotNull)) return !(t1.isNotNull || t2.isNotNull);
        return t1._1 == t2._1 && t1._2 == t2._2 && t1._3 == t2._3 && t1._4 == t2._4 && fabs(t1._5 - t2._5) <= 0.01 && t1._6 == t2._6;
    }

    inline bool Custequals(const CustomerVal& t1, const CustomerVal& t2) {
        if (!(t1.isNotNull && t2.isNotNull)) return !(t1.isNotNull || t2.isNotNull);
        return t1._1 == t2._1 && t1._2 == t2._2 && t1._3 == t2._3 && t1._4 == t2._4 && t1._5 == t2._5 && t1._6 == t2._6 && t1._7 == t2._7 && t1._8 == t2._8 && t1._9 == t2._9 && t1._10 == t2._10 && t1._11 == t2._11 && t1._12 == t2._12 && t1._13 == t2._13 && fabs(t1._14 - t2._14) <= 0.01 && t1._15 == t2._15 && t1._16 == t2._16 && t1._17 == t2._17 && t1._18 == t2._18;
    }

    //---------------------------------------------

    struct OrderPKey {
        uint8_t d_id, w_id;
        uint32_t c_id;

        OrderPKey(uint8_t d_id, uint8_t w_id, uint32_t c_id) :
        d_id(d_id), w_id(w_id), c_id(c_id) {
        }

        OrderPKey(const Entry<OrderKey, OrderVal>* e, const OrderVal& v) {
            const OrderKey& k = e->key;
            d_id = k._2;
            w_id = k._3;
            c_id = v._1;
        }

        bool operator<(const OrderPKey& that) const {
            if (w_id != that.w_id)
                return w_id < that.w_id;
            else if (d_id != that.d_id)
                return d_id < that.d_id;
            else
                return c_id < that.c_id;
        }
    };
    //

    struct OrderLinePKey {
        uint32_t o_id;
        uint8_t d_id, w_id;

        OrderLinePKey(uint32_t o_id, uint8_t d_id, uint8_t w_id) :
        o_id(o_id), d_id(d_id), w_id(w_id) {
        }

        OrderLinePKey(const Entry<OrderLineKey, OrderLineVal>* e, const OrderLineVal& v) {
            const OrderLineKey & k = e->key;
            o_id = k._1;
            d_id = k._2;
            w_id = k._3;
        }

        bool operator<(const OrderLinePKey& that) const {
            if (w_id != that.w_id)
                return w_id < that.w_id;
            else if (d_id != that.d_id)
                return d_id < that.d_id;
            else
                return o_id < that.o_id;
        }

    };
    //
    //        static inline size_t hash(const OrderLineKey& k) {
    //            size_t hash = 0xcafebabe;
    //            const unsigned int mask = (CHAR_BIT * sizeof (size_t) - 1);
    //            size_t mix = OrderLineKey::h1(k._1)*0xcc9e2d51;
    //            mix = (mix << 15) | (mix >> ((-15) & mask));
    //            mix = (mix * 0x1b873593) ^ hash;
    //            mix = (mix << 13) | (mix >> ((-13) & mask));
    //            hash = (mix << 1) + mix + 0xe6546b64;
    //            mix = OrderLineKey::h2(k._2)*0xcc9e2d51;
    //            mix = (mix << 15) | (mix >> ((-15) & mask));
    //            mix = (mix * 0x1b873593) ^ hash;
    //            mix = (mix << 13) | (mix >> ((-13) & mask));
    //            hash = (mix << 1) + mix + 0xe6546b64;
    //            mix = OrderLineKey::h3(k._3)*0xcc9e2d51;
    //            mix = (mix << 15) | (mix >> ((-15) & mask));
    //            mix = (mix * 0x1b873593) ^ hash;
    //            mix = (mix << 13) | (mix >> ((-13) & mask));
    //            hash = (mix << 1) + mix + 0xe6546b64;
    //            hash ^= 3;
    //            hash ^= (hash >> 16);
    //            hash *= 0x85ebca6b;
    //            hash ^= (hash >> 13);
    //            hash *= 0xc2b2ae35;
    //            hash ^= (hash >> 16);
    //            return hash;
    //        }
    //
    //        static inline bool equals(const OrderLineKey& k1, const OrderLineKey& k2) {
    //            return k1._1 == k2._1 && k1._2 == k2._2 && k1._3 == k2._3;
    //        }
    //    };
    //-----------------------------------------

    enum TPCC_Programs : char {
        NEWORDER, PAYMENTBYID, ORDERSTATUSBYID, DELIVERY, STOCKLEVEL, STOCKUPDATE, PAYMENTBYNAME, ORDERSTATUSBYNAME
    };
    const int txnTypes = 6;
    std::string prgNames[] = {"NO", "PY", "OS", "DE", "SL", "SU"};

    struct NewOrder : public Program {
        uint32_t c_id;
        uint8_t d_id, w_id, o_ol_cnt;
        datetime_t datetime;

        uint32_t itemid[15];
        uint8_t quantity[15], supware[15];

        bool o_all_local;

        NewOrder() : Program(NEWORDER) {
            o_all_local = true;
        }

        NewOrder(const Program *p) : Program(p) {
            assert(p->prgType == NEWORDER);
            const NewOrder* that = (const NewOrder*) p;
            c_id = that->c_id;
            d_id = that->d_id;
            w_id = that->w_id;
            o_ol_cnt = that -> o_ol_cnt;
            datetime = that->datetime;
            o_all_local = true;
            for (int i = 0; i < o_ol_cnt; i++) {
                itemid[i] = that->itemid[i];
                supware[i] = that->supware[i];
                if (supware[i] != w_id)
                    o_all_local = false;
                quantity[i] = that ->quantity[i];
            }
        }

        virtual std::ostream& print(std::ostream& s) {
            s << "NewOrder  " << datetime << "  " << w_id << "  " << d_id << "  " << c_id << "  " << o_ol_cnt << " ";
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

    struct PaymentById : public Program {
        datetime_t datetime;
        uint32_t c_id;
        uint8_t w_id, c_w_id, d_id, c_d_id;
        float h_amount;

        PaymentById() : Program(PAYMENTBYID) {
        }

        PaymentById(const Program* p) : Program(p) {
            assert(p->prgType == PAYMENTBYID);
            const PaymentById* that = (const PaymentById*) p;
            datetime = that->datetime;
            c_id = that->c_id;
            w_id = that->w_id;
            c_w_id = that->c_w_id;
            d_id = that->d_id;
            c_d_id = that->c_d_id;
            h_amount = that->h_amount;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "PaymentById  " << datetime << "  " << w_id << "  " << d_id << "  " << c_w_id << "  " << c_d_id << "  " << c_id << "  " << h_amount << std::endl;

            return s;
        }

    };

    struct PaymentByName : public Program {
        datetime_t datetime;
        uint8_t w_id, c_w_id;
        uint8_t d_id, c_d_id;
        String<16> c_last_input;
        float h_amount;

        PaymentByName() : Program(PAYMENTBYNAME) {
        }

        PaymentByName(const Program* p) : Program(p) {
            assert(p->prgType == PAYMENTBYNAME);
            const PaymentByName* that = (const PaymentByName*) p;
            datetime = that->datetime;
            w_id = that->w_id;
            c_w_id = that->c_w_id;
            d_id = that->d_id;
            c_d_id = that->c_d_id;
            c_last_input = that->c_last_input;
            h_amount = that->h_amount;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "PaymentByName  " << datetime << "  " << w_id << "  " << d_id << "  " << c_w_id << "  " << c_d_id << "  " << c_last_input << "  " << h_amount << std::endl;

            return s;
        }
    };

    struct OrderStatusById : public Program {
        uint32_t c_id;
        uint8_t w_id, d_id;
        //OUTPUT

        OrderStatusById() : Program(ORDERSTATUSBYID) {
        }

        OrderStatusById(const Program* p) : Program(p) {
            assert(p->prgType == ORDERSTATUSBYID);
            const OrderStatusById* that = (const OrderStatusById*) p;
            c_id = that->c_id;
            w_id = that->w_id;
            d_id = that->d_id;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "OrderStatusById  " << w_id << "  " << d_id << "  " << c_id << std::endl;
            return s;
        }

    };

    struct OrderStatusByName : public Program {
        uint8_t w_id;
        uint8_t d_id;
        String<16> c_last;
        //OUTPUT
        std::stringstream orderlinesSTR;

        OrderStatusByName() : Program(ORDERSTATUSBYNAME) {
        }

        OrderStatusByName(const Program * p) : Program(p) {
            assert(p->prgType == ORDERSTATUSBYNAME);
            const OrderStatusByName* that = (const OrderStatusByName*) p;
            w_id = that->w_id;
            d_id = that->d_id;
            c_last = that->c_last;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "OrderStatusByName  " << w_id << "  " << d_id << "  " << c_last << std::endl;
            return s;
        }

        virtual Program* clone() {
            return nullptr;
        }
    };

    struct StockLevel : public Program {
        uint8_t w_id;
        uint8_t d_id, threshold;
        std::unordered_set< uint32_t> unique_ol_i_id;

        StockLevel() : Program(STOCKLEVEL) {
        }

        StockLevel(const Program* p) : Program(p) {
            assert(p->prgType == STOCKLEVEL);
            const StockLevel* that = (const StockLevel*) p;
            w_id = that->w_id;
            d_id = that->d_id;
            threshold = that->threshold;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "StockLevel  " << w_id << "  " << d_id << "  " << threshold << std::endl;
            return s;
        }

    };

    struct StockUpdate : public Program {
        uint8_t w_id;
        uint8_t k;

        StockUpdate() : Program(STOCKUPDATE) {
        }

        StockUpdate(const Program* p) : Program(p) {
            assert(p->prgType == STOCKUPDATE);
            const StockUpdate* that = (const StockUpdate*) p;
            w_id = that->w_id;
            k = that->k;

        }

        virtual std::ostream& print(std::ostream & s) {
            s << "StockUpdate  " << w_id << "  " << k << std::endl;
            return s;
        }

    };

    struct Delivery : public Program {
        uint8_t w_id;
        uint8_t o_carrier_id;
        datetime_t datetime;

        Delivery() : Program(DELIVERY) {
        }

        Delivery(const Program *p) : Program(p) {
            assert(p->prgType == DELIVERY);
            const Delivery* that = (const Delivery*) p;
            w_id = that->w_id;
            o_carrier_id = that->o_carrier_id;
            datetime = that->datetime;
        }

        virtual std::ostream& print(std::ostream & s) {
            s << "Delivery  " << datetime << "  " << w_id << "  " << o_carrier_id << std::endl;
            return s;
        }

    };

    //---------------------------------------------------------------------------

    datetime_t StrToIntDate(const char* s) {
        datetime_t d = s[2] - '0';
        //        d = d * 10 + s[1] - '0';
        //        d = d * 10 + s[2] - '0';
        d = d * 10 + s[3] - '0';
        d = d * 10 + s[5] - '0';
        d = d * 10 + s[6] - '0';
        d = d * 10 + s[8] - '0';
        d = d * 10 + s[9] - '0';
        d = d * 10 + s[11] - '0';
        d = d * 10 + s[12] - '0';
        d = d * 10 + s[14] - '0';
        d = d * 10 + s[15] - '0';
        d = d * 10 + s[17] - '0';
        d = d * 10 + s[18] - '0';
        return d;
    }

    void IntToStrDate(char* s, datetime_t d) {
        s[0] = '2';
        s[1] = '0';
        s[21] = 0;
        s[20] = '0';
        s[19] = '.';
        s[18] = d % 10 + '0';
        d /= 10;
        s[17] = d % 10 + '0';
        d /= 10;
        s[16] = ':';
        s[15] = d % 10 + '0';
        d /= 10;
        s[14] = d % 10 + '0';
        d /= 10;
        s[13] = ':';
        s[12] = d % 10 + '0';
        d /= 10;
        s[11] = d % 10 + '0';
        d /= 10;
        s[10] = ' ';
        s[9] = d % 10 + '0';
        d /= 10;
        s[8] = d % 10 + '0';
        d /= 10;
        s[7] = '-';
        s[6] = d % 10 + '0';
        d /= 10;
        s[5] = d % 10 + '0';
        d /= 10;
        s[4] = '-';
        s[3] = d % 10 + '0';
        d /= 10;
        s[2] = d % 10 + '0';
        d /= 10;

    }
    ////////////

    struct TPCCDataGen {
        Program** programs;
        Map(Customer) iCustomer, oCustomer, fCustomer;
        Map(Warehouse) iWarehouse, oWarehouse, fWarehouse;
        Map(District) iDistrict, oDistrict, fDistrict;
        Map(History) iHistory, oHistory, fHistory;
        Map(NewOrder) iNewOrder, oNewOrder, fNewOrder;
        Map(Order) iOrder, oOrder, fOrder;
        Map(OrderLine) iOrderLine, oOrderLine, fOrderLine;
        Map(Item) iItem, oItem, fItem;
        Map(Stock) iStock, oStock, fStock;

#define STR "\"%[^\"]\""
        //#define u64 "%" SCNu64
#define DATE STR    
#define u32 "%" SCNu32
#define u16 "%" SCNu16
#define u8 "%" SCNu8
#define fp "%f"
#define dp "%lf"
#define nullable "%[^,]"   

        TPCCDataGen() {
            programs = new Program*[numPrograms];
        }

        ~TPCCDataGen() {
            for (size_t i = 0; i < numPrograms; i++)
                delete programs[i];
            delete programs;
            //            cerr << "TPCC deleted" << endl;
        }

        void loadPrograms() {
            std::ifstream fin(commandfile);
            std::string line;
            size_t curPrg = 0;
            while (std::getline(fin, line) && curPrg < numPrograms) {
                std::stringstream ss(line);
                std::string type;
                ss >> type;
                if (type == "NewOrder") {
                    NewOrder* o = new NewOrder();
                    ss >> o->datetime >> o->w_id >> o->d_id >> o->c_id >> o->o_ol_cnt;
                    for (int i = 0; i < 15; i++)
                        ss >> o->itemid[i];
                    for (int i = 0; i < 15; i++)
                        ss >> o->supware[i];
                    for (int i = 0; i < 15; i++)
                        ss >> o->quantity[i];
                    programs[curPrg++] = o;
                } else if (type == "PaymentById") {
                    PaymentById* o = new PaymentById();
                    ss >> o->datetime >> o->w_id >> o->d_id >> o->c_w_id >> o->c_d_id >> o->c_id >> o->h_amount;
                    programs[curPrg++] = o;
                } else if (type == "PaymentByName") {
                    PaymentByName* o = new PaymentByName();
                    ss >> o->datetime >> o->w_id >> o->d_id >> o->c_w_id >> o->c_d_id >> o->c_last_input >> o->h_amount;
                    programs[curPrg++] = o;
                } else if (type == "OrderStatusById") {
                    OrderStatusById* o = new OrderStatusById();
                    ss >> o->w_id >> o->d_id >> o->c_id;
                    programs[curPrg++] = o;
                } else if (type == "OrderStatusByName") {
                    OrderStatusByName* o = new OrderStatusByName();
                    ss >> o->w_id >> o->d_id >> o->c_last;
                    programs[curPrg++] = o;
                } else if (type == "Delivery") {
                    Delivery* o = new Delivery();
                    ss >> o->datetime >> o->w_id >> o->o_carrier_id;
                    programs[curPrg++] = o;
                } else if (type == "StockLevel") {
                    //Reading stock level as StockUPDATE
//                    StockUpdate * o = new StockUpdate();
//                    ss >> o->w_id >> o->k;
//                    uint8_t t;
//                    ss >> t;
                    StockLevel* o = new StockLevel();
                    ss >> o->w_id >> o->d_id >> o->threshold;
                    programs[curPrg++] = o;
                } else {
                    std::cerr << "UNKNOWN PROGRAM TYPE" << type << std::endl;
                }
            }
            fin.close();
        }

        void loadCust() {
            std::ifstream fin(inputTableDir + "customer.txt");
            std::string line;
            CustomerKey ck;
            CustomerVal cv;
            cv.isNotNull = true;
            char date[20];
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," DATE "," STR "," dp "," fp "," dp "," dp "," u16 "," u16 "," STR, &ck._1, &ck._2, &ck._3, cv._1.data, cv._2.data, cv._3.data, cv._4.data, cv._5.data, cv._6.data, cv._7.data, cv._8.data, cv._9.data, date, cv._11.data, &cv._12, &cv._13, &cv._14, &cv._15, &cv._16, &cv._17, cv._18.data);
                cv._10 = StrToIntDate(date);
                iCustomer.insert({ck, cv});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "customer.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," DATE "," STR "," dp "," fp "," dp "," dp "," u16 "," u16 "," STR, &ck._1, &ck._2, &ck._3, cv._1.data, cv._2.data, cv._3.data, cv._4.data, cv._5.data, cv._6.data, cv._7.data, cv._8.data, cv._9.data, date, cv._11.data, &cv._12, &cv._13, &cv._14, &cv._15, &cv._16, &cv._17, cv._18.data);
                cv._10 = StrToIntDate(date);
                oCustomer.insert({ck, cv});
            }
            fin.close();
#endif
        }

        void loadDist() {
            std::string line;
            std::ifstream fin(inputTableDir + "district.txt");

            DistrictKey k;
            DistrictVal v;
            v.isNotNull = true;

            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," fp "," dp "," u32, &k._1, &k._2, v._1.data, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, &v._7, &v._8, &v._9);
                iDistrict.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "district.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," fp "," dp "," u32, &k._1, &k._2, v._1.data, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, &v._7, &v._8, &v._9);
                oDistrict.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadHist() {

            std::string line;
            std::ifstream fin;
            char date[20];
            HistoryKey k;
            HistoryVal v;
            v.isNotNull = true;
            v._1 = true;
            fin.open(inputTableDir + "history.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u8 "," u32 "," DATE "," fp "," STR, &k._1, &k._2, &k._3, &k._4, &k._5, date, &k._7, k._8.data);
                k._6 = StrToIntDate(date);
                iHistory.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "history.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u8 "," u32 "," DATE "," fp "," STR, &k._1, &k._2, &k._3, &k._4, &k._5, date, &k._7, k._8.data);
                k._6 = StrToIntDate(date);
                oHistory.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadItem() {

            std::string line;
            std::ifstream fin;
            ItemKey k;
            ItemVal v;
            v.isNotNull = true;
            fin.open(inputTableDir + "item.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u32 "," STR "," fp "," STR, &k._1, &v._1, v._2.data, &v._3, v._4.data);
                iItem.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "item.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u32 "," STR "," fp "," STR, &k._1, &v._1, v._2.data, &v._3, v._4.data);
                oItem.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadNewOrd() {
            std::string line;
            std::ifstream fin;

            NewOrderKey k;
            NewOrderVal v;
            v.isNotNull = true;
            v._1 = true;
            fin.open(inputTableDir + "new_orders.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8, &k._1, &k._2, &k._3);
                iNewOrder.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "new_orders.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8, &k._1, &k._2, &k._3);
                oNewOrder.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadOrdLine() {

            std::string line;
            std::ifstream fin;
            char date[20];
            OrderLineKey k;
            OrderLineVal v;
            v.isNotNull = true;
            fin.open(inputTableDir + "order_line.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u8 "," u32 "," u8 "," nullable "," u8 "," fp "," STR, &k._1, &k._2, &k._3, &k._4, &v._1, &v._2, date, &v._4, &v._5, v._6.data);
                v._3 = strcmp(date, "\\N") == 0 ? 0 : StrToIntDate(date + 1);
                iOrderLine.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "order_line.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u8 "," u32 "," u8 "," nullable "," u8 "," fp "," STR, &k._1, &k._2, &k._3, &k._4, &v._1, &v._2, date, &v._4, &v._5, v._6.data);
                v._3 = strcmp(date, "\\N") == 0 ? 0 : StrToIntDate(date + 1);
                oOrderLine.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadOrders() {

            std::string line;
            std::ifstream fin;
            char date[20];

            OrderKey k;
            OrderVal v;
            v.isNotNull = true;
            char carrier[5];
            uint8_t local;
            fin.open(inputTableDir + "orders.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u32 "," DATE "," nullable "," u8 "," u8, &k._1, &k._2, &k._3, &v._1, date, carrier, &v._4, &local);
                v._2 = StrToIntDate(date);
                v._3 = strcmp(carrier, "\\N") == 0 ? 0 : atoi(carrier);
                v._5 = local;
                iOrder.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "orders.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," u32 "," DATE "," nullable "," u8 "," u8, &k._1, &k._2, &k._3, &v._1, date, carrier, &v._4, &local);
                v._2 = StrToIntDate(date);
                v._3 = strcmp(carrier, "\\N") == 0 ? 0 : atoi(carrier);
                v._5 = local;
                oOrder.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadStocks() {

            std::string line;
            std::ifstream fin;
            StockKey k;
            StockVal v;
            v.isNotNull = true;
            fin.open(inputTableDir + "stock.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," u32 "," u16 "," u16 "," STR, &k._1, &k._2, &v._1, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, v._7.data, v._8.data, v._9.data, v._10.data, v._11.data, &v._12, &v._13, &v._14, v._15.data);
                iStock.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "stock.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u32 "," u8 "," u8 "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," STR "," u32 "," u16 "," u16 "," STR, &k._1, &k._2, &v._1, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, v._7.data, v._8.data, v._9.data, v._10.data, v._11.data, &v._12, &v._13, &v._14, v._15.data);
                oStock.insert({k, v});
            }
            fin.close();
#endif
        }

        void loadWare() {

            std::string line;
            std::ifstream fin;
            WarehouseKey k;
            WarehouseVal v;
            v.isNotNull = true;
            fin.open(inputTableDir + "warehouse.txt");
            assert(fin.good());
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u8 "," STR "," STR "," STR "," STR "," STR "," STR "," fp "," dp, &k._1, v._1.data, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, &v._7, &v._8);
                iWarehouse.insert({k, v});
            }
            fin.close();
#ifdef VERIFY_TPCC
            fin.open(outputTableDir + "warehouse.txt");
            while (std::getline(fin, line)) {
                sscanf(line.c_str(), u8 "," STR "," STR "," STR "," STR "," STR "," STR "," fp "," dp, &k._1, v._1.data, v._2.data, v._3.data, v._4.data, v._5.data, v._6.data, &v._7, &v._8);
                oWarehouse.insert({k, v});
            }
            fin.close();
#endif
        }

        void checkCustomerResults() {

            bool isOkay = true;
            for (const auto& it : oCustomer) {
                try {
                    const CustomerVal& v = fCustomer.at(it.first);
                    if (!Custequals(v, it.second)) {
                        isOkay = false;
                        std::cerr << "Customer " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Customer " << it.first << " not present in table" << std::endl;
                    isOkay = false;

                    return;
                }
            }
            for (const auto& it : fCustomer) {
                try {
                    const CustomerVal& v = oCustomer.at(it.first);
                    if (!Custequals(v, it.second)) {
                        isOkay = false;
                        std::cerr << "Customer " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Customer " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "Customer table results are correct" << std::endl;
        }

        void checkDistrictResults() {

            bool isOkay = true;
            for (const auto& it : oDistrict) {
                try {
                    const DistrictVal& v = fDistrict.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "District " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "District " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fDistrict) {
                try {
                    const DistrictVal& v = oDistrict.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "District " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "District " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "District table results are correct" << std::endl;
        }

        void checkHistoryResults() {

            bool isOkay = true;
            for (const auto& it : oHistory) {
                try {
                    const HistoryVal& v = fHistory.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "History " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "History " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fHistory) {
                try {
                    const HistoryVal& v = oHistory.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "History " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "History " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "History table results are correct" << std::endl;
        }

        void checkItemResults() {

            bool isOkay = true;
            for (const auto& it : oItem) {
                try {
                    const ItemVal& v = fItem.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Item " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Item " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fItem) {
                try {
                    const ItemVal& v = oItem.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Item " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Item " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "Item table results are correct" << std::endl;
        }

        void checkNewOrderResults() {

            bool isOkay = true;
            for (const auto& it : oNewOrder) {
                try {
                    const NewOrderVal& v = fNewOrder.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "NewOrder " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "NewOrder " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fNewOrder) {
                try {
                    const NewOrderVal& v = oNewOrder.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "NewOrder " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "NewOrder " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "NewOrder table results are correct" << std::endl;
        }

        void checkOrderLineResults() {

            bool isOkay = true;
            for (const auto& it : oOrderLine) {
                try {
                    const OrderLineVal& v = fOrderLine.at(it.first);
                    if (!OLVequals(v, it.second)) {
                        isOkay = false;
                        std::cerr << "OrderLine " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "OrderLine " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fOrderLine) {
                try {
                    const OrderLineVal& v = oOrderLine.at(it.first);
                    if (!OLVequals(v, it.second)) {
                        isOkay = false;
                        std::cerr << "OrderLine " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "OrderLine " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "OrderLine table results are correct" << std::endl;
        }

        void checkOrderResults() {

            bool isOkay = true;
            for (const auto& it : oOrder) {
                try {
                    const OrderVal& v = fOrder.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Order " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Order " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fOrder) {
                try {
                    const OrderVal& v = oOrder.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Order " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Order " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "Order table results are correct" << std::endl;
        }

        void checkStockResults() {

            bool isOkay = true;
            for (const auto& it : oStock) {
                try {
                    const StockVal& v = fStock.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Stock " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Stock " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            for (const auto& it : fStock) {
                try {
                    const StockVal& v = oStock.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Stock " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Stock " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    return;
                }
            }
            if (isOkay)
                std::cout << "Stock table results are correct" << std::endl;
        }

        void checkWarehouseResults() {

            bool isOkay = true;
            for (const auto& it : oWarehouse) {
                try {
                    const WarehouseVal& v = fWarehouse.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Warehouse " << it.first << "contains " << v << " which should be " << it.second << std::endl;
                        //                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Warehouse " << it.first << " not present in table" << std::endl;
                    isOkay = false;
                    //                    return;
                }
            }
            for (const auto& it : fWarehouse) {
                try {
                    const WarehouseVal& v = oWarehouse.at(it.first);
                    if (!(v == it.second)) {
                        isOkay = false;
                        std::cerr << "Warehouse " << it.first << "contains " << it.second << " which should be " << v << std::endl;
                        //                        return;
                    }
                } catch (const std::exception &ex) {
                    std::cerr << "Warehouse " << it.first << " is extra in table" << std::endl;
                    isOkay = false;
                    //                    return;
                }
            }
            if (isOkay)
                std::cout << "Warehouse table results are correct" << std::endl;
        }
    };
}

#endif /* TPCCPARSER_H */

