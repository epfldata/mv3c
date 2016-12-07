#ifndef TIMER_H
#define TIMER_H
#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::nanoseconds;
typedef system_clock::time_point Timepoint;
#ifdef DTIMER
#define DNow high_resolution_clock::now()
#define DDurationMS(x) std::chrono::duration_cast<milliseconds>(x).count()
#define DDurationUS(x) std::chrono::duration_cast<microseconds>(x).count()
#define DDurationNS(x) std::chrono::duration_cast<nanoseconds>(x).count()
#else
#define DNow 0
#define DDurationMS(x) 0
#define DDurationUS(x) 0
#define DDurationNS(x) 0
#endif

#define Now high_resolution_clock::now()
#define DurationMS(x) std::chrono::duration_cast<milliseconds>(x).count()
#define DurationUS(x) std::chrono::duration_cast<microseconds>(x).count()
#define DurationNS(x) std::chrono::duration_cast<nanoseconds>(x).count()

#endif /* TIMER_H */

