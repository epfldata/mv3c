
#ifndef TRADING_H
#define TRADING_H
#include <cinttypes>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <cassert>
#include "Tuple.h"
#include <iomanip>
#include "CustomString.h"
#include <random>
#include "types.h"

#include "cryptlib.h"
using CryptoPP::Exception;

#include "skipjack.h"
using CryptoPP::SKIPJACK;
#define KEY_SIZE 10
#define BLOCK_SIZE 8
#include "aes.h"
using CryptoPP::AES;

#include "modes.h"
using CryptoPP::ECB_Mode;
namespace trading_ns {


#ifdef NUMPROG
    const uint32_t numPrograms = NUMPROG;
#else
    const uint32_t numPrograms = 100;
#endif

    const uint numThreads = 10;

    ECB_Mode< SKIPJACK>::Encryption encryptors[numThreads + 1];
    ECB_Mode< SKIPJACK>::Decryption decryptors[numThreads + 1];
#ifndef POWER
#define POWER 1.1
#endif
    const size_t SecuritySize = 100000;
    const size_t TradeSize = numPrograms;
    const size_t TradeLineSize = TradeSize * 8;
    const size_t CustomerSize = 100000;

    const size_t SecurityEntrySize = SecuritySize;
    const size_t SecurityDVSize = 2 * numPrograms / numThreads;
    const size_t TradeEntrySize = numPrograms;
    const size_t TradeDVSize = numPrograms;
    const size_t TradeLineEntrySize = TradeLineSize;
    const size_t TradeLineDVSize = TradeLineSize;
    const size_t CustomerEntrySize = CustomerSize;
    const size_t CustomerDVSize = CustomerSize;

#define Map(x) std::unordered_map<x##Key, x##Val, CityHasher<x##Key>>


    //------------------------------------------------------------------------------
    typedef uint32_t datetime_t;

    struct TradeRequest {
        uint32_t t_id;
        datetime_t datetime;
        uint32_t sec_ids[8];
        uint8_t numSecs;
        char pad[7];

        friend std::ostream& operator<<(std::ostream& s, TradeRequest const& obj) {
            s << obj.t_id << " " << obj.datetime << " " << obj.numSecs;
            for (int i = 0; i < obj.numSecs; i++)
                s << " " << obj.sec_ids[i];
            return s;
        }

    };

    struct TradeValue {
        datetime_t datetime;
        char pad[4];

    };

    struct SecIDPrice {
        uint32_t sec_id;
        double price;
        char pad[4];


    };

    typedef KeyTuple<uint32_t> SecurityKey;
    typedef ValTuple<double, String < 3 >> SecurityVal;

    typedef KeyTuple<uint32_t> TradeKey;
    typedef ValTuple<uint32_t, String <sizeof (TradeValue) >> TradeVal;

    typedef KeyTuple<uint32_t, uint8_t> TradeLineKey;
    typedef ValTuple<String<sizeof (SecIDPrice) >> TradeLineVal;

    typedef KeyTuple<uint32_t> CustomerKey;
    typedef String<KEY_SIZE> mykey_t;
    typedef ValTuple<mykey_t> CustomerVal;

    //-----------------------------------------

    enum TRADING_PROGRAMS {
        TRADE_ORDER, PRICE_UPDATE
    };

    void printByteArray(const char * a, int s) {
        for (int i = 0; i < s; ++i) {
            std::cout << hex << (uint8_t) a[i] << " ";
        }
    }

    struct TradeOrder : public Program {
        uint32_t c_id;
        char request[sizeof (TradeRequest)];

        TradeOrder() : Program(TRADE_ORDER) {
        }

        TradeOrder(const Program *p) : Program(p) {
            assert(p->prgType == TRADE_ORDER);
            auto prg = (TradeOrder*) p;
            c_id = prg->c_id;
            memcpy(request, prg->request, sizeof (TradeRequest));
        }
    };

    struct PriceUpdate : public Program {
        uint32_t sec_id;
        double price;

        PriceUpdate() : Program(PRICE_UPDATE) {
        }

        PriceUpdate(const Program *p) : Program(p) {
            assert(p->prgType == PRICE_UPDATE);
            auto prg = (PriceUpdate*) p;
            sec_id = prg->sec_id;
            price = prg->price;
        }
    };
    //---------------------------------------------------------------------------

    void encrypt(char* out, char* in, const mykey_t key, uint numBytes, uint8_t tid) {
        encryptors[tid].SetKey((const byte*) key.data, KEY_SIZE);
        encryptors[tid].ProcessData((byte*) out, (byte*) in, numBytes);
    }

    void decrypt(char* out, char* in, const mykey_t key, uint numBytes, uint8_t tid) {
        decryptors[tid].SetKey((const byte*) key.data, KEY_SIZE);
        decryptors[tid].ProcessData((byte*) out, (byte*) in, numBytes);
    }
    //---------------------------------------------------------------------------

    struct TradingDataGen {
        Program** programs;
        Map(Customer) iCustomer;
        Map(Security) iSecurity;

        TradingDataGen() {
            programs = new Program*[numPrograms];
        }

        ~TradingDataGen() {
            for (size_t i = 0; i < numPrograms; i++)
                delete programs[i];
            delete[] programs;
        }

        uint32_t getNextSec(std::default_random_engine& rng) {
            static double cdf[SecuritySize];
            static std::uniform_real_distribution<double> U(0.0, 1.0);
            const double alpha = POWER;
            static bool firsttime = true;
            if (firsttime) {
                double sum = 0;
                for (uint32_t i = 0; i < SecuritySize; ++i) {
                    sum += pow(i + 1, -alpha);
                    cdf[i] = sum;
                }
                for (uint32_t i = 0; i < SecuritySize; ++i) {
                    cdf[i] /= sum;
                }
                firsttime = false;
            }

            double p = U(rng);
            uint32_t i = 0;
            while (p > cdf[i])
                i++;
            return i;
        }


        //Call loadCustomers before this

        void loadPrograms() {
            //            srand(time(NULL));
            uint s1 = rand(), s2 = rand(), s3 = rand(), s4 = rand(), s5 = rand();
            std::default_random_engine g1(s1); //prgGen
            std::default_random_engine g2(s2); //secGen
            std::default_random_engine g3(s3); //secGen
            std::default_random_engine g4(s4); //numSecGen
            std::default_random_engine g5(s5); //custGen

            std::uniform_real_distribution<double> prgGen(0.0, 1.0);
            std::uniform_int_distribution<uint8_t> numSecGen(3, 8);
            std::uniform_int_distribution<uint32_t> custGen(0, CustomerSize - 1);
            uint32_t t_id = 0;
            uint32_t datetime = 31536000 * 30;
            for (uint32_t curPrg = 0; curPrg < numPrograms; curPrg++) {
                if (prgGen(g1) < 0.5) {
                    //Trade order
                    t_id++;
                    datetime += rand() % 200;
                    TradeRequest req;
                    req.datetime = datetime;
                    req.t_id = t_id;


                    req.numSecs = numSecGen(g4);
                    for (uint8_t i = 0; i < req.numSecs; ++i) {
                        auto s_id = getNextSec(g2);
                        uint8_t j = 0;
                        for (; j < i; ++j) {
                            if (s_id == req.sec_ids[j])
                                break;
                        }
                        if (j == i)
                            req.sec_ids[i] = s_id;
                        else {
                            i--;
                        }

                    }
                    TradeOrder* p = new TradeOrder();
                    p->c_id = custGen(g5);
                    encrypt(p->request, (char *) &req, iCustomer.at(p->c_id)._1, sizeof (req), 0);
#ifndef NDEBUG
//                    std::cout << curPrg << "    TradeOrder  " << p->c_id << "  " << req << std::endl;
//                    std::cout << "      Key = ";
//                    printByteArray(iCustomer.at(p->c_id)._1.c_str(), KEY_SIZE);
//                    std::cout << "\n      Encrypted = ";
//                    printByteArray(p->request, sizeof (TradeRequest));
//                    std::cout << dec << std::endl;
#endif
                    programs[curPrg] = p;
                } else {
                    //Update price
                    PriceUpdate *p = new PriceUpdate();
                    p->price = rand()*1000.0 / RAND_MAX;
                    //                    p->sec_id = 1;
                    p->sec_id = getNextSec(g3);
#ifndef NDEBUG
//                    std::cout << curPrg << "    PriceUpdate  " << p->sec_id << "  " << p->price << std::endl;
#endif
                    programs[curPrg] = p;
                }

            }

        }

        void loadCustomer() {
            mykey_t key;
            for (uint32_t i = 0; i < CustomerSize; ++i) {
                for (uint8_t j = 0; j < KEY_SIZE; ++j) {
                    key.data[j] = (uint8_t) rand();
                }
                iCustomer.insert({CustomerKey(i), CustomerVal(key)});
            }
        }

        void loadSecurity() {
            for (uint32_t i = 0; i < SecuritySize; ++i) {
                double price = rand()*1000.0 / RAND_MAX;
                String<3> symbol;
                for (uint j = 0; j < 3; j++)
                    symbol.data[j] = 'A' + rand() % 26;
                iSecurity.insert({SecurityKey(i), SecurityVal(price, symbol)});
            }
        }
    };
}

#endif 

