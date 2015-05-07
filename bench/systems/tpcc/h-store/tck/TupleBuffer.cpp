#include "TupleBuffer.h"

TupleBuffer::TupleBuffer(int numBytes_, int id_)
{
  full = NULL;
  buffer = new byte[numBytes_];
  currPos = buffer;
  numBytes = numBytes_;
  endPos = buffer+numBytes_;
  id = id_;
}

TupleBuffer::~TupleBuffer()
{
  delete[] buffer;
  TBList* x=full;
  while (x!=NULL) { free(x->buffer); TBList* n=x->next; free(x); x=n; }
}

/*
byte* TupleBuffer::addTuple(byte* tuple, int tupleSize) {
  byte* toReturn = currPos;
  if (currPos+tupleSize > endPos)
    return NULL;
  memcpy(currPos,tuple,tupleSize);
  currPos+=tupleSize;
  return toReturn;
}
*/
