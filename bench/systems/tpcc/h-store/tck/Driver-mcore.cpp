#define DRIVER

#include "ClientSocket.h"
#include "SocketException.h"
#include "Constants.h"
#include "RandStuff.h"
#include "StopWatch.h"
#include <iostream>
#include <string>
#include <assert.h>

#define NUM_TX_ROUNDS 500
#define DEFAULT_WARE_COUNT 8

// Number of random numbers required
// per partition (roughly).
#define RANDS_PER_PART_PER_TX 25

using namespace std;

int do_neword(
    int count_ware, char* toSendDNO, int _core,
    char** toSend, int* curr, int* txcnt,
    char** toSendO, int* currO, int* txcntO);

int do_payment(
    int count_ware, char* toSendDNO, int _core,
    char** toSend, int* curr, int* txcnt,
    char** toSendO, int* currO, int* txcntO);

int do_ordstat(int count_ware, char* toSendDNO, int _core);
int do_delivery(int count_ware, char* toSendDNO, int _core);
int do_slev(int count_ware, char* toSendDNO, int _core);

// Global variable: Number of partitions (cores). */
int num_parts = 1;

/*==================================================================+
  | ROUTINE NAME
  | do_neword()
  +==================================================================*/

int do_neword(
    int count_ware, char* toSendDNO, int _core,
    char** toSend, int* curr, int* txcnt,
    char** toSendO, int* currO, int* txcntO) {

  long w_id;
  long d_id;
  long c_id;
  long ol_cnt;

  int rbk;
  int all_local = 1;

  long* item_id = new long[15];
  long* supware = new long[15];
  long* quantity = new long[15];

  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    long wares_per_core = count_ware/num_parts;
    w_id = RandomNumber((_core-1) * wares_per_core + 1,
                        _core * wares_per_core);
  }

  d_id = RandomNumber(1, DIST_PER_WARE);
  c_id = NURand(1023, 1, CUST_PER_DIST);
  ol_cnt = RandomNumber(5, 15);

  // simulate 1% of transactions with erroneous entry
  rbk = (int) RandomNumber(1, 100);
  long bad_id = MAXITEMS + 1;

  for(int i=0; i < ol_cnt; i++) {

    if(i == (ol_cnt-1) && rbk == 1) {
      // last item and flipped bad
      item_id[i] = bad_id;
    }
    else {
      item_id[i] = NURand(8191, 1, MAXITEMS);
    }

    //if(RandomNumber(1,100)==1)   //XXX Ben got this wrong..
    //// 1% are from home warehouse
    if(RandomNumber(1,100)!=1) {
      supware[i] = w_id;
    }
    else {
      // 1% are from remote warehouse
      if(count_ware == 1) {
        supware[i] = w_id;
      }
      else {
        while((supware[i] = RandomNumber(1, count_ware))==w_id);
        all_local = 0;
      }
    }
    quantity[i] = RandomNumber(1,10);
  }

  bool remote = false;
  int remote_core = -1;
  if (_core) {
    for(int i=0; i < ol_cnt; i++) {
      long wares_per_core = count_ware/num_parts;
      if (supware[i] <= (_core-1) * wares_per_core ||
          supware[i] > _core * wares_per_core) {
        remote = true;
        remote_core = (supware[i]-1)/wares_per_core;
      }
    }
  }

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+newordoffset1)) = d_id;
  *((long*)(ts+newordoffset2)) = c_id;
  *((long*)(ts+newordoffset3)) = ol_cnt;
  *((int*)(ts+newordoffset4)) = all_local;
  memcpy(ts+newordoffset5, item_id, sizeof(long)*ol_cnt);
  memcpy(ts+newordoffset6, supware, sizeof(long)*ol_cnt);
  memcpy(ts+newordoffset7, quantity, sizeof(long)*ol_cnt);

  if (!remote) { // SINGLE-SITE
    *((int*)(toSendDNO)) = 1;
  }
  else { // MULTI_SITE
    *((int*)(toSendDNO)) = 6;
    if (txcnt[remote_core] < TX_PER_ROUND_PER_CORE) {
      ts = toSend[remote_core] + curr[remote_core] + 2 * sizeof(int);
      memcpy(ts, toSendDNO+sizeof(int), newordoffset7 + sizeof(long)*15);
      *((int*)(toSend[remote_core] + curr[remote_core] + sizeof(int))) = 7;
      txcnt[remote_core]++;
      //*((int*)toSend[remote_core]) += 
      //    newordoffset7 + sizeof(long)*15 + sizeof(int);
      *((int*)toSend[remote_core]) += TX_SIZE;
      curr[remote_core] += TX_SIZE;
    }
    else {
      ts = toSendO[remote_core] + currO[remote_core] + 2 * sizeof(int);
      memcpy(ts, toSendDNO+sizeof(int), newordoffset7 + sizeof(long)*15);
      *((int*)(toSendO[remote_core] + currO[remote_core] + sizeof(int))) = 7;
      txcntO[remote_core]++;
      currO[remote_core] += TX_SIZE;
      *((int*)toSendO[remote_core]) += TX_SIZE;
    }
  }

  return newordoffset7 + sizeof(long)*15 + sizeof(int);
}

/*==================================================================+
  | ROUTINE NAME
  | do_payment()
  +==================================================================*/
int do_payment(
    int count_ware, char* toSendDNO, int _core,
    char** toSend, int* curr, int* txcnt,
    char** toSendO, int* currO, int* txcntO) {

  long w_id=1;
  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    long wares_per_core = count_ware/num_parts;
    w_id = RandomNumber((_core-1) * wares_per_core + 1,
                        _core * wares_per_core);
    //while (true) {
      //if (w_id > (_core-1) * wares_per_core && 
      //    w_id <= _core * wares_per_core) break;
    //}
  }

  long d_id = RandomNumber(1, DIST_PER_WARE);
  long c_id = NURand(1023, 1, CUST_PER_DIST);
  long h_amount = RandomNumber(5, 5000);
  long x = RandomNumber(1, 100);
  long y = RandomNumber(1, 100);

  long c_w_id;
  long c_d_id;
  char c_last[16];
  memset(c_last,0,16);
  int clastsiz;
  bool byname;
  int all_local = 1;

  if(x <= 85) {
    c_d_id = d_id;
    c_w_id = w_id;
  }
  else {
    c_d_id = RandomNumber(1, DIST_PER_WARE);
    if(count_ware > 1) {
      while((c_w_id = RandomNumber(1, count_ware)) == w_id);
      assert (c_w_id != w_id);
      all_local = 0;
    }
    else {
      c_w_id = w_id;
    }
  }

  if(y <= 60) {

    // by last name
    byname = true;

    // generate last name
    clastsiz = Lastname(NURand(255,0,999),c_last);
  }
  else {
    // by cust id
    byname = false;
  }

  bool remote = false;
  int remote_core = -1;
  if (_core) {
    long wares_per_core = count_ware/num_parts;
    if (c_w_id <= (_core-1) * wares_per_core ||
        c_w_id > _core * wares_per_core) {
      remote = true;
      remote_core = (c_w_id-1)/wares_per_core;
    }
  }

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+paymentoffset1)) = d_id;
  *((long*)(ts+paymentoffset2)) = c_id;
  *((long*)(ts+paymentoffset3)) = c_w_id;
  *((long*)(ts+paymentoffset4)) = c_d_id;
  *((bool*)(ts+paymentoffset5)) = byname;
  memcpy(ts+paymentoffset6, c_last, 16);
  *((int*)(ts+paymentoffset7)) = clastsiz;
  *((long*)(ts+paymentoffset8)) = h_amount;

  //*((int*)toSendDNO) = paymentoffset8 + sizeof(long) + sizeof(int);

  if (!remote) { // SINGLE-SITE
    //payment(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
    *((int*)(toSendDNO)) = 2;
  }
  else { // MULTI-SITE
    //payment_LOCAL(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
    //payment_REMOTE(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
    //status = 0;
    *((int*)(toSendDNO)) = 8;
    if (txcnt[remote_core] < TX_PER_ROUND_PER_CORE) {
      ts = toSend[remote_core] + curr[remote_core] + 2 * sizeof(int);
      memcpy(ts, toSendDNO+sizeof(int), paymentoffset8 + sizeof(long));
      *((int*)(toSend[remote_core] + curr[remote_core] + sizeof(int))) = 9;
      txcnt[remote_core]++;
      curr[remote_core] += TX_SIZE;
      //*((int*)toSend[remote_core]) += paymentoffset8 + sizeof(long) + sizeof(int);
      *((int*)toSend[remote_core]) += TX_SIZE;
    }
    else {
      ts = toSendO[remote_core] + currO[remote_core] + 2 * sizeof(int);
      memcpy(ts, toSendDNO+sizeof(int), paymentoffset8 + sizeof(long));
      *((int*)(toSendO[remote_core] + currO[remote_core] + sizeof(int))) = 9;
      txcntO[remote_core]++;
      currO[remote_core] += TX_SIZE;
      *((int*)toSendO[remote_core]) += TX_SIZE;
    }
  }

  return paymentoffset8 + sizeof(long) + sizeof(int);
}


/*==================================================================+
  | ROUTINE NAME
  | do_ordstat()
  +==================================================================*/

int do_ordstat(int count_ware, char* toSendDNO, int _core) {

  long w_id = 1;

  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    long wares_per_core = count_ware/num_parts;
    w_id = RandomNumber((_core-1) * wares_per_core + 1,
                        _core * wares_per_core);
    //while (true) {
    //  w_id = RandomNumber(1, count_ware);
    //  if (w_id > (_core-1) * wares_per_core && 
    //      w_id <= _core * wares_per_core) break;
    //}
  }

  long d_id = RandomNumber(1, DIST_PER_WARE);
  long c_id = NURand(1023, 1, CUST_PER_DIST);

  char c_last[16];
  memset(c_last, 0, 16);
  int clastsiz;
  bool byname;

  long y = RandomNumber(1, 100);
  if(y <= 60) {

    // by last name
    byname = true;

    // generate last name
    clastsiz = Lastname(NURand(255,0,999),c_last);
  }
  else {
    // by cust id
    byname = false;
  }

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+ordstatoffset1)) = d_id;
  *((long*)(ts+ordstatoffset2)) = c_id;
  *((bool*)(ts+ordstatoffset3)) = byname;
  memcpy(ts+ordstatoffset4, c_last, 16);
  *((int*)(ts+ordstatoffset5)) = clastsiz;
  *((int*)(toSendDNO)) = 3;
  return ordstatoffset5 + 2*sizeof(int);
}

/*==================================================================+
  | ROUTINE NAME
  | do_delivery()
  +==================================================================*/

int do_delivery(int count_ware, char* toSendDNO, int _core) {
  long w_id=1;

  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    long wares_per_core = count_ware/num_parts;
    w_id = RandomNumber((_core-1) * wares_per_core + 1,
                        _core * wares_per_core);
    //while (true) {
    //  long wares_per_core = count_ware/num_parts;
    //  w_id = RandomNumber(1, count_ware);
    //  if (w_id > (_core-1) * wares_per_core && 
    //      w_id <= _core * wares_per_core) break;
    //}
  }

  long o_carrier_id = RandomNumber(1, 10);

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+sizeof(long))) = o_carrier_id;
  *((int*)(toSendDNO)) = 4;
  return 2*sizeof(long) + sizeof(int);
}


/*==================================================================+
  | ROUTINE NAME
  | do_slev()
  +==================================================================*/

int do_slev(int count_ware, char* toSendDNO, int _core) {
  long w_id = 1;

  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    long wares_per_core = count_ware/num_parts;
    w_id = RandomNumber((_core-1) * wares_per_core + 1,
                        _core * wares_per_core);
    //while (true) {
    //  long wares_per_core = count_ware/num_parts;
    //  w_id = RandomNumber(1, count_ware);
    //  if (w_id > (_core-1) * wares_per_core && 
    //      w_id <= _core * wares_per_core) break;
    //}
  }

  long d_id = RandomNumber(1, DIST_PER_WARE);
  long threshold = RandomNumber(10, 20);

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+sizeof(long))) = d_id;
  *((long*)(ts+2*sizeof(long))) = threshold;
  *((int*)(toSendDNO)) = 5;
  return 3*sizeof(long) + sizeof(int);
}

#if 0
// Updates message pointers to keep them in sync
// with tx counts.
// TODO: Is there a cheaper and more reliable way
// to do this? I suspect there is: converting the
// byte copy operations to using a relay() API.
void update_ptrs(
  int* curr, int* currO,
  char** toSend, char** toSendO,
  int* txcnt, int* txcntO) {
  // Update pointers for all cores.
  for (int p=0; p<num_parts; ++p) {
    assert(curr[p] == txcnt[p] * TX_SIZE);
    //curr[p] = txcnt[p]*TX_SIZE;
    assert(currO[p] == txcntO[p] * TX_SIZE);
    //currO[p] = txcntO[p]*TX_SIZE;
    //printf("update: toSend[%d] = %d curr[%d] = %d\n",
    //        p, *((int*)toSend[p]), p, curr[p]);
    assert (*((int*)toSend[p]) == curr[p]);
    //printf("update: toSendO[%d] = %d currO[%d] = %d\n",
    //        p, *((int*)toSendO[p]), p, currO[p]);
    assert (*((int*)toSendO[p]) == currO[p]);
    //*((int*)toSend[p]) = curr[p];
    //*((int*)toSendO[p]) = currO[p];
  }
}
#endif

int main (int argc, char** argv) {
  int count_ware = DEFAULT_WARE_COUNT;

  // Read #cores and #warehouses from command line.
  if (argc!=3) {
    printf("Usage - ./driver-mcore [#parts (must match #server instances)] [#warehouses (must match W value on servers)]\n");
    exit(-1);
  }

  num_parts = atoi(argv[1]);
  count_ware = atoi(argv[2]);

  // Perform sanity checks.
  assert(num_parts>0 && count_ware>0 && count_ware%num_parts==0);

  // Precompute random numbers.
  PrecomputeRands(RANDS_PER_PART_PER_TX * TX_PER_ROUND_PER_CORE * 
                  NUM_TX_ROUNDS * num_parts);

  // This is the main program.
  try {
    ClientSocket** client_socket = new ClientSocket* [num_parts];
    char** toSend = new char* [num_parts];
    char** toSendO = new char* [num_parts];
    int* curr = new int[num_parts];
    int* currO = new int[num_parts];
    int* txcnt = new int[num_parts];
    int* txcntO = new int[num_parts];

    for (int p = 0; p < num_parts; ++p) {
      client_socket[p] =
          new ClientSocket("localhost", BEGIN_PORT + p);
      toSend[p] = new char[MSG_SIZE];
      toSendO[p] = new char[MSG_SIZE];
      memset(toSendO[p], 0, MSG_SIZE);
      curr[p] = 0;
      currO[p] = 0;
      txcnt[p] = 0;
      txcntO[p] = 0;
    }

    char* reply = new char[2*sizeof(int)];
    try {
      int prob;
      int committed = 0;
      int temp;
      StopWatch* sw = new StopWatch();
      sw->start();

      // We have NUM_TX_ROUNDS rounds of transactions,
      // each round consisting of TX_PER_ROUND_PER_CORE messages
      // sent to each core. Pending messages are
      // queued up in currO, toSendO, txcntO and dealt with
      // in the next round (thus compensating and ensuring the
      // overall probability distribution of txns is as per TPC-C).
      for (int i=0; i<NUM_TX_ROUNDS; ++i) {
        // Round begins here.

        // Copy over old deficits/pending messages to compensate.
        for (int p=0; p<num_parts; ++p) {
          memcpy(toSend[p], toSendO[p], MSG_SIZE);
          memset(toSendO[p], 0, MSG_SIZE);
          txcnt[p] = txcntO[p];
          curr[p] = currO[p];
          txcntO[p] = 0;
          currO[p] = 0;
        }

        // Generate transactions as per TPC-C probabilities,
        // until this round is complete.
        bool round_complete;
        do {
          round_complete = true;
          // For each core.
          for (int p=0; p<num_parts; ++p) {
            // Skip if quota for this core is complete.
            if (txcnt[p] >= TX_PER_ROUND_PER_CORE)
              continue;

            round_complete = false;

            // Update ptrs: remote txns issued to previous core
            // might have changed them.
            //update_ptrs(curr, currO, toSend, toSendO, txcnt, txcntO);

            // Otherwise generate new txns to send to this core.
            // Generate new txns, with probabilities as per TPC-C spec.
            prob = RandomNumber(0, 99);
            if (prob < 4) { // 4% ordstat txns
              do_ordstat(count_ware, toSend[p] + sizeof(int) + curr[p], p+1);
              //printf("curr[%d] = %d: ordstat wrote proc %d\n", p, curr[p],
              //        *((int*)(toSend[p] + sizeof(int) + curr[p])));
              *((int*)toSend[p]) += TX_SIZE;
              //printf("do_ordstat incremented toSend[%d] to %d\n",
              //        p, *((int*)toSend[p]));
            }
            else if (prob < 8) { // 4% stock level txns
              do_slev(count_ware, toSend[p] + sizeof(int) + curr[p], p+1);
              //printf("curr[%d] = %d: slev wrote proc %d\n", p, curr[p],
              //        *((int*)(toSend[p] + sizeof(int) + curr[p])));
              *((int*)toSend[p]) += TX_SIZE;
              //printf("do_slev incremented toSend[%d] to %d\n",
              //        p, *((int*)toSend[p]));
            }
            else if (prob < 12) { // 4% delivery txns
              do_delivery(count_ware, toSend[p] + sizeof(int) + curr[p], p+1);
              //printf("curr[%d] = %d: delivery wrote proc %d\n", p, curr[p],
              //        *((int*)(toSend[p] + sizeof(int) + curr[p])));
              *((int*)toSend[p]) += TX_SIZE;
              //printf("do_delivery incremented toSend[%d] to %d\n",
              //        p, *((int*)toSend[p]));
            }
            else if (prob < 55) { // 43% payment txns
              // 15% payment transactions are remote.
              // Local vs remote logic is now buried in do_payment().
              // So is the logic to update txcnt[remote] or
              // txcntO[remote] as needed.
              do_payment(count_ware, toSend[p] + sizeof(int) + curr[p],
                         p+1, toSend, curr, txcnt,
                         toSendO, currO, txcntO);
              //printf("curr[%d] = %d: payment wrote proc %d\n", p, curr[p],
              //        *((int*)(toSend[p] + sizeof(int) + curr[p])));
              *((int*)toSend[p]) += TX_SIZE;
              //printf("do_payment incremented toSend[%d] to %d\n",
              //        p, *((int*)toSend[p]));
            }
            else { // 45% new order txns
              // 1% new order txns are remote.
              // Local vs remote logic is now buried in do_neword().
              // So is the logic to update txcnt[remote] or
              // txcntO[remote] as needed.
              do_neword(count_ware, toSend[p] + sizeof(int) + curr[p],
                        p+1, toSend, curr, txcnt,
                        toSendO, currO, txcntO);
              //printf("curr[%d] = %d: neword wrote proc %d\n", p, curr[p],
              //        *((int*)(toSend[p] + sizeof(int) + curr[p])));
              *((int*)toSend[p]) += TX_SIZE;
              //printf("do_neword incremented toSend[%d] to %d\n",
              //        p, *((int*)toSend[p]));
            }

            // Increment local transaction count.
            txcnt[p]++;
            curr[p] += TX_SIZE;
          } // end of for()
        } while (!round_complete);

        // Dispatch messages.
        for (int p=0; p<num_parts; ++p) {
          //printf("Sending message to core %d: proc=%d\n", p,
          //        *((int*)(toSend[p] + sizeof(int))));
          (*(client_socket[p])) << toSend[p];
        }

        // Wait for replies.
        for (int p=0; p<num_parts; ++p) {
          (*(client_socket[p])) >> reply;
          committed += *((int*)(reply + sizeof(int)));
        }

        // Round ends here.

      } // end of OUTER for, after NUM_TX_ROUNDS rounds.

      double elapsed = sw->stop(); 
      printf("Committed %d transactions, TP = %f\n",
              committed, committed*1000.0/elapsed);
    }
    catch ( SocketException& e) {
      std::cout << "Exception was caught:" << e.description() << "\n";
    }

    // Free up resources.
    for (int p = 0; p < num_parts; ++p) {
      delete client_socket[p];
      delete [] toSend[p];
      delete [] toSendO[p];
    }

    delete [] client_socket;
    delete [] toSend;
    delete [] toSendO;
    delete [] curr;
    delete [] currO;
    delete [] txcnt;
    delete [] txcntO;
  }

  catch ( SocketException& e ) {
    std::cout << "Exception was caught:" << e.description() << "\n";
  }

  // Deallocate precomputed random numbers.
  DeallocateRands();

  return 0;
} // end of main()
