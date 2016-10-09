#ifndef TIMER_H
#define TIMER_H
#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::nanoseconds;
typedef system_clock::time_point Timepoint;
#define Now high_resolution_clock::now()
#define DurationMS(x) std::chrono::duration_cast<milliseconds>(x).count()
#define DurationNS(x) std::chrono::duration_cast<nanoseconds>(x).count()

#endif /* TIMER_H */

