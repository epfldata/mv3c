#include <chrono>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <locale>

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds milliseconds;

#define OPERATIONS 2500000
#define REPEAT 5

template<int Threads>
void bench_lock(){
    std::mutex mutex;
    unsigned long throughput = 0;

    for(int i = 0; i < REPEAT; ++i){
        std::vector<std::thread> threads;
        int counter = 0;

        Clock::time_point t0 = Clock::now();

        for(int i = 0; i < Threads; ++i){
            threads.push_back(std::thread([&](){
                for(int i = 0; i < OPERATIONS; ++i){
                    mutex.lock();
                    ++counter;
                    mutex.unlock();
                }
            }));
        }

        for(auto& thread : threads){
            thread.join();
        }

        Clock::time_point t1 = Clock::now();

        milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
        throughput += (Threads * OPERATIONS) / ms.count();
    }

    std::cout << "lock with " << Threads << " threads throughput = " << (throughput / REPEAT) << std::endl;
}

template<int Threads>
void bench_lock_guard(){
    std::mutex mutex;
    unsigned long throughput = 0;

    for(int i = 0; i < REPEAT; ++i){
        int counter = 0;

        std::vector<std::thread> threads;

        Clock::time_point t0 = Clock::now();

        for(int i = 0; i < Threads; ++i){
            threads.push_back(std::thread([&](){
                for(int i = 0; i < OPERATIONS; ++i){
                    std::lock_guard<std::mutex> guard(mutex);
                    ++counter;
                }
            }));
        }

        for(auto& thread : threads){
            thread.join();
        }

        Clock::time_point t1 = Clock::now();

        milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
        throughput += (Threads * OPERATIONS) / ms.count();
    }

    std::cout << "lock_guard with " << Threads << " threads throughput = " << (throughput / REPEAT) << std::endl;
}

template<int Threads>
void bench_atomic(){
    double throughput = 0.0;

    for(int i = 0; i < REPEAT; ++i){
        std::atomic<int> counter;
        counter.store(0);

        std::vector<std::thread> threads;

        Clock::time_point t0 = Clock::now();

        for(int i = 0; i < Threads; ++i){
            threads.push_back(std::thread([&](){
                for(int i = 0; i < OPERATIONS; ++i){
                    ++counter;
                }
            }));
        }

        for(auto& thread : threads){
            thread.join();
        }

        Clock::time_point t1 = Clock::now();

        milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
        throughput += ((Threads * OPERATIONS) * 1000.0) / ms.count();
    }
    // std::locale loc(""); // use default locale
    // std::cout.imbue( loc );
    std::cout.precision(0);
    std::cout << "atomic with " << Threads << " threads throughput = " << std::fixed << (throughput / REPEAT) << std::endl;
}

#define bench(name)\
    name<1>();\
    name<2>();\
    name<4>();\
    name<8>();\
    name<12>();\
    name<16>();\
    name<24>();\
    name<32>();\
    name<64>();\
    name<128>();

int main(){
    // bench(bench_lock);
    // bench(bench_lock_guard);
    // bench(bench_atomic);

    std::cout << sizeof(std::atomic<std::mutex*>) << std::endl;

    return 0;
}