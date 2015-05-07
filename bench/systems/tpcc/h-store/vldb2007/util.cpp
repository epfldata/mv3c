#include "util.h"
#include <string>
#include <stdlib.h>
#include <iostream>

using namespace std;

util::util()
{
#ifdef USE_PAPI
  eventList=NULL;
  eventSet=0;
#endif
}

util::~util()
{
#ifdef USE_PAPI
  if (eventList) {
    delete [] eventList;
    eventList = NULL;
  }
#endif
} 


#ifdef USE_PAPI
int util::init_PAPI()
{
  this->eventSet=PAPI_NULL;

  this->eventList = new unsigned int[9];
  this->eventList[0] = PAPI_TOT_CYC;
  this->eventList[1] = PAPI_TOT_INS;
  this->eventList[2] = PAPI_TLB_TL;
  this->eventList[3] = PAPI_BR_MSP;
  this->eventList[4] = PAPI_L1_DCM;
  this->eventList[5] = PAPI_L1_ICM;
  this->eventList[6] = PAPI_L2_TCM;
  this->eventList[7] = 0x40270001;
  this->eventList[8] = 0;

  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    cout << "*** PAPI_library_init failed! *** error:" << retval << endl;
    exit (1);
  }
  if ((retval = PAPI_create_eventset(&this->eventSet)) != PAPI_OK) {
    cout << "*** PAPI_create_eventset failed! *** error:" << retval << endl;
    exit (1);
  }
  return PAPI_OK;
}

void util::setup_PAPI_event()
{
  for (int i = 0; this->eventList[i] != 0; i++)
  {
    if (PAPI_query_event(this->eventList[i]) == PAPI_OK) {
      eventInfo[eventList[i]] = 0;
    }
  }
  /*
  int numCounters = 0;
  if ((numCounters = PAPI_get_opt(PAPI_MAX_HWCTRS,NULL)) > 0) {
    cout << "\t total " << numCounters << " counter(s) available!" << endl;
  }
  */
  char descr[PAPI_MAX_STR_LEN];
  std::map<unsigned int, long_long>::iterator iter;
  for (iter = this->eventInfo.begin(); iter != this->eventInfo.end(); ++iter) {
    PAPI_event_code_to_name((*iter).first, descr);
    PAPI_add_event(this->eventSet, (*iter).first);
    PAPI_event_code_to_name((*iter).first, descr);
    // cout << "added " << "event " << descr << " " << (*iter).first << endl;
  }
}

void util::start_PAPI_event()
{
  int retval = 0;
  if ( ( (retval = PAPI_start(this->eventSet) ) ) != PAPI_OK ) {
    cout << "*** PAPI_start failed: " << retval << endl;
  }
  if ( ( (retval = PAPI_reset(this->eventSet) ) ) != PAPI_OK ) {
    cout << "*** PAPI_reset failed: " << retval << endl;
  }
}

void util::read_PAPI_event()
{
  if (this->eventInfo.size() > 0) {
    int retval = 0;
    long_long* values = new long_long[eventInfo.size()];
    if (retval = PAPI_read(this->eventSet, values) != PAPI_OK) {
      cout << "*** PAPI_read failed! *** error:" << retval << endl;
      exit(1);
    }
    /*
    for (unsigned int k=0; k <eventInfo.size(); ++k) {
      cout << "=>" << values[k] << endl;
    }
    */
    std::map<unsigned int, long_long>::iterator iter;
    int i = 0;
    for (iter = eventInfo.begin(); iter != eventInfo.end(); ++iter) {
      (*iter).second = values[i];
      ++i;
    }
    delete [] values;
  } 
}

void util::print_PAPI_event()
{
  char descr[PAPI_MAX_STR_LEN];
  PAPI_event_info_t evinfo;

  cout << endl;
  std::map<unsigned int, long_long>::iterator iter;
  for (iter = eventInfo.begin(); iter != eventInfo.end(); ++iter) {
    PAPI_event_code_to_name((*iter).first, descr);
    if (PAPI_get_event_info((*iter).first, &evinfo) != PAPI_OK) {
      cout << "*** PAPI_get_event_info failed! *** error!" << endl;
      exit(1);
    }
    cout.setf(std::ios::left, std::ios::adjustfield);
    cout.width(15);
    cout << evinfo.symbol << ' ';
    cout.setf(std::ios::left, std::ios::adjustfield);
    cout.width(60);
    cout << evinfo.long_descr << ' ';
    cout.setf(std::ios::right, std::ios::adjustfield);
    cout.width(20);
    cout << (*iter).second << endl;
  }
}

long_long util::get_usec()
{
  return PAPI_get_real_usec();
}

long_long util::get_cycle()
{
  return PAPI_get_real_cyc();
}
#endif

