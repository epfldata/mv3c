#ifndef __Heap_H__
#define __Heap_H__
#include <stdlib.h>
#include <stdio.h>

template <typename T>
class Heap {
private:
  T* data;
  int capacity;
  int size;
  inline void resize() {
    T* p = (T*)realloc(data,sizeof(T)*capacity*2);
    if (p!=NULL) { capacity=capacity*2; data=p; }
    else { printf("Memory allocation error in heap.\n"); }
  }
  inline void percolateDown(int hole) {
    int child = ((hole+1)<<1)-1;
    T tmp = data[hole];
    while (child<size) {
      if (child<size-1 && data[child+1]<data[child]) child+=1;
      if (data[child]<tmp) data[hole]=data[child]; else { data[hole]=tmp; return; }
      hole=child; child=((hole+1)<<1)-1;
    }
    data[hole]=tmp;
  }
public:
  Heap(int capacity_=128) { capacity=capacity_; size=0; data=(T*)malloc(sizeof(T)*capacity); }
  ~Heap() { free(data); }
  inline T get() { return data[0]; }
  inline bool empty() { return size==0; }
  void add(T x) {
    if (size==capacity) resize();
    size+=1;
    int hole=size-1; int h=((hole+1)>>1)-1;
    while (hole>0 && x<data[h]) { data[hole]=data[h]; hole=h; h=((hole+1)>>1)-1; }
    data[hole]=x;
  }
  void remove(T x) {
    int p=-1; for (int i=0;i<size;++i) if (data[i]==x) { p=i; break; } if (p==-1) return;
    data[p]=data[size-1]; --size; if (p<size-1) percolateDown(p);
  }
};
#endif

/*
int main() {
  Heap<long> h(128);
  for (int i=0;i<100;++i) { h.add(rand()&0xfff); }
  int i=0; while (!h.empty()) { long r=h.get(); printf("%d: %ld\n",++i,r); h.remove(r); }
  return 0;
}
*/
