#ifndef SECONDARYINDEX_H
#define SECONDARYINDEX_H
#include "types.h"
template <typename K, typename V>
struct SecondaryIndex {
    //assumes value that is indexed doesnt change
    typedef Entry<K,V>* EntryPtrType;
    virtual void insert(EntryPtrType e, const V& val) = 0;
    
    ///Not required for TPCC as long as it is for Order and OrderLine
//    virtual void erase(EntryPtrType e, const V& val) = 0;     
};


#endif /* SECONDARYINDEX_H */

