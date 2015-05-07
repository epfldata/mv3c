#ifndef __Set_H__
#define __Set_H__
#include <stdlib.h>
#include <stdio.h>

// Set backed by an un-rebalanced tree stored in an array
template <typename T>
class Set {
private:
  struct N { T v; int l,r; }; // -1=null
  N* data;
  int capacity;
  int size;
  inline int mk(T x) { int s=size; if (capacity==size) resize(); ++size; data[s].v=x; data[s].l=-1; data[s].r=-1; return s; }
  inline void resize() {
    N* p = (N*)realloc(data,sizeof(N)*capacity*2);
    if (p!=NULL) { capacity=capacity*2; data=p; }
    else { printf("Memory allocation error in Set.\n"); }
  }
public:
  Set(int capacity_=128) { capacity=capacity_; size=0; data=(N*)malloc(sizeof(N)*capacity); }
  ~Set() { free(data); }
  void add(T x) {
    if (size==0) mk(x);
    else {
      N* n=&data[0];
      do {
        if (x<n->v) { if (n->l==-1) { n->l=mk(x); return; } else n=&data[n->l]; }
        else if (n->v<x) { if (n->r==-1) { n->r=mk(x); return; } else n=&data[n->r]; }
        else return;
      } while(true);
    }
  }
  int count() { return size; }
  void clear() { size=0; }
};
#endif

/*
int main() {
  Set<long> s(128);
  for (int i=0;i<100;++i) { s.add(rand()&0xf); }
  printf("count = %d\n",s.count());
  //int i=0; while (!h.empty()) { long r=h.get(); printf("%d: %ld\n",++i,r); h.remove(r); }
  return 0;
}
*/
