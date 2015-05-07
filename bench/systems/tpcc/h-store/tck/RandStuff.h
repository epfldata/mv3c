#ifndef RANDSTUFF_H
#define RANDSTUFF_H
#include <cstring>
#include <stdio.h>
#include <cstdlib>
#include <assert.h>

long RandomNumber(long min, long max);
int MakeAlphaString(int min, int max, char* str);
int MakeNumberString(int min, int max, char* str);
long NURand(int A, int x, int y);
int Lastname(int num, char* name);
bool C_255_init = false;
bool C_1023_init = false;
bool C_8191_init = false;
int C_255, C_1023, C_8191;
//int newordoffset1;
//int newordoffset2;
//int newordoffset3;
//int newordoffset4;
//int newordoffset5;
//int newordoffset6;
//int newordoffset7;
//void setnewordoffsets();
//void setnewordoffsets{
int newordoffset1 = sizeof(long);
int newordoffset2 = 2*sizeof(long);
int newordoffset3 = 3*sizeof(long);
int newordoffset4 = 4*sizeof(long);
int newordoffset5 = 4*sizeof(long) + sizeof(int);
int newordoffset6 = 4*sizeof(long) + sizeof(int) + 15*sizeof(long);
int newordoffset7 = 4*sizeof(long) + sizeof(int) + 30*sizeof(long);
int paymentoffset1 = sizeof(long);
int paymentoffset2 = 2*sizeof(long);
int paymentoffset3 = 3*sizeof(long);
int paymentoffset4 = 4*sizeof(long);
int paymentoffset5 = 5*sizeof(long);
int paymentoffset6 = 5*sizeof(long) + sizeof(bool);
int paymentoffset7 = 5*sizeof(long) + sizeof(bool) + 16;
int paymentoffset8 = 5*sizeof(long) + sizeof(bool) + 16 + sizeof(int);
int ordstatoffset1 = sizeof(long);
int ordstatoffset2 = 2*sizeof(long);
int ordstatoffset3 = 3*sizeof(long);
int ordstatoffset4 = 3*sizeof(long) + sizeof(bool);
int ordstatoffset5 = 3*sizeof(long) + sizeof(bool) + 16;
//}

#ifdef DRIVER
// Buffer for fast random generation.
int *rand_buffer = 0;
int rand_index = 0;

/*
 * Precompute results of num_rands() calls to rand()
 * and store in a buffer for fast access.
 */
void PrecomputeRands(int num_rands) {
  assert(!rand_buffer);
  rand_buffer = new int[num_rands];
  for (int i = 0; i < num_rands; ++i) {
    rand_buffer[i] = rand();
  }
}

/* Deallocate precomputed rand buffer. */
void DeallocateRands() {
  assert(rand_buffer);
  delete [] rand_buffer;
  rand_index = 0;
}
#endif

/*==================================================================+
| ROUTINE NAME
| RandomNumber
+==================================================================*/

#ifdef DRIVER
inline long RandomNumber(long min, long max) {
  long r = (long)rand_buffer[rand_index++];
  return (min + (r % (max - min + 1)));
}
#else
long RandomNumber(long min, long max) {
  long r = (long)rand();
  return (min + (r % (max - min + 1)));
}
#endif

/*==================================================================+
| ROUTINE NAME
| MakeAlphaString
+==================================================================*/

int MakeAlphaString(int min, int max, char* str) {

  char char_list[] = {'1','2','3','4','5','6','7','8','9','a','b','c',
                      'd','e','f','g','h','i','j','k','l','m','n','o',
                      'p','q','r','s','t','u','v','w','x','y','z','A',
                      'B','C','D','E','F','G','H','I','J','K','L','M',
                      'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
  int cnt = (int)RandomNumber(min, max);
  for (int i = 0; i < cnt; i++) {
    str[i] = char_list[(int)RandomNumber(0L, 60L)];
  }
  return cnt;
}


/*==================================================================+
| ROUTINE NAME
| MakeNumberString
+==================================================================*/

int MakeNumberString(int min, int max, char* str) {

  int cnt = (int)RandomNumber(min, max);
  for (int i = 0; i < cnt; i++) {
    int r = (int)RandomNumber(0L,9L);
    switch(r) {
      case 0:
        str[i] = '0';
        break;
      case 1:
        str[i] = '1';
        break;
      case 2:
        str[i] = '2';
        break;
      case 3:
        str[i] = '3';
        break;
      case 4:
        str[i] = '4';
        break;
      case 5:
        str[i] = '5';
        break;
      case 6:
        str[i] = '6';
        break;
      case 7:
        str[i] = '7';
        break;
      case 8:
        str[i] = '8';
        break;
      case 9:
        str[i] = '9';
        break;
    }
  }
  return cnt;
}

/*==================================================================+
| ROUTINE NAME
| NURand
+==================================================================*/

inline long NURand(int A, int x, int y) {
  int C = 0;
  switch(A) {
    case 255:
      if(!C_255_init) {
        C_255 = (int) RandomNumber(0,255);
        C_255_init = true;
      }
      C = C_255;
      break;
    case 1023:
      if(!C_1023_init) {
        C_1023 = (int) RandomNumber(0,1023);
        C_1023_init = true;
      }
      C = C_1023;
      break;
    case 8191:
      if(!C_8191_init) {
        C_8191 = (int) RandomNumber(0,8191);
        C_8191_init = true;
      }
      C = C_8191;
      break;
    default:
      printf("Error! NURand: A=%d\n", A);
      exit(-1);
  }
  return(((RandomNumber(0,A) | RandomNumber(x,y))+C)%(y-x+1))+x;
}

/*==================================================================+
| ROUTINE NAME
| Lastname
| DESCRIPTION
| TPC-C Lastname Function.
| ARGUMENTS
| num - non-uniform random number
| name - last name string
+==================================================================*/

inline int Lastname(int num, char* name) {
  int i;
  static const char *n[] =
    {"BAR", "OUGHT", "ABLE", "PRI", "PRES",
     "ESE", "ANTI", "CALLY", "ATION", "EING"};

  strcpy(name,n[num/100]);
  strcat(name,n[(num/10)%10]);
  strcat(name,n[num%10]);

  return strlen(name);
}

#endif // RANDSTUFF_H
