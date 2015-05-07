#ifndef HASH_FUNCTION_H
#define HASH_FUNCTION_H

#include "UnexpectedException.h"
#include "Constants.h"

class HashFunction {
 public:
  virtual ~HashFunction(){};
  virtual unsigned int operator()(unsigned int value) = 0;
  virtual unsigned int operator()(byte* value) = 0;
  virtual unsigned int operator()(byte* value, int strsize) = 0;

};

class IdentityHashFunction : public HashFunction {
 public:
  IdentityHashFunction(int max) {
    _max = max;
  }
  virtual ~IdentityHashFunction(){};
  virtual unsigned int inline operator()(unsigned int value) { return value % _max; }
  virtual unsigned int inline operator()(byte* value) { throw new UnexpectedException("invalid input to IdentityHashFunction ... only ints allowed"); }
  virtual unsigned int inline operator()(byte* value,int strsize) { throw new UnexpectedException("invalid input to IdentityHashFunction ... only ints allowed"); }

 private:
  int _max;
};

class StringHashFunction : public HashFunction {
 public:
  StringHashFunction(/*int strsize*/) {
    //_strsize = strsize;
  }
  virtual ~StringHashFunction(){};
  /*virtual unsigned int inline operator()(unsigned int value) { 
    byte* val = (byte*)(&value);
    unsigned long long hash = 0;
    byte c;
    int i=0;
    while (i<_strsize) {
      c = *val++;
      hash = c + (hash << 6) + (hash << 16) - hash;
      i++;
    }
    return hash;
    }*/
  virtual unsigned int inline operator()(unsigned int value) { throw new UnexpectedException("invalid input to StringHashFunction ... only byte* allowed"); }
    
  virtual unsigned int inline operator()(byte* value) {
    byte c = *value;
    unsigned long long hash = 0;
    while (c) {
      hash = c + (hash << 6) + (hash << 16) - hash;
      //int i=0;
      //while (i<_strsize) {
      c = *value++;
      //i++;
    }
    return (unsigned int)hash;
  }
  virtual unsigned int inline operator()(byte* value,int strsize) {
    unsigned long long hash = 0;
    byte c;
    int i=0;
    while (i<strsize) {
      c = *value++;
      hash = c + (hash << 6) + (hash << 16) - hash;
      i++;
    }
    return (unsigned int)hash;
  }

  //private:
  //int _strsize;
};

#endif
