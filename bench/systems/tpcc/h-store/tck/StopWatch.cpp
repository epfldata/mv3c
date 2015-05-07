#include "StopWatch.h"

StopWatch::StopWatch()
{
	totalReal=0;
}

StopWatch::~StopWatch()
{
	//cout << "Total Lapped time: " << totalReal << endl;
}

void StopWatch::start() {

       r1 = times(&t);
}

double StopWatch::stop() {
  //clock_t diff = clock()-watch;
           r2 = times(&u);
           printf("user time=%f\n",
                 ((float)(u.tms_utime-t.tms_utime))/(HZ));
           printf("system time=%f\n",
                 ((float)(u.tms_stime-t.tms_stime))/(HZ));
           printf("real time=%f\n",
                 ((float)(r2-r1))/(HZ));
	   //return (((double) diff)/((double) CLOCKS_PER_SEC))*1000;
	   return (((float)(r2-r1))/(HZ))*1000;
}

double StopWatch::stopToFile(const char* x) {
  ofstream flStream;
  flStream.open(x, ios::out | ios::app);
  flStream.seekp(ios::end);
  //clock_t diff = clock()-watch;
  r2 = times(&u);
  flStream << "user time=" <<
    ((float)(u.tms_utime-t.tms_utime))/(HZ) << std::endl;
  flStream << "system time=" <<
    ((float)(u.tms_stime-t.tms_stime))/(HZ) << std::endl;
  flStream << "real time=" <<
    ((float)(r2-r1))/(HZ) << std::endl;
  //return (((double) diff)/((double) CLOCKS_PER_SEC))*1000;
  return (((float)(r2-r1))/(HZ))*1000;
}

double StopWatch::secondsPerTick()
{
  // constant factor, measured on sorrel. Should be measured at run time
  // by sleeping for some time.
  return (1/0.820578)*((1<<21) / 3.9e9);
}

double StopWatch::quietStop()
{
           r2 = times(&u);
	   // user time!
	   return  ((float)(u.tms_utime-t.tms_utime))/(HZ);
//(((float)(r2-r1))/(HZ))*1000;
}

void StopWatch::lapStart() {
	start();
}
void StopWatch::lapStop() {
	totalReal+=stop();
}
