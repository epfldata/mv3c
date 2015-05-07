#include "ClientSocket.h"
#include "SocketException.h"
#include "Constants.h"
#include "RandStuff.h"
#include "StopWatch.h"
#include <assert.h>
#include <iostream>
#include <string>

using namespace std;
int do_neword(int count_ware, char* toSendDNO, char* toSend2, int _core);
int do_payment(int count_ware, char* toSendDNO, char* toSend2, int _core);
int do_ordstat(int count_ware, char* toSendDNO, int _core);
int do_delivery(int count_ware, char* toSendDNO, int _core);
int do_slev(int count_ware, char* toSendDNO, int _core);

/*int neword(long w_id, long d_id, long c_id, long ol_cnt,
            int all_local, long* item_id, long* supware, long* quantity) {

  cout << w_id << " " << d_id << " " << c_id << " " << ol_cnt << " " << all_local << " " << item_id[0] << " " << supware[0] << " " << quantity[0] << endl;
}

void ordstat(long w_id, long d_id, long c_id,
             bool byname, char* c_last, int clastsiz) {

  std::cout << w_id << " " << d_id << " " << c_id << " " << byname << " " << c_last[0] << " " << clastsiz << std::endl;
  }*/

/*==================================================================+
| ROUTINE NAME
| do_neword()
+==================================================================*/

int do_neword(int count_ware, char* toSendDNO, char* toSend2, int _core) {

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
    if (_core == 1) {
      while((w_id = RandomNumber(1, count_ware)) > count_ware/2);
    }
    else {
      while((w_id = RandomNumber(1, count_ware)) <= count_ware/2);
    }
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

    //if(RandomNumber(1,100)==1) {  //XXX Ben got this wrong..
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
  if (_core) {
    for(int i=0; i < ol_cnt; i++) {
      if (_core == 1) {
        if (supware[i]> count_ware/2)
          remote = true;
      }
      if (_core == 2) {
        if (supware[i] <=count_ware/2)
          remote = true;
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

  //int status = neword(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
  //*((int*)toSendDNO) = newordoffset7 + sizeof(long)*15 + sizeof(int);

  if (!remote) { // SINGLE-SITE
    //status = neword(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
    *((int*)(toSendDNO)) = 1;
  }
  else { // MULTI_SITE
    //status = neword_LOCAL(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
    *((int*)(toSendDNO)) = 6;
    ts = toSend2 + sizeof(int);
    memcpy(ts, toSendDNO+sizeof(int), newordoffset7 + sizeof(long)*15);
    // status = neword_REMOTE(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
    *((int*)(toSend2)) = 7;
  }


  return newordoffset7 + sizeof(long)*15 + sizeof(int);
}

/*==================================================================+
| ROUTINE NAME
| do_payment()
+==================================================================*/
int do_payment(int count_ware, char* toSendDNO, char* toSend2, int _core) {

  long w_id=1;
  if (!_core) {
    w_id = RandomNumber(1, count_ware);
  }
  else {
    if (_core == 1) {
      while((w_id = RandomNumber(1, count_ware)) > count_ware/2);
    }
    else {
      while((w_id = RandomNumber(1, count_ware)) <= count_ware/2);
    }
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
      while((c_w_id = RandomNumber(1, count_ware)) != w_id);
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
  if (_core) {
      if (_core == 1) {
        if (c_w_id > count_ware/2)
          remote = true;
      }
      if (_core == 2) {
        if (c_w_id <= count_ware/2)
          remote = true;
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
    // payment_REMOTE(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
    //status = 0;
    *((int*)(toSendDNO)) = 8;
    ts = toSend2 + sizeof(int);
    memcpy(ts, toSendDNO+sizeof(int), paymentoffset8 + sizeof(long));
    *((int*)(toSend2)) = 9;
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
    if (_core == 1) {
      while((w_id = RandomNumber(1, count_ware)) > count_ware/2);
    }
    else {
      while((w_id = RandomNumber(1, count_ware)) <= count_ware/2);
    }
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

/*
  std::cout << w_id << " " << d_id << " " << c_id << " " << byname << " ";
  for (int i = 0; i < clastsiz; i++) {
    if (c_last[i]) {
      std::cout << (ts+ordstatoffset4)[i];
    }
    else {
      break;
    }
  }

  std::cout << " " << clastsiz << " " << _core << std::endl;

  if (c_id == 2054 && d_id == 2 && c_last[0]=='A')
  cout << "hi\n" << endl;*/

  //ordstat(w_id, d_id, c_id, byname, c_last, clastsiz);
  //*((int*)toSendDNO) = ordstatoffset5 + 2*sizeof(int);
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
    if (_core == 1) {
      while((w_id = RandomNumber(1, count_ware)) > count_ware/2);
    }
    else {
      while((w_id = RandomNumber(1, count_ware)) <= count_ware/2);
    }
  }

  long o_carrier_id = RandomNumber(1, 10);

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+sizeof(long))) = o_carrier_id;
  //delivery(w_id, o_carrier_id);
  //*((int*)toSendDNO) = 2*sizeof(long) + sizeof(int);
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
    if (_core == 1) {
      while((w_id = RandomNumber(1, count_ware)) > count_ware/2);
    }
    else {
      while((w_id = RandomNumber(1, count_ware)) <= count_ware/2);
    }
  }

  long d_id = RandomNumber(1, DIST_PER_WARE);
  long threshold = RandomNumber(10, 20);

  char* ts = toSendDNO + sizeof(int);
  *((long*)ts) = w_id;
  *((long*)(ts+sizeof(long))) = d_id;
  *((long*)(ts+2*sizeof(long))) = threshold;
  //slev(w_id, d_id, threshold);
  //*((int*)toSendDNO) = 3*sizeof(long) + sizeof(int);
  *((int*)(toSendDNO)) = 5;
  return 3*sizeof(long) + sizeof(int);
}

int main ( int argc, char* argv[]) {
  try {
    ClientSocket client_socket ( "localhost", 30000 );
    ClientSocket client_socket2 ( "localhost", 30001 );
    char* reply = new char[2*sizeof(int)];
    char* toSend1 = new char[sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int))];
    char* toSend2 = new char[sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int))];
    char* toSendO1 = new char[sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int))];
    char* toSendO2 = new char[sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int))];
    memset(toSendO1,0,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
    memset(toSendO2,0,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
    int count_ware=10;
    int tempcount1 = 0;
    int tempcount2 = 0;
    try {

      int prob;
      int committed = 0;
      int currL;
      int currR;
      int lj;
      int rj;
      int currOL;
      int currOR;
      int lOj = 0;
      int rOj = 0;
      int temp;
      StopWatch* sw = new StopWatch();
      sw->start();
      for (int i=0; i<2000; i++) {
	memcpy(toSend1, toSendO1,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
	memcpy(toSend2, toSendO2,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
	memset(toSendO1,0,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
	memset(toSendO2,0,sizeof(int) + 25*(sizeof(long)*(49) + 2*sizeof(int)));
	lj = lOj;
	rj = rOj;
	lOj = 0;
	rOj = 0;
	while ((lj < 25) || (rj < 25)) {
	  //for (int j=0; j<10; j++) {
	  if (lj < 25) {
	    currL = lj*(sizeof(long)*(49) + 2*sizeof(int));
	    currR = rj*(sizeof(long)*(49) + 2*sizeof(int));
	    currOL = lOj*(sizeof(long)*(49) + 2*sizeof(int));
	    currOR = rOj*(sizeof(long)*(49) + 2*sizeof(int));
	    *((int*)toSend1) = currL;
	    *((int*)toSend2) = currR;
	    *((int*)toSendO1) = currOL;
	    *((int*)toSendO2) = currOR;
	    prob = RandomNumber(0, 99);
	    if (prob < 4) {
	      *((int*)toSend1) += do_ordstat(count_ware, toSend1 + sizeof(int) + currL, 1);
	    }
	    else if (prob < 8) {
	      *((int*)toSend1) += do_slev(count_ware, toSend1 + sizeof(int) + currL, 1);
	    }
	    else if (prob < 12) {
	      *((int*)toSend1) += do_delivery(count_ware, toSend1 + sizeof(int) + currL, 1);
	    }
	    else if (prob < 55) {
	      if (rj < 25) {
		temp = do_payment(count_ware, toSend1 + sizeof(int) + currL, toSend2 + sizeof(int) + currR, 1);
		*((int*)toSend1) += temp;
		if (*((int*)(toSend2 + sizeof(int) + currR))) {
		  assert(*((int*)(toSend1 + sizeof(int) + currL)) == 8);
		  rj++;
		  *((int*)toSend2) += temp;
		}
	      }
	      else {
		*((int*)toSend1) += do_payment(count_ware, toSend1 + sizeof(int) + currL, toSendO2 + sizeof(int) + currOR, 1);
		if (*((int*)(toSendO2 + sizeof(int) + currOR))) {
		  assert(*((int*)(toSend1 + sizeof(int) + currL)) == 8);
		  rOj++;
		}
	      }
	    }
	    else {
	      if (rj < 25) {
		temp = do_neword(count_ware, toSend1 + sizeof(int) + currL, toSend2 + sizeof(int) + currR, 1);
		*((int*)toSend1) += temp;
		if (*((int*)(toSend2 + sizeof(int) + currR))) {
		  assert(*((int*)(toSend1 + sizeof(int) + currL)) == 6);
		  rj++;
		  *((int*)toSend2) += temp;
		}
	      }
	      else {
		*((int*)toSend1) += do_neword(count_ware, toSend1 + sizeof(int) + currL, toSendO2 + sizeof(int) + currOR, 1);
		if (*((int*)(toSendO2 + sizeof(int) + currOR))) {
		  assert(*((int*)(toSend1 + sizeof(int) + currL)) == 6);
		  rOj++;
		}
	      }
	    }
	    lj++;
	  }
	  if (rj < 25) {
	    currL = lj*(sizeof(long)*(49) + 2*sizeof(int));
	    currR = rj*(sizeof(long)*(49) + 2*sizeof(int));
	    currOL = lOj*(sizeof(long)*(49) + 2*sizeof(int));
	    currOR = rOj*(sizeof(long)*(49) + 2*sizeof(int));
	    *((int*)toSend1) = currL;
	    *((int*)toSend2) = currR;
	    *((int*)toSendO1) = currOL;
	    *((int*)toSendO2) = currOR;
	    prob = RandomNumber(0, 99);
	    if (prob < 4) {
	      *((int*)toSend2) += do_ordstat(count_ware, toSend2 + sizeof(int) + currR, 2);
	    }
	    else if (prob < 8) {
	      *((int*)toSend2) += do_slev(count_ware, toSend2 + sizeof(int) + currR, 2);
	    }
	    else if (prob < 12) {
	      *((int*)toSend2) += do_delivery(count_ware, toSend2 + sizeof(int) + currR, 2);
	    }
	    else if (prob < 55) {
	      if (lj < 25) {
		temp = do_payment(count_ware, toSend2 + sizeof(int) + currR, toSend1 + sizeof(int) + currL, 2);
		*((int*)toSend2) += temp;
		if (*((int*)(toSend1 + sizeof(int) + currL))) {
		  assert(*((int*)(toSend2 + sizeof(int) + currR)) == 8);
		  lj++;
		  *((int*)toSend1) += temp;
		}
	      }
	      else {
		*((int*)toSend2) += do_payment(count_ware, toSend2 + sizeof(int) + currR, toSendO1 + sizeof(int) + currOL, 2);
		if (*((int*)(toSendO1 + sizeof(int) + currOL))) {
		  assert(*((int*)(toSend2 + sizeof(int) + currR)) == 8);
		  lOj++;
		}
	      }
	    }
	    else {
	      if (lj < 25) {
		temp = do_neword(count_ware, toSend2 + sizeof(int) + currR, toSend1 + sizeof(int) + currL, 2);
		*((int*)toSend2) += temp;
		if (*((int*)(toSend1 + sizeof(int) + currL))) {
		  assert(*((int*)(toSend2 + sizeof(int) + currR)) == 6);
		  lj++;
		  *((int*)toSend1) += temp;
		}
	      }
	      else {
		*((int*)toSend2) += do_neword(count_ware, toSend2 + sizeof(int) + currR, toSendO1 + sizeof(int) + currOL, 2);
		if (*((int*)(toSendO1 + sizeof(int) + currOL))) {
		  assert(*((int*)(toSend2 + sizeof(int) + currR)) == 6);
		  lOj++;
		}
	      }
	    }
	    rj++;
	  }
	}
	client_socket << toSend1;
	client_socket2 << toSend2;
	client_socket >> reply;
	committed += *((int*)(reply + sizeof(int)));
	client_socket2 >> reply;
	committed += *((int*)(reply + sizeof(int)));
      }

      double elapsed = sw->stop();
      printf("Committed %d transactions, TP = %f\n", committed, committed * 1000.0 / elapsed);


      //while(true) {

      //char* received= do_neword(1);
      //long* temp2 = (long*) (received + sizeof(int));
      //cout << "temp1: " << temp2[0] << " " << temp2[1] << endl;

      //int committed = neword(*(long*)received, *(long*)(received+newordoffset1), *(long*)(received+newordoffset2), *(long*)(received+newordoffset3), *(int*)(received+newordoffset4), (long*)(received + newordoffset5), (long*)(received + newordoffset6), (long*)(received + newordoffset7));


      //client_socket << do_neword(1);
      //client_socket >> reply;

      //cout << *(int*)(reply) << " " << *((int*)(reply + sizeof(int))) << endl;
	//}
    }
    catch ( SocketException& e) {
      std::cout << "Exception was caught:" << e.description() << "\n";
    }

  }
  catch ( SocketException& e )
    {
      std::cout << "Exception was caught:" << e.description() << "\n";
    }

  return 0;
}

