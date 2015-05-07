#ifndef __UTIL_H
#define __UTIL_H

#include <map>
#include <vector>


// define following if you want PAPI support
//#define USE_PAPI

#ifdef USE_PAPI
#include "papi.h"
#endif

class util {
public:
  util();
  virtual ~util();
#ifdef USE_PAPI
  //+ PAPI
  int init_PAPI();
  void setup_PAPI_event();
  void start_PAPI_event();
  void read_PAPI_event();
  void print_PAPI_event();
  long_long get_usec();
  long_long get_cycle();
  //-
#endif
//private:
#ifdef USE_PAPI
  unsigned int* eventList;
  int eventSet;
  std::map<unsigned int, long_long> eventInfo;
#endif
};

#endif // __UTIL_H
