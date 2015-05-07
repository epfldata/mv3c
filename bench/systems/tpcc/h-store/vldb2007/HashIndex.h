#ifndef _HASHINDEX_H_
#define _HASHINDEX_H_

#include <iterator>
#include <cassert>
#include <map>
#include <vector>
#include <cstring>

#include "HashFunction.h"

using namespace std;

class HashIndex {
 public:
  HashIndex(unsigned int numBuckets_, unsigned int dataCapacity_ = 100000);
  ~HashIndex();
  void init(unsigned int numBuckets_, unsigned int dataCapacity_ = 100000);

  // set max memory capacity
  //void setDataCapacity(unsigned int dataCapacity_);

  bool isEmpty();

  // memory capacity in bytes
  unsigned int getDataCapacity();
  // memory footprint in bytes
  unsigned int getDataSize();

  // number of hash buckets
  unsigned int getNumBuckets();
  // number of unique keys stored
  unsigned int getNumKeys();

  void put(byte* key_, int keysize_, void *value_, unsigned int size_);
  //void put(ValPos* key_, void *value_, unsigned int size_);
  //void put(int key_, void *value_, unsigned int size_);
  void* get(byte* key_, int keysize_);
  //void* get(ValPos* key_);
  //void* get(int key_);
  //void* remove(int key_);
  //void* remove(ValPos* key_);
  void* remove(byte* key_, int keysize_);
  void* removeTuple(byte* key_, int keysize_, void* tuple);
  void* getAgain();

  void setHashFunction(HashFunction *hashFunc);

  //ValPos* getKey();

 private:

  struct StringObj {
    StringObj() {
	    _count=0;
    }
    StringObj(byte* key_, int keysize_, void *value_, unsigned int size_) : _key(key_), _keysize(keysize_), _value(value_), _size(size_) {
    	_next=NULL;
    }
    byte* _key;
    int _keysize;
    void *_value;
    unsigned int _size;
    StringObj* _next;
    unsigned int _count;
  };

  struct IntObj {
    IntObj() {
	    _count=0;
    }
    IntObj(int key_, void *value_, unsigned int size_) : _key(key_), _value(value_), _size(size_) {
    	_next=NULL;
    }
	int _key;
    void *_value;
    unsigned int _size;
    IntObj* _next;
    unsigned int _count;
  };

  //void** _buckets;
  StringObj* _buckets;
  IntObj* _intbuckets;
  //int* bucketSize;

  unsigned int _minNonEmptyBucket;
  
  unsigned int _numBuckets;
  unsigned int _numKeys;
  unsigned int _dataCapacity;
  unsigned int _currDataSize;

  HashFunction* _hashFunc;
  bool useIntObjs;
  byte* playstring;
  StringObj* currBucket;
  byte* currkey;
  unsigned int currkeysize;

};

#endif
