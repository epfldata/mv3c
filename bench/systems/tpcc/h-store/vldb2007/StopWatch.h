#ifndef _STOPWATCH_H_
#define _STOPWATCH_H_

#include <ctime>
#include <stdio.h>
#include <iostream>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#ifndef HZ
    #define HZ 100
#endif

using namespace std;


class StopWatch
{
public:
	StopWatch(); //start counting time
	~StopWatch();
	// starts counting 
	void start();
	// returns number of milliseconds since start
	double stop();
	double stopToFile(const char* x);

	double quietStop();

	static unsigned long ticks();
	static double secondsPerTick();

	void lapStart();
	void lapStop();
private:
 	clock_t watch;           
	struct tms t,u;
	long r1,r2;
	double totalReal;
};

inline unsigned long StopWatch::ticks()
{
  unsigned int low, high;
  asm volatile ("rdtsc" : "=a" (low), "=d" (high));

  return (high<<11) + (low>>21);
}

#endif //_STOPWATCH_H_
