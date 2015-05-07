#include "HashIndex.h"
#include <iostream>

HashIndex::HashIndex(unsigned int numBuckets_, unsigned int dataCapacity_) {
  init(numBuckets_, dataCapacity_);
}

HashIndex::~HashIndex() {
  if (!_buckets)
    delete[] _intbuckets;
  else
    delete[] _buckets;
  //delete playvp;
}

void HashIndex::init(unsigned int numBuckets_, unsigned int dataCapacity_) {
  _numBuckets = numBuckets_;
  _dataCapacity = dataCapacity_;
  _numKeys = 0;
  _currDataSize = 0;
  _minNonEmptyBucket=numBuckets_;
//	_buckets=new void*[numBuckets_];
//	memset(_buckets,0,numBuckets_*sizeof(void*));
  _buckets = new StringObj[numBuckets_];
  _intbuckets = NULL;
  playstring=NULL;
  useIntObjs = false;
  _hashFunc = NULL;
  currBucket = NULL;
  currkey=NULL;
  //bucketSize=new int[numBuckets_];
  //for (unsigned int i=0; i<numBuckets_; i++) {
  //  bucketSize[i]=0;
  //}
}

bool HashIndex::isEmpty() {
  return _numKeys == 0;
}

void HashIndex::setHashFunction(HashFunction* hashFunc) {
  _hashFunc = hashFunc;
}

unsigned int HashIndex::getDataCapacity() {
  return _dataCapacity;
}

unsigned int HashIndex::getDataSize() {
  return _currDataSize;
}

unsigned int HashIndex::getNumBuckets() {
  return _numBuckets;
}

unsigned int HashIndex::getNumKeys() {
  return _numKeys;
}

void HashIndex::put(byte* key_, int keysize_, void *value_, unsigned int size_) {
  _currDataSize += size_;
  unsigned int hashValue;
  int keysize = keysize_;
  hashValue = ((*_hashFunc)(key_, keysize_)) % _numBuckets;
  
  if (_minNonEmptyBucket>hashValue) _minNonEmptyBucket=hashValue;
  
  StringObj& obj=_buckets[hashValue];
  int size=obj._count;
  _numKeys++;
  if (size==0) {
    obj._count=1;
    obj._value=value_;
    obj._size=size_;
    obj._key=key_;
    obj._keysize=keysize;
    obj._next=NULL;
  }
  else {
    StringObj* bucketPtr=obj._next;
    obj._next = new StringObj(key_, keysize, value_, size_);
    obj._count++;
    obj._next->_next = bucketPtr;

    //while (bucketPtr->_next)
    //  bucketPtr=bucketPtr->_next;

    //if ((obj._keysize==keysize) && (!memcmp(obj._key,key_,keysize)))
    //if (*obj._key==key_) {
    //  _currDataSize-=obj._size;
    //  obj._value=value_;
    //  return;
    //}

    /*
    Obj* bucketPtr=obj._next;
    if (bucketPtr==NULL) {
      obj._next=new Obj(key_->clone(), value_, size_);
      obj._count++;
      _numKeys++;
    }
    else {
      for (int i=1; i<size && !found ; i++) {
	if (*(bucketPtr->_key)==key_) {
	  _currDataSize-=bucketPtr->_size;
	  bucketPtr->_value=value_;
	  return;
	}
	if (i!=size-1) bucketPtr=bucketPtr->_next;
      }
      if (!found) {
	bucketPtr->_next=new Obj(key_->clone(), value_, size_);
	obj._count++;
	_numKeys++;
      }
      }*/

    //bucketPtr->_next=new StringObj(key_, keysize, value_, size_);
    //obj._count++;
  }
}

/*
void HashIndex::put(int key_, void *value_, unsigned int size_) {
  _currDataSize += size_;
  if (!useIntObjs) {
    useIntObjs = true;
    delete[] _buckets;
    _buckets=NULL;  
    _intbuckets = new IntObj[_minNonEmptyBucket];
  }
  unsigned int hashValue = (*_hashFunc)(key_);
  if (_minNonEmptyBucket>hashValue) _minNonEmptyBucket=hashValue;
  
  IntObj& obj=_intbuckets[hashValue];
  int size=obj._count;
  // Let's see if key exists
  if (size==0) {
    _numKeys++;
    obj._count=1;
    obj._value=value_;
    obj._size=size_;
    obj._key=key_;
    obj._next=NULL;
  }
  else {
    bool found=false;
    if (obj._key==key_) {
      _currDataSize-=obj._size;
      obj._value=value_;
      return;
    }
    
    IntObj* bucketPtr=obj._next;
    if (bucketPtr==NULL) {
      obj._next=new IntObj(key_, value_, size_);
      obj._count++;
      _numKeys++;
    }
    else {
      for (int i=1; i<size && !found ; i++) {
	if (bucketPtr->_key==key_) {
	  _currDataSize-=bucketPtr->_size;
	  bucketPtr->_value=value_;
	  return;
	}
	if (i!=size-1) bucketPtr=bucketPtr->_next;
      }
      if (!found) {
	bucketPtr->_next=new IntObj(key_, value_, size_);
	obj._count++;
	_numKeys++;
      }
    }
  }
}
*/

void* HashIndex::remove(byte* key_, int keysize_) {
	unsigned int hashValue;
	int keysize = keysize_;
	hashValue = ((*_hashFunc)(key_, keysize_)) % _numBuckets;
	StringObj& obj=_buckets[hashValue];
	int size=obj._count;
  	// most often the bucket will be of size 1, so we optimize for this case
  	// plus this is a quick check
  	if (size==0) return NULL; 
  	if (size==1) {
	  if(memcmp(key_,obj._key,keysize)) {
	    //currBucket=NULL;
	    //currkey=NULL;
	    return NULL;
	  }
	  _numKeys--;
	  obj._count=0;
	  void* temp = obj._value;
	  obj._value=NULL;
	  obj._size=0;
	  obj._key=NULL;
	  obj._keysize=0;
	  obj._next=NULL;
  	  return temp;
  	}
  	else {
	  if(!memcmp(key_,obj._key,keysize)) {
	    //currBucket=obj._next;
	    //currkey = key_;
	    //currkeysize = keysize_;
	    assert(obj._next);
	    _numKeys--;
	    obj._count--;
	    void* temp = obj._value;
	    obj._value=obj._next->_value;
	    obj._size=obj._next->_size;
	    obj._key=obj._next->_key;
	    obj._keysize=obj._next->_keysize;
	    obj._next=obj._next->_next;
	    return temp;
	  }
	  StringObj* nxtObj = obj._next;
	  StringObj* currObj = &obj;
	  //bool found=false;
	  for (int i=1; i<size /*&& !found*/ ; i++) {
	    if(!memcmp(key_,nxtObj->_key,keysize)) {
	      //currBucket=nxtObj->_next;
	      //currkey = key_;
	      //currkeysize = keysize_;
	      _numKeys--;
	      obj._count--;
	      void* temp = nxtObj->_value;
	      currObj->_next = nxtObj->_next;
	      delete nxtObj;
	      return temp;
	    }
	    currObj=nxtObj;
	    nxtObj=nxtObj->_next;
	  }
	  //currBucket=NULL;
	  //currkey = NULL;
	  return NULL;
	}   	
}

void* HashIndex::removeTuple(byte* key_, int keysize_, void* tuple) {
	unsigned int hashValue;
	int keysize = keysize_;
	hashValue = ((*_hashFunc)(key_, keysize_)) % _numBuckets;
	StringObj& obj=_buckets[hashValue];
	int size=obj._count;
  	// most often the bucket will be of size 1, so we optimize for this case
  	// plus this is a quick check
  	if (size==0) return NULL; 
  	if (size==1) {
	  if((memcmp(key_,obj._key,keysize)) || (obj._value != tuple)) {
	    //currBucket=NULL;
	    //currkey=NULL;
	    return NULL;
	  }
	  _numKeys--;
	  obj._count=0;
	  void* temp = obj._value;
	  obj._value=NULL;
	  obj._size=0;
	  obj._key=NULL;
	  obj._keysize=0;
	  obj._next=NULL;
  	  return temp;
  	}
  	else {
	  if((!memcmp(key_,obj._key,keysize)) && (obj._value == tuple)) {
	    //currBucket=obj._next;
	    //currkey = key_;
	    //currkeysize = keysize_;
	    assert(obj._next);
	    _numKeys--;
	    obj._count--;
	    void* temp = obj._value;
	    obj._value=obj._next->_value;
	    obj._size=obj._next->_size;
	    obj._key=obj._next->_key;
	    obj._keysize=obj._next->_keysize;
	    obj._next=obj._next->_next;
	    return temp;
	  }
	  StringObj* nxtObj = obj._next;
	  StringObj* currObj = &obj;
	  //bool found=false;
	  for (int i=1; i<size /*&& !found*/ ; i++) {
	    if((!memcmp(key_,nxtObj->_key,keysize)) && (nxtObj->_value == tuple)) {
	      //currBucket=nxtObj->_next;
	      //currkey = key_;
	      //currkeysize = keysize_;
	      _numKeys--;
	      obj._count--;
	      void* temp = nxtObj->_value;
	      currObj->_next = nxtObj->_next;
	      delete nxtObj;
	      return temp;
	    }
	    currObj=nxtObj;
	    nxtObj=nxtObj->_next;
	  }
	  //currBucket=NULL;
	  //currkey = NULL;
	  return NULL;
	}   	
}

void* HashIndex::get(byte* key_, int keysize_) {
	unsigned int hashValue;
	int keysize = keysize_;
	hashValue = ((*_hashFunc)(key_, keysize_)) % _numBuckets;
	StringObj& obj=_buckets[hashValue];
	int size=obj._count;
  	// most often the bucket will be of size 1, so we optimize for this case
  	// plus this is a quick check
  	if (size==0) return NULL; 
  	if (size==1) {
	  if(memcmp(key_,obj._key,keysize)) {
	    currBucket=NULL;
	    currkey=NULL;
	    return NULL;
	  }
  	  return obj._value;
  	}
  	else {
	  if(!memcmp(key_,obj._key,keysize)) {
	    currBucket=obj._next;
	    currkey = key_;
	    currkeysize = keysize_;
	    return obj._value;
	  }
	  StringObj* nxtObj = obj._next;
	  //bool found=false;
	  for (int i=1; i<size /*&& !found*/ ; i++) {
	    if(!memcmp(key_,nxtObj->_key,keysize)) {
	      currBucket=nxtObj->_next;
	      currkey = key_;
	      currkeysize = keysize_;
	      return nxtObj->_value;
	    }
	    nxtObj=nxtObj->_next;
	  }
	  currBucket=NULL;
	  currkey = NULL;
	  return NULL;
	}   
	
}

void* HashIndex::getAgain() {
  if (!currBucket)
    return NULL;
  assert(currkey);
  StringObj* obj = currBucket;
  if(!memcmp(currkey,obj->_key,currkeysize)) {
    currBucket=obj->_next;
    return obj->_value;
  }
  obj = obj->_next;
  //bool found=false;
  while (obj) {
    if(!memcmp(currkey,obj->_key,currkeysize)) {
      currBucket=obj->_next;
      return obj->_value;
    }
    obj=obj->_next;
  }
  currBucket=NULL;
  currkey = NULL;
  return NULL;
}

/*
void* HashIndex::get(ValPos* key_) {
	unsigned int hashValue;
	int keysize = key_->getSize();
	if (keysize == 4)
	  hashValue = (*_hashFunc)(*(int*)key_->value);//even though it might not be an int
	else
	  hashValue = ((*_hashFunc)((unsigned char*)key_->value)) % _numBuckets;
  	//int hashValue = (*_hashFunc)(key_);

	Obj& obj=_buckets[hashValue];
	int size=obj._count;
  	// most often the bucket will be of size 1, so we optimize for this case
  	// plus this is a quick check
  	if (size==0) return NULL; 
  	if (size==1) {
	  if(*key_ != obj._key) {
	    return NULL;
	  }
  	  return obj._value;
  	}
  	else {
	  if (*(obj._key) == key_) {
	    return obj._value;
	  }
    	Obj* nxtObj = obj._next;
	  	bool found=false;
		for (int i=1; i<size && !found ; i++) {
			if (*nxtObj->_key==key_) {
			  return nxtObj->_value;
			}
			if (i!=size-1) nxtObj=nxtObj->_next;
		}
    	return NULL;
  }   

}

void* HashIndex::get(int key_) {
  if (!_intbuckets)
    return NULL;
  unsigned int hashValue = (*_hashFunc)(key_);
  IntObj& obj=_intbuckets[hashValue];
  int size=obj._count;
  // most often the bucket will be of size 1, so we optimize for this case
  // plus this is a quick check
  if (size==0) return NULL; 
  if (size==1) {
    if(key_ != obj._key) {
      return NULL;
    }
    return obj._value;
  }
  else {
    if ((obj._key) == key_) {
      return obj._value;
    }
    IntObj* nxtObj = obj._next;
    bool found=false;
    for (int i=1; i<size && !found ; i++) {
      if (nxtObj->_key==key_) {
	return nxtObj->_value;
      }
      if (i!=size-1) nxtObj=nxtObj->_next;
    }
    return NULL;
  } 
}

void* HashIndex::remove(ValPos* key_) {
	unsigned int hashValue;
	int keysize = key_->getSize();
	if (keysize == 4)
	  hashValue = (*_hashFunc)(*(int*)key_->value);//even though it might not be an int
	else
	  hashValue = ((*_hashFunc)((unsigned char*)key_->value)) % _numBuckets;
  	//int hashValue = (*_hashFunc)(key_);
	Obj& obj=_buckets[hashValue];
    int size=obj._count;
    
	if (size==0) return NULL;
  	if (size==1) {
		void* value = obj._value;
		obj._count=0;
		_numKeys--;
		delete obj._key;
  	  	return value;
  	}
  	else if (*_buckets[hashValue]._key==key_) {
  		void* val=obj._value;
 		Obj* nxtObj = obj._next;
 		
 		obj._value=nxtObj->_value;
		delete obj._key;
		obj._key=nxtObj->_key;
		obj._size=nxtObj->_size;
		obj._next=nxtObj->_next;
		
		obj._count--;
		_numKeys--;
 		delete nxtObj;
 		return val;
  	}
  	else {
    	Obj* currObj = _buckets[hashValue]._next;
	  	Obj* prevObj=NULL;
		for (int i=1; i<size; i++) {
			if (*currObj->_key==key_) {
				void* value = currObj->_value;
				if (prevObj==NULL) 
					obj._next=currObj->_next;
				else
					prevObj->_next=currObj->_next;
				obj._count--;
				
				delete currObj->_key;
				currObj->_key=NULL;
				delete ((Obj*) currObj);
				_numKeys--;
		  	  	return value;
			}
			prevObj=currObj;
			if (i!=size-1) currObj=currObj->_next;
		}
    	return NULL;
  }   
}

void* HashIndex::remove(int key_) {
  unsigned int hashValue= (*_hashFunc)(key_);
  IntObj& obj=_intbuckets[hashValue];
  int size=obj._count;
  
  if (size==0) return NULL;
  if (size==1) {
    void* value = obj._value;
    obj._count=0;
    _numKeys--;
    return value;
  }
  else if (_intbuckets[hashValue]._key==key_) {
    void* val=obj._value;
    IntObj* nxtObj = obj._next;
    
    obj._value=nxtObj->_value;
    obj._key=nxtObj->_key;
    obj._size=nxtObj->_size;
    obj._next=nxtObj->_next;
    
    obj._count--;
    _numKeys--;
    delete nxtObj;
    return val;
  }
  else {
    IntObj* currObj = _intbuckets[hashValue]._next;
    IntObj* prevObj=NULL;
    for (int i=1; i<size; i++) {
      if (currObj->_key==key_) {
	void* value = currObj->_value;
	if (prevObj==NULL) 
	  obj._next=currObj->_next;
	else
	  prevObj->_next=currObj->_next;
	obj._count--;
	delete ((Obj*) currObj);
	_numKeys--;
	return value;
      }
      prevObj=currObj;
      if (i!=size-1) currObj=currObj->_next;
    }
    return NULL;
  }   
}


ValPos* HashIndex::getKey() {
  if (!useIntObjs) {
    while (_minNonEmptyBucket<_numBuckets) {
      if (_buckets[_minNonEmptyBucket]._count==0)
	_minNonEmptyBucket++;
      else {
	return _buckets[_minNonEmptyBucket]._key;
      }
    }
    throw new UnexpectedException("Error: there are no keys");
    
    return NULL;
  }
  else{
    while (_minNonEmptyBucket<_numBuckets) {
      if (_intbuckets[_minNonEmptyBucket]._count==0)
	_minNonEmptyBucket++;
      else {
	playvp->set((byte*)&(_intbuckets[_minNonEmptyBucket]._key));
	return playvp;
      }
    }
    throw new UnexpectedException("Error: there are no keys");
    
    return NULL;
  }
}
*/
