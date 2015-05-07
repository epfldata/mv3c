#ifndef _TUPLEBUFFER_H_
#define _TUPLEBUFFER_H_

#include <ctime>
#include <stdio.h>
#include <iostream>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/types.h>
#include <string.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "Constants.h"

using namespace std;

struct TBList { byte* buffer; TBList* next; };

class TupleBuffer
{

 public:

  TupleBuffer(int numBytes_, int id_); //start counting time
  ~TupleBuffer();
  //byte* addTuple(byte* tuple, int tupleSize);

  inline byte* allocateTuple(int tupleSize) {
    if (currPos+tupleSize > endPos) {
      // Buffer is full, stash is and use a fresh buffer
      TBList* x=new TBList(); x->buffer=buffer; x->next=full; full=x;
      buffer = new byte[numBytes];
      currPos = buffer;
      endPos = buffer+numBytes;
      /*
        printf("ERROR: No space left in buffer with id %d.\n", id);
        exit(1);
      */
    }
    byte* toReturn = currPos;
    currPos+=tupleSize;
    return toReturn;
  }

 private:
  TBList* full;
  byte* buffer;
  int numBytes;
  byte* endPos;
  byte* currPos;
  int id;

};

#endif //_TUPLEBUFFER_H_
