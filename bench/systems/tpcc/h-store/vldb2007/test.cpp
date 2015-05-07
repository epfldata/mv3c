#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"
#include "TupleBuffer.h"
#include "HashIndex.h"
#include "ServerSocket.h"
#include "SocketException.h"
#include "StopWatch.h"
#include "RandStuff.h"
#include "util.h"

extern long count_ware;
int _core;

/* Functions */
void InitPermutation();
long GetPermutation();
void LoadItems();
void LoadWare();
void LoadCust();
void LoadOrd();
void LoadNewOrd();
long Stock(long w_id);
long District(long w_id);
long Customer(long d_id, long w_id);
void Orders(long d_id, long w_id);
void New_Orders();
void MakeAddress();
void Error();

/* Transaction-related functions */
int do_neword();
int do_payment();
void do_ordstat();
void do_delivery();
void do_slev();
int neword(long w_id, long d_id, long c_id, long ol_cnt,
           int all_local, long* item_id, long* supware, long* quantity);
int neword_LOCAL(long w_id, long d_id, long c_id, long ol_cnt,
           int all_local, long* item_id, long* supware, long* quantity);
int neword_REMOTE(long w_id, long d_id, long c_id, long ol_cnt,
           int all_local, long* item_id, long* supware, long* quantity);
void payment(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount);
void payment_LOCAL(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount);
void payment_REMOTE(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount);
void ordstat(long w_id, long d_id, long c_id,
             bool byname, const char* c_last, int clastsiz);
void delivery(long w_id, long o_carrier_id);
void slev(long w_id, long d_id, long threshold);

#ifdef USE_PAPI
util* u;
long_long* values;
long_long inst_count[200];
long_long cyc_count[200];
long_long neword_cyc[200];
long_long payment_cyc[200];
long_long neword_inst[200];
long_long payment_inst[200];
int inst = 0;
int neword_i = 0;
int payment_i = 0;
int neword_completed = 0;
int payment_completed = 0;
#endif
bool stavros=false;

void read_inst() {

#ifdef USE_PAPI
  if (PAPI_read(u->eventSet, values) != PAPI_OK) {
      cout << "*** PAPI_read failed! *** " << endl;
      exit(1);
  }
  inst_count[inst] = values[6];
  cyc_count[inst] = values[7];
  ++inst;
#endif

  return;
}
void print_inst() {
#ifdef USE_PAPI
  for(int i=0; i<inst-1; i++)
    cout << "Chkpoint #" << i+1 << ": " << inst_count[i+1]-inst_count[i] << " instructions" << endl;
  cout << endl;
  for(int i=0; i<inst-1; i++)
    cout << "Chkpoint #" << i+1 << ": " << cyc_count[i+1]-cyc_count[i] << " cycles" << endl;
#endif
  return;
}

/* Global SQL Variables */
/*============================+
EXEC SQL BEGIN DECLARE SECTION;
+============================*/
char timestamp[20];
long count_ware;
/*============================+
EXEC SQL END DECLARE SECTION;
+============================*/

/* Global Variables */
int i;
int option_debug = 0; /* 1 if generating debug output */
long perm_c_id[CUST_PER_DIST];
int perm_count = 0;
long h_table_size = 0;
long no_table_size = 0;
long ol_table_size = 0;
long o_table_size = 0;
// XXX: this is hack to keep the lowest new order id for each district
long* lowest_no_id;

/* TPC-C Tables */
char* ITEM; //  | long I_ID | long I_IM_ID | float I_PRICE | char offset #1
            //  | char offset #2 | varchar(14-24) I_NAME | varchar(26-50) I_DATA |   

BPlusTree<int,byte*,8,8> item_tree;

char* WAREHOUSE; // | long W_ID | float W_TAX | float W_YTD | char[2] W_STATE | char[9] W_ZIP    
                 // | char offset #1 |  char offset #2 | char offset #3 | char offset #4
                 // | varchar(6-10) W_NAME | varchar(10-20) W_STREET_1
                 // | varchar(10-20) W_STREET_2 | varchar(10-20) W_CITY 

BPlusTree<int,byte*,8,8> warehouse_tree;


char* STOCK; // | long S_I_ID | long S_W_ID | long S_QUANTITY
             // | float S_YTD | int S_CNT_ORDER | int S_CNT_REMOTE
             // | char[24] S_DIST_01 | char[24] S_DIST_02 | char[24] S_DIST_03 | char[24] S_DIST_04
             // | char[24] S_DIST_05 | char[24] S_DIST_06 | char[24] S_DIST_07 | char[24] S_DIST_08
             // | char[24] S_DIST_09 | char[24] S_DIST_10 | int offset | varchar(26-50) S_DATA 
#define STOCKkey(s_i_id, s_w_id)     (s_w_id*MAXITEMS + s_i_id)

//BPlusTree<int,byte*,8,8> stock_tree;
//HashIndex* stockhi; // hashkey is (s_i_id, s_w_id)
char** stockarray; // array index is STOCKkey(s_i_id, s_w_id)

char* DISTRICT; // | long D_ID | long D_W_ID | float D_TAX | float D_YTD | long D_NEXT_O_ID
                // | char[2] D_STATE | char[9] D_ZIP
                // | char offset #1 |  char offset #2 | char offset #3 | char offset #4
                // | varchar(6-10) D_NAME | varchar(10-20) D_STREET_1
                // | varchar(10-20) D_STREET_2 | varchar(10-20) D_CITY
#define DISTRICTkey(d_id, d_w_id)    (d_w_id * DIST_PER_WARE + d_id)

BPlusTree<int,byte*,8,8> district_tree;

char* CUSTOMER; // | long C_ID | long C_D_ID | long C_W_ID
                // | long C_CREDIT_LIM | float C_DISCOUNT | float C_BALANCE
                // | float C_YTD_PAYMENT | long C_CNT_PAYMENT | long C_CNT_DELIVERY
                // | char[2] C_MIDDLE | char[2] C_STATE | char[9] C_ZIP
                // | char[16] C_PHONE | char[11] C_SINCE | char[2] C_CREDIT
                // | char offset #1 | char offset #2 | char offset #3
                // | char offset #4 | char offset #5 | int offset #6
                // | varchar(8-16) C_FIRST | varchar(10-16) C_LAST
                // | varchar(10-20) C_STREET_1 | varchar(10-20) C_STREET_2 | varchar(10-20) C_CITY
                // | varchar(300-500) C_DATA
#define CUSTOMERkey(c_id, c_d_id, c_w_id)   ((c_w_id*DIST_PER_WARE + c_d_id)*CUST_PER_DIST + c_id)

BPlusTree<int,byte*,8,8> customer_tree;
HashIndex* custhi;

char* HISTORY; // | long H_C_ID | long H_C_D_ID | long H_C_W_ID
               // | long H_W_ID | long H_D_ID | float H_AMOUNT
               // | char[11] H_DATE | char offset | varchar(12-24) H_DATA

char* NEW_ORDER; // | long NO_O_ID | long NO_D_ID | long NO_W_ID
#define NEW_ORDERkey(no_o_id, no_d_id, no_w_id)   ((no_w_id*DIST_PER_WARE + no_d_id)*ORD_PER_DIST + no_o_id)

//BPlusTree<int,byte*,8,8> neworder_tree;
HashIndex* newordhi;

char* ORDERS; // | long O_ID | long O_C_ID | long O_D_ID | long O_W_ID
              // | long O_CARRIER_ID | long O_OL_CNT | long O_ALL_LOCAL
              // | char[11] O_ENTRY_D
#define ORDERSkey(o_id, o_d_id, o_w_id)   ((o_w_id*DIST_PER_WARE + o_d_id)*ORD_PER_DIST + o_id)

BPlusTree<int,byte*,8,8> orders_tree;
HashIndex* ordhi; // hashkey is (o_c_id, o_d_id, o_w_id)

char* ORDER_LINE; // | long OL_O_ID | long OL_D_ID | long OL_W_ID | long OL_NUMBER
                  // | long OL_I_ID | long OL_SUPPLY_W_ID | long OL_QUANTITY | float OL_AMOUNT
                  // | char[24] OL_DIST_INFO | char[11] OL_DELIVERY_D 
#define ORDER_LINEkey(ol_o_id, ol_d_id, ol_w_id, ol_number)  (((ol_w_id*DIST_PER_WARE + ol_d_id)*ORD_PER_DIST + ol_o_id)*15 + ol_number)

BPlusTree<int,byte*,8,8> orderline_tree;

TupleBuffer* tb;
TupleBuffer* historyBuffer;
TupleBuffer* hashKeys;

/*==================================================================+
| main()
| ARGUMENTS
| Warehouses n [Debug] [Help]
+==================================================================*/

int localcount=0;
int remotecount=0;
int newordcount=0;

int main(int argc, char** argv) {
  char arg[2];

  /*===========================================+
  EXEC SQL WHENEVER SQLERROR GOTO Error_SqlCall;
  +============================================*/

  count_ware=0;
  for (i=1; i<argc; i++) {

  strncpy(arg,argv[i],2);
  //arg[0] = toupper(arg[0]);

  switch (arg[0]) {
    case 'W': /* Warehouses */
      if (count_ware) {
        printf("Error - Warehouses specified more than once.\n");
        exit(-1);
      }
      if (argc-1>i) {
        i++;
        count_ware=atoi(argv[i]);
        if (count_ware<=0) {
          printf("Invalid Warehouse Count.\n");
          exit(-1);
        }
        if (argc-1>i) {
          i++;
          _core = atoi(argv[i]);
          if (_core!=1 && _core!=2) {
            printf("Currently, only values 1 and 2 are supported for #core.\n");
            exit(-1);
          }
        }
        else {
          _core = 0;
        }
      }
      else {
        printf("Error - Warehouse count must follow Warehouse keyword\n");
        exit(-1);
      }
      break;

    /******* Generic Args *********************/

    case 'D': /* Debug Option */
      if (option_debug) {
        printf("Error - Debug option specified more than once\n");
        exit(-1);
      }
      option_debug=1;
      break;

    case 'H': /* List Args */
      printf("Usage - Warehouses n #core [Debug] [Help]\n");
      exit(0);
      break;

    default :
      printf("Error - Unknown Argument (%s)\n",arg);
      printf("Usage - Warehouses n [Debug] [Help]\n");
      exit(-1);
    }
  }

  if (!(count_ware)) {
    printf("Not enough arguments.\n");
    printf("Usage - Warehouses n ");
    printf(" [Debug] [Help]\n");
    exit(-1);
  }
  
  // SetSeed( time( 0 ) );
  /* Initialize timestamp (for date columns) */
  
  //XXX: diff from TPC-C

  //gettimestamp(timestamp);
  printf( "TPCC Data Load Started...\n" );
  if (_core) {
    printf("\n--- Partitioning for Core #%d ---\n\n", _core);
  }
  //XXX: hack for keeping the lowest neworder id
  lowest_no_id = new long[(count_ware+1)*10 + 1];

  if (!_core) {
    tb = new TupleBuffer(1300000000, 0);
    historyBuffer = new TupleBuffer(200000000, 1);
    hashKeys = new TupleBuffer(50000000, 2);
    custhi = new HashIndex(count_ware*2*30000);
    ordhi = new HashIndex(count_ware*10*30000);
    newordhi = new HashIndex(count_ware*3*30000);
  } 
  else {
    tb = new TupleBuffer(650000000, 0);
    historyBuffer = new TupleBuffer(100000000, 1);
    hashKeys = new TupleBuffer(25000000, 2);
    custhi = new HashIndex(count_ware*30000);
    ordhi = new HashIndex(count_ware*5*30000);
    newordhi = new HashIndex(count_ware*3*15000);
  }
  stockarray = new char*[(count_ware+1)*MAXITEMS+1];

  StringHashFunction* hf = new StringHashFunction();
  custhi->setHashFunction(hf);
  ordhi->setHashFunction(hf);
  newordhi->setHashFunction(hf);
  LoadItems();
  LoadWare();
  LoadCust();
  LoadOrd();
  printf( "\n...DATA LOADING COMPLETED SUCCESSFULLY.\n" );


  if (!_core) {
    int prob;
    int committed = 0;
      int newOrdCommitted = 0;
    
#ifdef USE_PAPI
    for(int i=0; i<200; i++) {
      neword_inst[i] = 0;
      neword_cyc[i] = 0;
      payment_inst[i] = 0;
      payment_cyc[i] = 0;
    }
    
    //+ setting up PAPI
    u = new util();
    u->init_PAPI();
    u->setup_PAPI_event();
    //-
    
    //+ starting PAPI
    u->start_PAPI_event();
    long_long start_usec = u->get_usec();
    long_long start_cyc = u->get_cycle();
    //-
    if (u->eventInfo.size() <= 0)
      cout << "\n PROBLEM \n\n";
    values = new long_long[u->eventInfo.size()];
#endif

    StopWatch* sw = new StopWatch();

    sw->start();
    #ifndef NUM_LOOPS
    #define NUM_LOOPS 8000000
    #endif
    for (i=0; i<NUM_LOOPS; i++) {
      prob = RandomNumber(0, 99);
      //while (prob < 12)
      //  prob = RandomNumber(0, 99);
      if (prob < 4) {
	do_ordstat();
	++committed;
      }
      else if (prob < 8) {
	do_slev();
	++committed;
      }
      else if (prob < 12) {
	do_delivery();
	++committed;
      }
      else if (prob < 55) {
	committed += do_payment();
#ifdef USE_PAPI
	for(int i=0; i<inst-1; i++) {
	  payment_inst[i+1] += inst_count[i+1]-inst_count[i];
	  payment_cyc[i+1] += cyc_count[i+1]-cyc_count[i];
	}
	payment_i = inst -1;
	++payment_completed;
	inst = 0;
#endif        
      }
      else {
	//if (do_neword() != 1) {
	//cout << "Ahhhh!" << endl;
	//exit(1);
	//}
	committed += do_neword();
          ++newOrdCommitted;
	//committed ++;
#ifdef USE_PAPI
	for(int i=0; i<inst-1; i++) {
	  neword_inst[i+1] += inst_count[i+1]-inst_count[i];
	  neword_cyc[i+1] += cyc_count[i+1]-cyc_count[i];
	}
	neword_i = inst -1;
	++neword_completed;
	inst = 0;
#endif        
      }
    }
    
      double elapsed = sw->stop();
      printf("Committed %d transactions, TP = %f\n", committed, committed * 1000.0 / elapsed);
      printf("Committed %d new order transactions, TP = %f\n", newOrdCommitted, newOrdCommitted * 1000.0 / elapsed);
      printf("TPM = %ld\n", (long) (newOrdCommitted * 1000.0 / elapsed));
#ifdef USE_PAPI
    cout << endl << "PAYMENT:" << endl << "Instructions" << endl;
    for(int i=1; i<=payment_i; i++)
      cout << "Chkpoint #" << i << ": " << payment_inst[i]/payment_completed - payment_inst[payment_i]/payment_completed<< " instructions" << endl;
    cout << endl << "Cycles" << endl;
    for(int i=1; i<=payment_i; i++)
      cout << "Chkpoint #" << i << ": " << payment_cyc[i]/payment_completed - payment_cyc[payment_i]/payment_completed << " cycles" << endl;
    cout << endl;
    
    cout << endl << "NEWORDER:" << endl << "Instructions" << endl;
    for(int i=1; i<=neword_i; i++)
      cout << "Chkpoint #" << i << ": " << neword_inst[i]/neword_completed - neword_inst[neword_i]/neword_completed<< " instructions" << endl;
    cout << endl << "Cycles" << endl;
    for(int i=1; i<=neword_i; i++)
      cout << "Chkpoint #" << i << ": " << neword_cyc[i]/neword_completed - neword_cyc[neword_i]/neword_completed << " cycles" << endl;
    cout << endl;
#endif
    
    /*=============================+
      EXEC SQL COMMIT WORK RELEASE;
     +============================*/
  }
  else {
    int committed = 0;
    cout << "Waiting for requests....\n";
    char* toReturn = new char[2*sizeof(int)];
    *((int*)(toReturn)) = sizeof(int);
    char* x = new char[MAXRECV + sizeof(int)];
    char* received;
    int proc;
    ServerSocket* server;
   
    try
      {
	// Create the socket
	if (_core == 2)
	  server = new ServerSocket( 30001 );
	else
	  server = new ServerSocket( 30000 );
	
	while ( true )
	  {
	    
	    ServerSocket new_sock;
	    server->accept ( new_sock );
	    
	    try
	      {
		//sw->start();
		while ( true )
		  {
		    new_sock >> x;
		    committed=0;
		    char* curr = x + sizeof(int);
		    for (int j = 0; j <25; j++) {
		      proc = *(int*)(curr);
		      received = curr + sizeof(int);
		      if (proc == 1) {
			committed += neword(*(long*)received, *(long*)(received+newordoffset1), *(long*)(received+newordoffset2), *(long*)(received+newordoffset3), *(int*)(received+newordoffset4), (long*)(received + newordoffset5), (long*)(received + newordoffset6), (long*)(received + newordoffset7));
		      }
		      else if (proc == 6) {
			committed += neword_LOCAL(*(long*)received, *(long*)(received+newordoffset1), *(long*)(received+newordoffset2), *(long*)(received+newordoffset3), *(int*)(received+newordoffset4), (long*)(received + newordoffset5), (long*)(received + newordoffset6), (long*)(received + newordoffset7));
		      }
		      else if (proc == 7) {
			neword_REMOTE(*(long*)received, *(long*)(received+newordoffset1), *(long*)(received+newordoffset2), *(long*)(received+newordoffset3), *(int*)(received+newordoffset4), (long*)(received + newordoffset5), (long*)(received + newordoffset6), (long*)(received + newordoffset7));
		      }
		      else if (proc == 2) {
			payment(*(long*)received, *(long*)(received+paymentoffset1), *(long*)(received+paymentoffset2), *(long*)(received+paymentoffset3), *(long*)(received+paymentoffset4), *(bool*)(received+paymentoffset5), (received + paymentoffset6), *(int*)(received + paymentoffset7), *(long*)(received + paymentoffset8));
			committed++;
		      }
		      else if (proc == 8) {
			payment_LOCAL(*(long*)received, *(long*)(received+paymentoffset1), *(long*)(received+paymentoffset2), *(long*)(received+paymentoffset3), *(long*)(received+paymentoffset4), *(bool*)(received+paymentoffset5), (received + paymentoffset6), *(int*)(received + paymentoffset7), *(long*)(received + paymentoffset8));
			committed++;
		      }
		      else if (proc == 9) {
			payment_REMOTE(*(long*)received, *(long*)(received+paymentoffset1), *(long*)(received+paymentoffset2), *(long*)(received+paymentoffset3), *(long*)(received+paymentoffset4), *(bool*)(received+paymentoffset5), (received + paymentoffset6), *(int*)(received + paymentoffset7), *(long*)(received + paymentoffset8));
			//committed++;
		      }
		      else if (proc == 3) {
			ordstat(*(long*)received, *(long*)(received+ordstatoffset1), *(long*)(received+ordstatoffset2), *(bool*)(received+ordstatoffset3), (received + ordstatoffset4), *(int*)(received + ordstatoffset5));
			committed++;
		      }
		      else if (proc == 5) {
			slev(*(long*)received, *(long*)(received+sizeof(long)), *(long*)(received+2*sizeof(long)));
			committed++;
		      }
		      else if (proc == 4) {
			delivery(*(long*)received, *(long*)(received+sizeof(long)));
			committed++;
		      }
		      else {
			printf("Error!");
			exit(-1);
		      }
		      curr+=(sizeof(long)*(49) + 2*sizeof(int));
		    }
		    *((int*)(toReturn+sizeof(int))) = committed;
		    //std::cout << data.c_str() << std::endl;
		    //std::cout << committed << " " << ((int*)toReturn)[0] << " " << ((int*)toReturn)[1] << std::endl;
		    new_sock << toReturn;
		    
		    //if (committed == 100000) {
		    //  double elapsed = sw->stop(); 
		    //  printf("Committed %d New Order transactions, TP = %f\n",
		    //          committed, committed * 1000.0 / elapsed); 
		    //  break;
		    //}
		  }
		
	      }
	    catch ( SocketException& ) {}
	    
	  }
      }
    catch ( SocketException& e )
      {
	std::cout << "Exception was caught:" << e.description() << "\nExiting.\n";
      }
  }

  //delete sw;
  delete tb;
  delete historyBuffer;
  delete hashKeys;
  exit( 0 );

  /*====================+
  Error_SqlCall: Error();
  +=====================*/
}


/*==================================================================+
| ROUTINE NAME
| LoadItems
| DESCRIPTION
| Loads the Item table
| ARGUMENTS
| none
+==================================================================*/

void LoadItems() {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long i_id;
  char i_name[24];
  float i_price;
  char i_data[50];
  /*===========================+
  EXEC SQL END DECLARE SECTION;
  +============================*/
  int idatasiz;
  int inamesiz;
  int orig[MAXITEMS];
  long pos;
  int i;
  long table_size = 0;
  //char temp_tuple[100];
  byte* temp_tuple;

  /*===========================+
  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +============================*/
  printf("Loading Item \n");
  for (i=0; i<MAXITEMS; i++)
    orig[i]=0;
  for (i=0; i<MAXITEMS/10; i++) {
    do {
      //pos = RandomNumber(0L,MAXITEMS);
      pos = RandomNumber(0L,MAXITEMS-1);  // XXX: diff from TPC-C
    } while (orig[pos]);
    orig[pos] = 1;
  }

  for (i_id=1; i_id<=MAXITEMS; i_id++) {
    
    /* Generate Item Data */

    inamesiz = MakeAlphaString( 14, 24, i_name);
    i_price=((float) RandomNumber(100L,10000L))/100.0;
    idatasiz=MakeAlphaString(26,50,i_data);
    //if (orig[i_id]) {
    if (orig[i_id-1]) {  // XXX: diff from TPC-C
      pos = RandomNumber(0L,idatasiz-8);
      i_data[pos]='o';
      i_data[pos+1]='r';
      i_data[pos+2]='i';
      i_data[pos+3]='g';
      i_data[pos+4]='i';
      i_data[pos+5]='n';
      i_data[pos+6]='a';
      i_data[pos+7]='l';
    }

    if ( option_debug )
      printf( "IID = %ld, Name= %16s, Price = %5.2f, Data= %16s\n",
              i_id, i_name, i_price, i_data );

    /*==========================================+
    EXEC SQL INSERT INTO
      item (i_id, i_name, i_price, i_data)
      values (:i_id, :i_name, :i_price, :i_data);
    +==========================================*/

    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/
  
    // ITEM tuple:
    //  | long I_ID | long I_IM_ID | float I_PRICE | char offset #1
    //  | char offset #2 | varchar(14-24) I_NAME | varchar(26-50) I_DATA |   
    
    int tuple_size = 2 * sizeof(long) + sizeof(float) + 2 + inamesiz + idatasiz;
    table_size += tuple_size;

    temp_tuple = tb->allocateTuple(tuple_size);

    int offset = 0;
    *((long*)temp_tuple) = i_id;
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = RandomNumber(1L,10000L);
    offset += sizeof(long);
    *((float*)(temp_tuple + offset)) = i_price;   
    offset += sizeof(float);
    *(temp_tuple + offset) = (char)(offset + 2 + inamesiz);
    offset += 1;
    *(temp_tuple + offset) = tuple_size;
    offset += 1;
    memcpy(temp_tuple + offset, i_name, inamesiz);
    offset += inamesiz;
    memcpy(temp_tuple + offset, i_data, idatasiz);
    

    int key = (int) i_id;
    item_tree.insert(key, temp_tuple);


#if 0
    // DEBUG
    printf("IID = %ld, IMID = %ld, IPRICE = %5.2f, INAME = %s, IDATA = %s\n",
           *((long*)temp_tuple),
           *((long*)(temp_tuple + sizeof(long))),
           *((float*)(temp_tuple + 2 * sizeof(long))),
           (temp_tuple + 2 * sizeof(long) + sizeof(float) + 2),
           (temp_tuple + int(*(temp_tuple + 2 * sizeof(long) + sizeof(float)))));

#endif

    if ( !(i_id % 100) ) {
      //printf(".");
      /*==================+
      EXEC SQL COMMIT WORK;
      +===================*/
      if ( !(i_id % 5000) ) {}
        //printf(" %ld\n",i_id);
    }
  }

  /*==================+
  EXEC SQL COMMIT WORK;
  +===================*/
  printf("Item Done. (size in bytes: %ld) \n", table_size);
  return;
  /*==================+
  sqlerr: Error();
  +===================*/
} 


/*==================================================================+
| ROUTINE NAME
| LoadWare
| DESCRIPTION
| Loads the Warehouse table
| Loads Stock, District as Warehouses are created
| ARGUMENTS
| none
+==================================================================*/

void LoadWare() {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long w_id;
  char w_name[10];
  char w_street_1[20];
  char w_street_2[20];
  char w_city[20];
  char w_state[2];
  char w_zip[9];
  float w_tax;
  float w_ytd;
  /*====================================+
  EXEC SQL END DECLARE SECTION;
  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +====================================*/
  long stock_table_size = 0;
  long district_table_size = 0;
  long table_size = 0;
  byte* temp_tuple;

  printf("Loading Warehouse \n");
  printf("Loading Stock ");
  fflush(stdout);

  long start_val = 1L;
  long end_val = count_ware;
  if (_core) {
    if (_core == 1) {
      end_val = count_ware/2;
    }
    else {
      start_val = count_ware/2 + 1;
    }
  }

  for (w_id=start_val; w_id<=end_val; w_id++) {

    /* Generate Warehouse Data */

    int wnamesiz = MakeAlphaString( 6, 10, w_name);
    // MakeAddress( w_street_1, w_street_2, w_city, w_state, w_zip );     
    int wstreet1siz = MakeAlphaString(10, 20, w_street_1); /* Street 1*/
    int wstreet2siz = MakeAlphaString(10, 20, w_street_2); /* Street 2*/
    int wcitysiz = MakeAlphaString(10, 20, w_city); /* City */
    MakeAlphaString(2, 2, w_state); /* State */
    MakeNumberString(9, 9, w_zip); /* Zip */

    w_tax=((float)RandomNumber(10L,20L))/100.0;
    w_ytd=3000000.00;

    if ( option_debug )
      printf( "WID= %ld, Name= %16s, City= %16s, Tax= %5.2f, YTD= %5.2f\n",
               w_id, w_name, w_city, w_tax, w_ytd);

    /*====================================================================+
    EXEC SQL INSERT INTO warehouse (w_id, w_name,
              w_street_1, w_street_2, w_city, w_state, w_zip, w_tax, w_ytd)
            values (:w_id, :w_name, :w_street_1,
              :w_street_2, :w_city, :w_state, :w_zip, :w_tax, :w_ytd);
    +=====================================================================*/

    // WAREHOUSE tuple
    // | long W_ID | float W_TAX | float W_YTD | char[2] W_STATE | char[9] W_ZIP    
    // | char offset #1 |  char offset #2 | char offset #3 | char offset #4
    // | varchar(6-10) W_NAME | varchar(10-20) W_STREET_1
    // | varchar(10-20) W_STREET_2 | varchar(10-20) W_CITY 

    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/
  
    int tuple_size = sizeof(long) + 2*sizeof(float) + 15 + 
                     wnamesiz + wstreet1siz + wstreet2siz + wcitysiz;
    table_size += tuple_size;

    temp_tuple = tb->allocateTuple(tuple_size);

    int offset = 0;
    *((long*)temp_tuple) = w_id;
    offset += sizeof(long);
    *((float*)(temp_tuple + offset)) = w_tax;   
    offset += sizeof(float);
    *((float*)(temp_tuple + offset)) = w_ytd;   
    offset += sizeof(float);
    memcpy(temp_tuple+offset, w_state, 2);
    offset += 2;
    memcpy(temp_tuple+offset, w_zip, 9);
    offset += 9;
    *(temp_tuple+offset) = (char)(offset + 4 + wnamesiz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 3 + wnamesiz + wstreet1siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 2 + wnamesiz + wstreet1siz + wstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + wnamesiz + wstreet1siz + wstreet2siz + wcitysiz);
    offset += 1;
    memcpy(temp_tuple+offset, w_name, wnamesiz);
    offset += wnamesiz;
    memcpy(temp_tuple+offset, w_street_1, wstreet1siz);
    offset += wstreet1siz;
    memcpy(temp_tuple+offset, w_street_2, wstreet2siz);
    offset += wstreet2siz;
    memcpy(temp_tuple+offset, w_city, wcitysiz);


    // KEY: long w_id
    int key = (int) w_id;
    warehouse_tree.insert(key, temp_tuple);

#if 0
   //DEBUG
   printf("WID= %ld, Name= %16s, City= %16s, Tax= %5.2f, YTD= %5.2f\n",
   *((long*)temp_tuple),
   (temp_tuple + sizeof(long) + 2*sizeof(float) + 15),
   (temp_tuple + (int)(*(temp_tuple + sizeof(long) + 2*sizeof(float) + 13))),
   *((float*)(temp_tuple + sizeof(long))),
   *((float*)(temp_tuple + sizeof(long) + sizeof(float))));
#endif

    /** Make Rows associated with Warehouse **/

    //stock_table_size += Stock(w_id);
    district_table_size += District(w_id);
    printf(".");
    fflush(stdout);

    /*==================+
    EXEC SQL COMMIT WORK;
    +===================*/
  }

  // For dual-core instances, we replicate Stock  

  for (w_id=1L; w_id<=count_ware; w_id++) {
    stock_table_size += Stock(w_id);
  }


  printf("\nWarehouse Done. (size in bytes: %ld) \n", table_size);
  printf("Stock Done. (size in bytes: %ld) \n", stock_table_size);
  printf("District Done. (size in bytes: %ld) \n", district_table_size);
  return;
  /*==================+
  sqlerr: Error();
  +===================*/
}


/*==================================================================+
| ROUTINE NAME
| LoadCust
| DESCRIPTION
| Loads the Customer Table
| ARGUMENTS
| none
+==================================================================*/

void LoadCust() {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  EXEC SQL END DECLARE SECTION;
  +============================*/
  
  long w_id;
  long d_id;
  long table_size = 0;

  /*====================================+
  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +=====================================*/

  printf("Loading Customer ");
  fflush(stdout);

  long start_val = 1L;
  long end_val = count_ware;
  if (_core) {
    if (_core == 1) {
      end_val = count_ware/2;
    }
    else {
      start_val = count_ware/2 + 1;
    }
  }

  for (w_id=start_val; w_id<=end_val; w_id++) {
    for (d_id=1L; d_id<=DIST_PER_WARE; d_id++)
      table_size += Customer(d_id,w_id);
    printf(".");
    fflush(stdout);
  }

  /*====================================+
  EXEC SQL COMMIT WORK; 
  +=====================================*/
  printf("\nCustomer Done. (size in bytes: %ld) \n", table_size);
  printf("History Done. (size in bytes: %ld) \n", h_table_size);
  return;
  /*=============+
  sqlerr: Error();
  +=============*/
}

/*==================================================================+
| ROUTINE NAME
| LoadOrd
| DESCRIPTION
| Loads the Orders and Order_Line Tables
| ARGUMENTS
| none
+==================================================================*/

void LoadOrd() {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long w_id;
  float w_tax;
  long d_id;
  float d_tax;
  /*====================================+
  EXEC SQL END DECLARE SECTION;
  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +=====================================*/

  printf("Loading Orders ");
  fflush(stdout);

  long start_val = 1L;
  long end_val = count_ware;
  if (_core) {
    if (_core == 1) {
      end_val = count_ware/2;
    }
    else {
      start_val = count_ware/2 + 1;
    }
  }

  for (w_id=start_val; w_id<=end_val; w_id++) {
    for (d_id=1L; d_id<=DIST_PER_WARE; d_id++)
      Orders(d_id, w_id);
    printf(".");
    fflush(stdout);
  }

  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  printf("\nOrders Done. (size in bytes: %ld) \n", o_table_size);
  printf("New Orders Done. (size in bytes: %ld) \n", no_table_size);
  printf("Order-lines Done. (size in bytes: %ld) \n", ol_table_size);
  return;
  /*=============+
  sqlerr: Error();
  +=============*/
}


/*==================================================================+
| ROUTINE NAME
| Stock
| DESCRIPTION
| Loads the Stock table
| ARGUMENTS
| w_id - warehouse id
+==================================================================*/

long Stock(long w_id) {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long s_i_id;
  long s_w_id;
  long s_quantity;
  char s_dist_01[24];
  char s_dist_02[24];
  char s_dist_03[24];
  char s_dist_04[24];
  char s_dist_05[24];
  char s_dist_06[24];
  char s_dist_07[24];
  char s_dist_08[24];
  char s_dist_09[24];
  char s_dist_10[24];
  char s_data[50];
  /*===========================+
  EXEC SQL END DECLARE SECTION;
  +============================*/
  int sdatasiz;
  long orig[MAXITEMS];
  long pos;
  int i;
  long table_size = 0;
  byte* temp_tuple;
  /*====================================+
  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +====================================*/
  //printf("Loading Stock Wid=%ld\n",w_id);
  s_w_id=w_id;

  for (i=0; i<MAXITEMS; i++)
    orig[i]=0;
  for (i=0; i<MAXITEMS/10; i++) {
    do {
      //pos=RandomNumber(0L,MAXITEMS); 
      pos=RandomNumber(0L,MAXITEMS-1); // XXX: diff from TPC-C
    } while (orig[pos]);
    orig[pos] = 1;
  }

  for (s_i_id=1; s_i_id<=MAXITEMS; s_i_id++) {

    /* Generate Stock Data */
    
    s_quantity=RandomNumber(10L,100L);
    MakeAlphaString(24,24,s_dist_01);
    MakeAlphaString(24,24,s_dist_02);
    MakeAlphaString(24,24,s_dist_03);
    MakeAlphaString(24,24,s_dist_04);
    MakeAlphaString(24,24,s_dist_05);
    MakeAlphaString(24,24,s_dist_06);
    MakeAlphaString(24,24,s_dist_07);
    MakeAlphaString(24,24,s_dist_08);
    MakeAlphaString(24,24,s_dist_09);
    MakeAlphaString(24,24,s_dist_10);
    sdatasiz=MakeAlphaString(26,50,s_data);
    
    //if (orig[s_i_id]) {
    if (orig[s_i_id - 1]) { // XXX: diff from TPC-C
      pos=RandomNumber(0L,sdatasiz-8);
      s_data[pos]='o';
      s_data[pos+1]='r';
      s_data[pos+2]='i';
      s_data[pos+3]='g';
      s_data[pos+4]='i';
      s_data[pos+5]='n';
      s_data[pos+6]='a';
      s_data[pos+7]='l';
    }
 
    /*==================================================================+
    EXEC SQL INSERT INTO
      stock (s_i_id, s_w_id, s_quantity,
             s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
             s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10,
             s_data, s_ytd, s_cnt_order, s_cnt_remote)
      values (:s_i_id, :s_w_id, :s_quantity,
              :s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
              :s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10,
              :s_data, 0, 0, 0);
    +==================================================================*/

    if ( option_debug )
      printf( "SID = %ld, WID = %ld, Quan = %ld, Data = %16s\n", s_i_id, s_w_id, s_quantity, s_data );

    // STOCK tuple
    // | long S_I_ID | long S_W_ID | long S_QUANTITY
    // | float S_YTD | int S_CNT_ORDER | int S_CNT_REMOTE
    // | char[24] S_DIST_01 | char[24] S_DIST_02 | char[24] S_DIST_03 | char[24] S_DIST_04
    // | char[24] S_DIST_05 | char[24] S_DIST_06 | char[24] S_DIST_07 | char[24] S_DIST_08
    // | char[24] S_DIST_09 | char[24] S_DIST_10 | int offset | varchar(26-50) S_DATA 
  
    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/
  
    int tuple_size = 3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240 + sdatasiz; 
    table_size += tuple_size;

    temp_tuple = tb->allocateTuple(tuple_size);

    int offset = 0;
    *((long*)temp_tuple) = s_i_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = s_w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = s_quantity;
    offset += sizeof(long);
    *((float*)(temp_tuple + offset)) = 0.0;   
    offset += sizeof(float);
    *((int*)(temp_tuple+offset)) = 0;
    offset += sizeof(int);
    *((int*)(temp_tuple+offset)) = 0;
    offset += sizeof(int);

    memcpy(temp_tuple+offset, s_dist_01, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_02, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_03, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_04, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_05, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_06, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_07, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_08, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_09, 24);
    offset += 24;
    memcpy(temp_tuple+offset, s_dist_10, 24);
    offset += 24;

    *((int*)(temp_tuple+offset)) = offset + sizeof(int) + sdatasiz;
    offset += sizeof(int);
    memcpy(temp_tuple+offset, s_data, sdatasiz);

    //int key = STOCKkey(s_i_id, s_w_id);
    //stock_tree.insert(key, temp_tuple);
#if 0
    int hashKeySize=2*sizeof(long);
    byte* hashkey = hashKeys->allocateTuple(hashKeySize);
    // hashkey is (s_i_id, s_w_id)
    memcpy(hashkey,temp_tuple, 2*sizeof(long));
    stockhi->put(hashkey,hashKeySize,temp_tuple, tuple_size);
#endif
    stockarray[(int)STOCKkey(s_i_id, s_w_id)] = (char*)temp_tuple;

#if 0
    //DEBUG
    printf("SID = %ld, WID = %ld, Quan = %ld, Data = %16s\n",
           *((long*)(temp_tuple)), 
           *((long*)(temp_tuple+sizeof(long))), 
           *((long*)(temp_tuple+2*sizeof(long))), 
           (temp_tuple+3*sizeof(long)+ sizeof(float) + 3*sizeof(int) + 240)); 
#endif

    if ( !(s_i_id % 100) ) {
      /*==================+
      EXEC SQL COMMIT WORK;
      +===================*/
      // printf(".");
      if ( !(s_i_id % 5000) ) {}
        // printf(" %ld\n",s_i_id);
    }
  }

  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  //printf(" Stock Done.\n");
  return table_size;
  /*==================+
  sqlerr: Error();
  +==================*/
}


/*==================================================================+
| ROUTINE NAME
| District
| DESCRIPTION
| Loads the District table
| ARGUMENTS
| w_id - warehouse id
+==================================================================*/

long District(long w_id) {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long d_id;
  long d_w_id;
  char d_name[10];
  char d_street_1[20];
  char d_street_2[20];
  char d_city[20];
  char d_state[2];
  char d_zip[9];
  float d_tax;
  float d_ytd;
  long d_next_o_id;
  /*===========================+
  EXEC SQL END DECLARE SECTION;

  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +============================*/

  long table_size = 0;
  byte* temp_tuple;
 
  //printf("Loading District\n");
  d_w_id=w_id;
  d_ytd=30000.0;
  d_next_o_id=3001L;

  for (d_id=1; d_id<=DIST_PER_WARE; d_id++) {

    /* Generate District Data */
   
    int dnamesiz = MakeAlphaString(6,10,d_name);
    //MakeAddress( d_street_1, d_street_2, d_city, d_state, d_zip );     
    int dstreet1siz = MakeAlphaString(10, 20, d_street_1); /* Street 1*/
    int dstreet2siz = MakeAlphaString(10, 20, d_street_2); /* Street 2*/
    int dcitysiz = MakeAlphaString(10, 20, d_city); /* City */
    MakeAlphaString(2, 2, d_state); /* State */
    MakeNumberString(9, 9, d_zip); /* Zip */

    d_tax=((float)RandomNumber(10L,20L))/100.0;

    /*=========================================================+
    EXEC SQL INSERT INTO
      district (d_id, d_w_id, d_name,
                d_street_1, d_street_2, d_city, d_state, d_zip,
                d_tax, d_ytd, d_next_o_id)
      values (:d_id, :d_w_id, :d_name,
              :d_street_1, :d_street_2, :d_city, :d_state, :d_zip,
              :d_tax, :d_ytd, :d_next_o_id);
    +=========================================================*/
    
    if ( option_debug )
      printf( "DID = %ld, WID = %ld, Name = %10s, City= %16s, Tax = %5.2f, YTD= %5.2f\n",
               d_id, d_w_id, d_name, d_city, d_tax, d_ytd);

    // DISTRICT tuple
    // | long D_ID | long D_W_ID | float D_TAX | float D_YTD | long D_NEXT_O_ID
    // | char[2] D_STATE | char[9] D_ZIP
    // | char offset #1 |  char offset #2 | char offset #3 | char offset #4
    // | varchar(6-10) D_NAME | varchar(10-20) D_STREET_1
    // | varchar(10-20) D_STREET_2 | varchar(10-20) D_CITY

    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/

    int tuple_size = 3*sizeof(long) + 2*sizeof(float) + 15 +
                     dnamesiz + dstreet1siz + dstreet2siz + dcitysiz;
    table_size += tuple_size;

    temp_tuple = tb->allocateTuple(tuple_size);

    int offset = 0;
    *((long*)temp_tuple) = d_id;
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = d_w_id;
    offset += sizeof(long);
    *((float*)(temp_tuple + offset)) = d_tax;
    offset += sizeof(float);
    *((float*)(temp_tuple + offset)) = d_ytd;
    offset += sizeof(float);
    *((long*)(temp_tuple + offset)) = d_next_o_id;
    offset += sizeof(long);
    memcpy(temp_tuple+offset, d_state, 2);
    offset += 2;
    memcpy(temp_tuple+offset, d_zip, 9);
    offset += 9;
    *(temp_tuple+offset) = (char)(offset + 4 + dnamesiz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 3 + dnamesiz + dstreet1siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 2 + dnamesiz + dstreet1siz + dstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + dnamesiz + dstreet1siz + dstreet2siz + dcitysiz);
    offset += 1;
    memcpy(temp_tuple+offset, d_name, dnamesiz);
    offset += dnamesiz;
    memcpy(temp_tuple+offset, d_street_1, dstreet1siz);
    offset += dstreet1siz;
    memcpy(temp_tuple+offset, d_street_2, dstreet2siz);
    offset += dstreet2siz;
    memcpy(temp_tuple+offset, d_city, dcitysiz);

    int key = DISTRICTkey(d_id, d_w_id);
    district_tree.insert(key, temp_tuple);

#if 0
   //DEBUG
   printf( "DID = %ld, WID = %ld, Name = %10s, City= %16s, Tax = %5.2f, YTD= %5.2f\n",
   *((long*)temp_tuple),
   *((long*)(temp_tuple + sizeof(long))),
   (temp_tuple + 3*sizeof(long) + 2*sizeof(float) + 15),
   (temp_tuple + (int)(*(temp_tuple + 3*sizeof(long) + 2*sizeof(float) + 13))),
   *((float*)(temp_tuple + 2*sizeof(long))),
   *((float*)(temp_tuple + 2*sizeof(long) + sizeof(float))));
#endif

  }

  /*===========================+
  EXEC SQL COMMIT WORK;
  +===========================*/

  return table_size;
  /*=============+
  sqlerr: Error();
  +=============*/
}


/*==================================================================+
| ROUTINE NAME
| Customer
| DESCRIPTION
| Loads Customer Table
| Also inserts corresponding history record
| ARGUMENTS
| id - customer id
| d_id - district id
| w_id - warehouse id
+==================================================================*/

long Customer(long d_id, long w_id) { 
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long c_id;
  long c_d_id;
  long c_w_id;
  char c_first[FIRSTNAME_LEN];
  char c_middle[MIDDLE_LEN];
  char c_last[LASTNAME_LEN];
  char c_street_1[20];
  char c_street_2[20];
  char c_city[20];
  char c_state[2];
  char c_zip[9];
  char c_phone[16];
  char c_since[11];
  char c_credit[2];
  long c_credit_lim;
  float c_discount;
  float c_balance;
  char c_data[500];
  float h_amount;
  char h_data[24];
  /*===========================+
  EXEC SQL END DECLARE SECTION;

  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +============================*/

  long table_size = 0;
  byte* temp_tuple;

  //printf("Loading Customer for DID=%ld, WID=%ld\n",d_id,w_id);

  for (c_id=1; c_id<=CUST_PER_DIST; c_id++) {

    /* Generate Customer Data as per TPC-C 4.3.3.1 */

    c_d_id=d_id;
    c_w_id=w_id;
    int cfirstsiz = MakeAlphaString(FIRSTNAME_MINLEN, sizeof(c_first), c_first);
    c_middle[0]='O';
    c_middle[1]='E';
    int clastsiz;
    if (c_id <= 1000)
      clastsiz = Lastname(c_id-1,c_last);
    else
      clastsiz = Lastname(NURand(255,0,999),c_last);
    //MakeAddress( c_street_1, c_street_2, c_city, c_state, c_zip );
    int cstreet1siz = MakeAlphaString(10, 20, c_street_1); /* Street 1*/
    int cstreet2siz = MakeAlphaString(10, 20, c_street_2); /* Street 2*/
    int ccitysiz = MakeAlphaString(10, 20, c_city); /* City */
    MakeAlphaString(2, 2, c_state); /* State */
    MakeNumberString(9, 9, c_zip); /* Zip */

    MakeNumberString( 16, 16, c_phone );
    //TODO add timestamp here
    memcpy(c_since, "2007-01-01:", 11);
    if (RandomNumber(0L,1L))
      c_credit[0]='G';
    else
      c_credit[0]='B';
    c_credit[1]='C';
    c_credit_lim=50000;
    c_discount=((float)RandomNumber(0L,50L))/100.0;
    c_balance=-10.0;
    int cdatasiz = MakeAlphaString(300,500,c_data);

    /*=====================================================================+
    EXEC SQL INSERT INTO
      customer (c_id, c_d_id, c_w_id,
                c_first, c_middle, c_last,
                c_street_1, c_street_2, c_city, c_state, c_zip,
                c_phone, c_since, c_credit,
                c_credit_lim, c_discount, c_balance, c_data,
                c_ytd_payment, c_cnt_payment, c_cnt_delivery)
      values (:c_id, :c_d_id, :c_w_id,
              :c_first, :c_middle, :c_last,
              :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
              :c_phone, :timestamp, :c_credit,
              :c_credit_lim, :c_discount, :c_balance, :c_data, 10.0, 1, 0) ;
    +=====================================================================*/

    if ( option_debug ) {
      char temp_phone[16];
      memcpy(temp_phone, c_phone, 16); 
      temp_phone[15] = '\0';
      printf( "CID = %ld, LST = %s, P# = %s\n", c_id, c_last, temp_phone );
    }

    // CUSTOMER tuple
    // | long C_ID | long C_D_ID | long C_W_ID 
    // | long C_CREDIT_LIM | float C_DISCOUNT | float C_BALANCE
    // | float C_YTD_PAYMENT | long C_CNT_PAYMENT | long C_CNT_DELIVERY
    // | char[2] C_MIDDLE | char[2] C_STATE | char[9] C_ZIP
    // | char[16] C_PHONE | char[11] C_SINCE | char[2] C_CREDIT
    // | char offset #1 | char offset #2 | char offset #3
    // | char offset #4 | char offset #5 | int offset #6
    // | varchar(8-16) C_FIRST | varchar(10-16) C_LAST
    // | varchar(10-20) C_STREET_1 | varchar(10-20) C_STREET_2 | varchar(10-20) C_CITY
    // | varchar(300-500) C_DATA

    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/

    int tuple_size = 6*sizeof(long) + 3*sizeof(float) + 47 + sizeof(int) +
                     cfirstsiz + clastsiz + cstreet1siz + cstreet2siz + ccitysiz + cdatasiz;
    table_size += tuple_size;

    temp_tuple = tb->allocateTuple(tuple_size);

    int offset = 0;
    *((long*)temp_tuple) = c_id;
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = c_d_id;
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = c_w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = c_credit_lim;
    offset += sizeof(long);
    *((float*)(temp_tuple + offset)) = c_discount;
    offset += sizeof(float);   
    *((float*)(temp_tuple + offset)) = c_balance;
    offset += sizeof(float);   
    *((float*)(temp_tuple + offset)) = 10.0; // C_YTD_PAYMENT
    offset += sizeof(float);   
    *((long*)(temp_tuple + offset)) = 1; // C_CNT_PAYMENT
    offset += sizeof(long);
    *((long*)(temp_tuple + offset)) = 0; // C_CNT_DELIVERY
    offset += sizeof(long);
    memcpy(temp_tuple+offset, c_middle, MIDDLE_LEN);
    offset += 2;
    memcpy(temp_tuple+offset, c_state, 2);
    offset += 2;
    memcpy(temp_tuple+offset, c_zip, 9);
    offset += 9;
    memcpy(temp_tuple+offset, c_phone, 16);
    offset += 16;
    memcpy(temp_tuple+offset, c_since, 11);
    offset += 11;
    memcpy(temp_tuple+offset, c_credit, 2);
    offset += 2;
    *(temp_tuple+offset) = (char)(offset + 5 + sizeof(int) + cfirstsiz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 4 + sizeof(int) + cfirstsiz + clastsiz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 3 + sizeof(int) + cfirstsiz + clastsiz + cstreet1siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 2 + sizeof(int) + cfirstsiz + clastsiz + cstreet1siz
                                                           + cstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + sizeof(int) + cfirstsiz + clastsiz + cstreet1siz
                                                           + cstreet2siz + ccitysiz);
    offset += 1;
    *((int*)(temp_tuple+offset)) = offset + sizeof(int) + cfirstsiz + clastsiz + cstreet1siz
                                                + cstreet2siz + ccitysiz + cdatasiz;
    offset += sizeof(int);
    memcpy(temp_tuple+offset, c_first, cfirstsiz);
    offset += cfirstsiz;
    memcpy(temp_tuple+offset, c_last, clastsiz);
    offset += clastsiz;
    memcpy(temp_tuple+offset, c_street_1, cstreet1siz);
    offset += cstreet1siz;
    memcpy(temp_tuple+offset, c_street_2, cstreet2siz);
    offset += cstreet2siz;
    memcpy(temp_tuple+offset, c_city, ccitysiz);
    offset += ccitysiz;
    memcpy(temp_tuple+offset, c_data, cdatasiz);

    int key = CUSTOMERkey(c_id, c_d_id, c_w_id);
    customer_tree.insert(key, temp_tuple);

    int hashKeySize=clastsiz+sizeof(long)+sizeof(long);
    byte* hashkey = hashKeys->allocateTuple(hashKeySize);
    memcpy(hashkey,c_last,clastsiz);
    memcpy(hashkey+clastsiz,temp_tuple + sizeof(long),2*sizeof(long));

    custhi->put(hashkey,hashKeySize,temp_tuple, tuple_size);

#if 0
    //DEBUG
    int offs1 = (int)( *(temp_tuple + 6*sizeof(long) + 3*sizeof(float) + 42));
    int offs2 = (int)( *(temp_tuple + 6*sizeof(long) + 3*sizeof(float) + 43));
    char temp_last[LASTNAME_LEN];
    memcpy(temp_last, temp_tuple+offs1, offs2-offs1);
    if (offs2-offs1 < LASTNAME_LEN)
      temp_last[offs2-offs1] = '\0'; 
    else
      temp_last[LASTNAME_LEN-1] = '\0';
    char temp_phone[16];
    memcpy(temp_phone, temp_tuple+6*sizeof(long) + 3*sizeof(float) + 13, 16);
    temp_phone[15] = '\0';
    printf("CID = %ld, LST = %s, P# = %s\n",
           *((long*)temp_tuple),
           temp_last,
           temp_phone);
#endif

    h_amount=10.0;
    int hdatasiz = MakeAlphaString(12,24,h_data);
      

    /*=================================================================+
    EXEC SQL INSERT INTO
      history (h_c_id, h_c_d_id, h_c_w_id,
               h_w_id,  h_d_id, h_date, h_amount, h_data)
      values (:c_id, :c_d_id, :c_w_id,
              :c_w_id, :c_d_id, :timestamp, :h_amount, :h_data);
    +=================================================================*/

    // HISTORY tuple
    // | long H_C_ID | long H_C_D_ID | long H_C_W_ID
    // | long H_W_ID | long H_D_ID | float H_AMOUNT
    // | char[11] H_DATE | char offset | varchar(12-24) H_DATA

    /*==========================================+
    Horizontica INSERT INTO
    +==========================================*/

    int h_tuple_size = 5*sizeof(long) + sizeof(float) + 12 + hdatasiz;
    h_table_size += h_tuple_size;

    byte* h_temp_tuple = historyBuffer->allocateTuple(h_tuple_size);

    offset = 0;
    *((long*)h_temp_tuple) = c_id;
    offset += sizeof(long);
    *((long*)(h_temp_tuple + offset)) = c_d_id;
    offset += sizeof(long);
    *((long*)(h_temp_tuple + offset)) = c_w_id;
    offset += sizeof(long);
    *((long*)(h_temp_tuple + offset)) = c_w_id;
    offset += sizeof(long);
    *((long*)(h_temp_tuple + offset)) = c_d_id;
    offset += sizeof(long);
    *((float*)(h_temp_tuple + offset)) = h_amount;
    offset += sizeof(float);
    //TODO add proper timestamp
    memcpy(h_temp_tuple+offset, c_since, 11);
    offset += 11;
    *(h_temp_tuple+offset) = (char)(offset + 1 + hdatasiz);
    offset += 1;
    memcpy(h_temp_tuple+offset, h_data, hdatasiz);
    
    //Do not insert into B tree since no unique key

    if ( option_debug ) {
      char temp_data[25];
      memcpy(temp_data, h_data, hdatasiz);
      temp_data[hdatasiz] = '\0';
      printf( "HCID = %ld, DATA = %s\n", c_id, temp_data );
    }

#if 0
    //DEBUG
    int offs1 = 5*sizeof(long) + sizeof(float) + 12;
    int offs2 = (int)( *(h_temp_tuple + 5*sizeof(long) + sizeof(float) + 11));
    char temp_data[25];
    memcpy(temp_data, h_temp_tuple+offs1, offs2-offs1);
    temp_data[offs2-offs1] = '\0';
    printf( "HCID = %ld, DATA = %s\n", *((long*)h_temp_tuple), temp_data );
#endif

    if ( !(c_id % 100) ) {
      /*==================+
      EXEC SQL COMMIT WORK;
      +==================*/
      //printf(".");
      //if ( !(c_id % 1000) )
      //  printf(" %ld\n",c_id);
    }
  }
  //printf("Customer Done.\n");
  return table_size; 
  /*==================+
  sqlerr: Error();
  +==================*/
}


/*==================================================================+
| ROUTINE NAME
| Orders
| DESCRIPTION
| Loads the Orders table
| Also loads the Order_Line table on the fly
| ARGUMENTS
| w_id - warehouse id
+==================================================================*/
void Orders(long d_id, long w_id) {
  /*===========================+
  EXEC SQL BEGIN DECLARE SECTION;
  +============================*/
  long o_id;
  long o_c_id;
  long o_d_id;
  long o_w_id;
  long o_carrier_id;
  long o_ol_cnt;
  long ol;
  long ol_i_id;
  long ol_supply_w_id;
  long ol_quantity;
  float ol_amount;
  char ol_dist_info[24];
  float i_price;
  float c_discount;
  /*====================================+
  EXEC SQL END DECLARE SECTION;

  EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
  +=====================================*/

  byte* temp_tuple;
  int offset = 0;

  //printf("Loading Orders for D=%ld, W= %ld\n", d_id, w_id);
  o_d_id=d_id;
  o_w_id=w_id;
  InitPermutation(); /* initialize permutation of customer numbers */

  // XXX: hack
  lowest_no_id[(int)(w_id*10 + d_id)] = 2101;

  for (o_id=1; o_id<=ORD_PER_DIST; o_id++) {

    /* Generate Order Data */

    o_c_id=GetPermutation();
    o_carrier_id=RandomNumber(1L,10L);
    o_ol_cnt=RandomNumber(5L,15L);
    if (o_id > 2100) { /* the last 900 orders have not been delivered) */ 
      /*======================================================+
       EXEC SQL INSERT INTO
        orders (o_id, o_c_id, o_d_id, o_w_id,
                o_entry_d, o_carrier_id, o_ol_cnt, o_all_local)
        values (:o_id, :o_c_id, :o_d_id, :o_w_id,
                :timestamp, NULL, :o_ol_cnt, 1);
      EXEC SQL INSERT INTO
        new_order (no_o_id, no_d_id, no_w_id)
        values (:o_id, :o_d_id, :o_w_id);
       +======================================================*/
      
       // XXX: hack: we represent NULL as 0
       o_carrier_id = 0;

       // NEW_ORDER tuple
       // | long NO_O_ID | long NO_D_ID | long NO_W_ID

       /*==========================================+
         Horizontica INSERT INTO
       +==========================================*/
      
       int no_tuple_size = 3 * sizeof(long);
       no_table_size += no_tuple_size;  

       temp_tuple = tb->allocateTuple(no_tuple_size);

       offset = 0;
       *((long*)temp_tuple) = o_id;
       offset += sizeof(long);
       *((long*)(temp_tuple+offset)) = o_d_id;
       offset += sizeof(long);
       *((long*)(temp_tuple+offset)) = o_w_id;

       //int no_key = NEW_ORDERkey(o_id, o_d_id, o_w_id);
       //neworder_tree.insert(no_key, temp_tuple);

       int hashKeySize = 3*sizeof(long);
       byte* hashkey = hashKeys->allocateTuple(hashKeySize);
       //hashkey is (o_id, o_d_id, o_w_id)
       memcpy(hashkey,temp_tuple, 3*sizeof(long));
       newordhi->put(hashkey,hashKeySize,temp_tuple, no_tuple_size);
    }
    else {}
      /*=======================================================+
      EXEC SQL INSERT INTO
      orders (o_id, o_c_id, o_d_id, o_w_id,
              o_entry_d, o_carrier_id, o_ol_cnt, o_all_local)
      values (:o_id, :o_c_id, :o_d_id, :o_w_id,
              :timestamp, :o_carrier_id, :o_ol_cnt, 1);
      +========================================================*/

    // ORDERS tuple
    // | long O_ID | long O_C_ID | long O_D_ID | long O_W_ID
    // | long O_CARRIER_ID | long O_OL_CNT | long O_ALL_LOCAL
    // | char[11] O_ENTRY_D

    /*==========================================+
      Horizontica INSERT INTO
    +==========================================*/

    int o_tuple_size = 7*sizeof(long) + 11;
    o_table_size += o_tuple_size;

    temp_tuple = tb->allocateTuple(o_tuple_size);

    offset = 0;
    *((long*)temp_tuple) = o_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = o_c_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = o_d_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = o_w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = o_carrier_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = o_ol_cnt;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = 1;
    offset += sizeof(long);
    //TODO add timestamp here
    memcpy(temp_tuple+offset, "2007-01-01:", 11);

    int o_key = ORDERSkey(o_id, o_d_id, o_w_id);
    orders_tree.insert(o_key, temp_tuple);

    int hashKeySize=3*sizeof(long);
    byte* hashkey = hashKeys->allocateTuple(hashKeySize);
    // hashkey is (o_c_id, o_d_id, o_w_id)
    memcpy(hashkey,temp_tuple + sizeof(long), 3*sizeof(long));

    ordhi->put(hashkey,hashKeySize,temp_tuple, o_tuple_size);

    if ( option_debug )
      printf( "OID = %ld, CID = %ld, DID = %ld, WID = %ld\n",
               o_id, o_c_id, o_d_id, o_w_id);
    
    for (ol=1; ol<=o_ol_cnt; ol++) {

      /* Generate Order Line Data */

      ol_i_id=RandomNumber(1L,MAXITEMS);
      ol_supply_w_id=o_w_id;
      ol_quantity=5;
      ol_amount=0.0; 
      MakeAlphaString(24,24,ol_dist_info);
      char ol_delivery_d[11];
      //TODO add timestamp here
      memcpy(ol_delivery_d, "2007-01-01:", 11);

      if (o_id > 2100) {
        /*===========================================================+
        EXEC SQL INSERT INTO
          order_line (ol_o_id, ol_d_id, ol_w_id, ol_number,
                      ol_i_id, ol_supply_w_id, ol_quantity, ol_amount,
                      ol_dist_info, ol_delivery_d)
          values (:o_id, :o_d_id, :o_w_id, :ol,
                  :ol_i_id, :ol_supply_w_id, :ol_quantity, :ol_amount,
                  :ol_dist_info, NULL);
        +============================================================*/

        // XXX hack: we represent NULL timestamp as "0"
        ol_delivery_d[0] = 0;  
        ol_delivery_d[1] = '\0';  
      }
      else {
        /*============================================================+
        EXEC SQL INSERT INTO
          order_line (ol_o_id, ol_d_id, ol_w_id, ol_number,
                      ol_i_id, ol_supply_w_id, ol_quantity,
                      ((float)(RandomNumber(10L, 10000L)))/100.0,
                      ol_dist_info, ol_delivery_d)
          values (:o_id, :o_d_id, :o_w_id, :ol,
                  :ol_i_id, :ol_supply_w_id, :ol_quantity,
                  :ol_amount, :ol_dist_info, datetime);
         +============================================================*/
        
        ol_amount = ((float)(RandomNumber(10L, 10000L)))/100.0;
      }

      // ORDER_LINE tuple
      // | long OL_O_ID | long OL_D_ID | long OL_W_ID | long OL_NUMBER
      // | long OL_I_ID | long OL_SUPPLY_W_ID | long OL_QUANTITY | float OL_AMOUNT
      // | char[24] OL_DIST_INFO | char[11] OL_DELIVERY_D 
 
      /*==========================================+
        Horizontica INSERT INTO
      +==========================================*/
  
      int ol_tuple_size = 7*sizeof(long) + sizeof(float) + 24 + 11;
      ol_table_size += ol_tuple_size;

      temp_tuple = tb->allocateTuple(ol_tuple_size);

      offset = 0;
      *((long*)temp_tuple) = o_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = o_d_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = o_w_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = ol;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = ol_i_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = ol_supply_w_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = ol_quantity;
      offset += sizeof(long);
      *((float*)(temp_tuple+offset)) = ol_amount;
      offset += sizeof(float);
      memcpy(temp_tuple+offset, ol_dist_info, 24);
      offset += 24;
      memcpy(temp_tuple+offset, ol_delivery_d, 11);

      int ol_key = ORDER_LINEkey(o_id, o_d_id, o_w_id, ol);
      orderline_tree.insert(ol_key, temp_tuple);



      if ( option_debug )
        printf( "OL = %ld, IID = %ld, QUAN = %ld, AMT = %8.2f\n",
                 ol, ol_i_id, ol_quantity, ol_amount);
    }

    if ( !(o_id % 100) ) {
      //printf(".");
      /*=====================+
      EXEC SQL COMMIT WORK;
      +=====================*/
      if ( !(o_id % 1000) ) {}
        //printf(" %ld\n",o_id);
    }
  }
  /*=====================+
  EXEC SQL COMMIT WORK;
  +=====================*/
  //printf("Orders Done.\n");
  return;
  /*=============+
  sqlerr: Error();
  +=============*/
}

/*==================================================================+
| ROUTINE NAME
| do_neword()
+==================================================================*/

int do_neword() {

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
  if (stavros)
    ol_cnt = 10;

  // simulate 1% of transactions with erroneous entry
  rbk = (int) RandomNumber(1, 100);
  if (stavros)
    rbk=42;
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
    if(stavros || RandomNumber(1,100)!=1) {
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
  int status = 0;
  if (!remote) { // SINGLE-SITE
    status = neword(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
  }
  else { // MULTI_SITE
    status = neword_LOCAL(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
    status = neword_REMOTE(w_id, d_id, c_id, ol_cnt, all_local, item_id, supware, quantity);
  }
  return status;
}

/*==================================================================+
| ROUTINE NAME
| do_payment()
+==================================================================*/

int do_payment() {

  long w_id;
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
  char c_last[LASTNAME_LEN];
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

#ifdef USE_PAPI
  y=65;
#endif
  if (stavros)
    y=65;
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

  int status = 1;
  if (!remote) { // SINGLE-SITE
    payment(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
  }
  else { // MULTI-SITE
    payment_LOCAL(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
    payment_REMOTE(w_id, d_id, c_id, c_w_id, c_d_id, byname, c_last, clastsiz, h_amount);
  }
  return status;
}


/*==================================================================+
| ROUTINE NAME
| do_ordstat()
+==================================================================*/

void do_ordstat() {

  long w_id;
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

  char c_last[LASTNAME_LEN];
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

  ordstat(w_id, d_id, c_id, byname, c_last, clastsiz);
  return;
}


/*==================================================================+
| ROUTINE NAME
| do_delivery()
+==================================================================*/

void do_delivery() {
  long w_id;
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

  delivery(w_id, o_carrier_id);
  return;
}


/*==================================================================+
| ROUTINE NAME
| do_slev()
+==================================================================*/

void do_slev() {
  long w_id;
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

  slev(w_id, d_id, threshold);
  return;
}


/*==================================================================+
| ROUTINE NAME
| neword()
+==================================================================*/

int neword(long w_id, long d_id, long c_id, long ol_cnt,
            int all_local, long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;
  read_inst();
  //newordcount++;
  //if (newordcount % 20 == 0)
  //  std::cout << newordcount << std::endl;

  //std::cout << w_id << " " << d_id << " " << c_id << " " << ol_cnt << " " << all_local << " " << item_id[0] << " " << supware[0] << " " << quantity[0] << std::endl;

  ////////////////////////////////////////
  //
  // 2-phase New Order Xaction:
  // first we check if all items are valid

  byte** items = new byte*[15];
  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {

    /*===========================================+
    EXEC SQL WHENEVER NOT FOUND GOTO invaliditem;
    EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
    +===========================================*/
  
    if(!item_tree.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return 0;
    }
  }
   
  ///////////////////////////////////////
  //
  // 2nd phase of New Order
  // all items were valid


  /*=======================================================================+
  EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
    INTO :c_discount, :c_last, :c_credit, :w_tax
    FROM customer, warehouse
    WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
  +========================================================================*/
  
  char* w_tuple;
  if (!warehouse_tree.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 1!%d\n", w_id);
    exit(-1);
  }
  float w_tax = *((float*)(w_tuple + sizeof(long)));

  char* c_tuple;
  if (!customer_tree.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
    printf("Lookup error 2!\n");
    exit(-1);
  }
  // c_last and c_credit are not used in this Xaction
  float c_discount = *((float*)(c_tuple + 4 * sizeof(long)));
  

  /*==================================================+
  EXEC SQL SELECT d_next_o_id, d_tax
    INTO :d_next_o_id, :d_tax
    FROM district WHERE d_id = :d_id AND d_w_id = :w_id;
  +===================================================*/

  char* d_tuple;
  if (!district_tree.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 3!\n");
    exit(-1);
  }
  long d_next_o_id = *((long*)(d_tuple + 2 * sizeof(long) + 2 *sizeof(float)));
  float d_tax = *((float*)(d_tuple + 2 * sizeof(long)));


  /*=====================================+
  EXEC SQL UPDATE district
    SET d_next_o_id = :d_next_o_id + 1
    WHERE d_id = :d_id AND d_w_id = :w_id;
  +=====================================*/

  *((long*)(d_tuple + 2 * sizeof(long) + 2 *sizeof(float))) = d_next_o_id + 1;

  long o_id=d_next_o_id;
 

  /*========================================================================================+
  EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
    VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
  +========================================================================================*/

  int o_tuple_size = 7*sizeof(long) + 11;
  byte* temp_tuple = tb->allocateTuple(o_tuple_size);
  int offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = c_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = 0; // o_carrier_id
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = ol_cnt;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = all_local;
  offset += sizeof(long);
  //TODO add timestamp here
  memcpy(temp_tuple+offset, "2007-01-01:", 11);

  int o_key = ORDERSkey(o_id, d_id, w_id);
  orders_tree.insert(o_key, temp_tuple);

  int hashKeySize=3*sizeof(long);
  byte* hashkey = hashKeys->allocateTuple(hashKeySize);
  // hashkey is (o_c_id, o_d_id, o_w_id)
  memcpy(hashkey,temp_tuple + sizeof(long), 3*sizeof(long));

  ordhi->put(hashkey,hashKeySize,temp_tuple, o_tuple_size);

  //XXX: diff from Ben's code; he does not have the following insert
  /*=======================================================+
  EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
  +=======================================================*/

  int no_tuple_size = 3 * sizeof(long);
  temp_tuple = tb->allocateTuple(no_tuple_size);
  offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  //int no_key = NEW_ORDERkey(o_id, d_id, w_id);
  //neworder_tree.insert(no_key, temp_tuple);

  hashKeySize = 3*sizeof(long);
  hashkey = hashKeys->allocateTuple(hashKeySize);
  // hashkey is (o_id, o_d_id, o_w_id)
  memcpy(hashkey,temp_tuple, 3*sizeof(long));
  newordhi->put(hashkey,hashKeySize,temp_tuple, no_tuple_size);

  //XXX: hack
  if (lowest_no_id[(int)(w_id*10+d_id)] < 0)
    lowest_no_id[(int)(w_id*10+d_id)] = o_id;

  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {
     
    long ol_supply_w_id = supware[ol_number-1];
    // XXX: the following is not used anywhere else
    // if(ol_supply_w_id != w_id) all_local = 0;
    long ol_i_id = item_id[ol_number-1];
    long ol_quantity = quantity[ol_number-1];


    /*===========================================+
    EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
    +===========================================*/

    // We have already performed the above retrievals.
    // We just need to read the i_price value.
    // XXX: we don't use i_name (it's used only for printing)

    float i_price = *((float*)(items[ol_number-1] + 2 * sizeof(long)));
    char i_data[51]; 
    int idatasiz = (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 1))
                 - (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float)));
    memcpy(i_data, items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 2, idatasiz);
    i_data[idatasiz] = '\0';

    /*===================================================================+
    EXEC SQL SELECT s_quantity, s_data,
                    s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
                    s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
      INTO :s_quantity, :s_data,
           :s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
           :s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
      FROM stock
      WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;
    +===================================================================*/

    char* s_tuple;
    s_tuple = stockarray[(int)STOCKkey(ol_i_id, ol_supply_w_id)];  

    long s_quantity = *((long*)(s_tuple + 2*sizeof(long)));
    int sdatasiz = *((int*)(s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + 240))
                   - (3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240);
    char s_data[51];
    memcpy(s_data, s_tuple + 3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240, sdatasiz);
    s_data[sdatasiz] = '\0';
    char* ol_dist_info = s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + (d_id-1)*24;

    if ((strstr(i_data, "original") != NULL &&
        strstr(s_data, "original") != NULL))
      bg[ol_number-1] = 'B';
    else
      bg[ol_number-1] = 'G';

    if(s_quantity > ol_quantity)
      s_quantity = s_quantity - ol_quantity;
    else
      s_quantity = s_quantity - ol_quantity + 91;

    /*===============================================+
    EXEC SQL UPDATE stock SET s_quantity = :s_quantity
      WHERE s_i_id = :ol_i_id
      AND s_w_id = :ol_supply_w_id;
    +===============================================*/

    *((long*)(s_tuple + 2*sizeof(long))) = s_quantity;

    int ol_amount = (int)(ol_quantity * i_price * (100 + w_tax + d_tax)
                                      * (100 - c_discount)) / (100*100);
    total += ol_amount;

    /*====================================================+
    EXEC SQL INSERT
      INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_number,
                      ol_i_id, ol_supply_w_id,
                      ol_quantity, ol_amount, ol_dist_info)
      VALUES(:o_id, :d_id, :w_id, :ol_number,
             :ol_i_id, :ol_supply_w_id,
             :ol_quantity, :ol_amount, :ol_dist_info);
    +====================================================*/


    int ol_tuple_size = 7*sizeof(long) + sizeof(float) + 24 + 11;
    ol_table_size += ol_tuple_size;
    temp_tuple = tb->allocateTuple(ol_tuple_size);
    offset = 0;
    *((long*)temp_tuple) = o_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = d_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_number;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_i_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_supply_w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_quantity;
    offset += sizeof(long);
    *((float*)(temp_tuple+offset)) = ol_amount;
    offset += sizeof(float);
    memcpy(temp_tuple+offset, ol_dist_info, 24);
    offset += 24;
    // XXX hack: we represent NULL timestamp as "0"
    char ol_delivery_d[11];
    ol_delivery_d[0] = 0;
    ol_delivery_d[1] = '\0';
    memcpy(temp_tuple+offset, ol_delivery_d, 11);

    int ol_key = ORDER_LINEkey(o_id, d_id, w_id, ol_number);
    orderline_tree.insert(ol_key, temp_tuple);

  }
  read_inst();
  read_inst();
  /*==================+
  EXEC SQL COMMIT WORK
  +==================*/
  return 1;
}


/*==================================================================+
| ROUTINE NAME
| neword_LOCAL()
+==================================================================*/

int neword_LOCAL(long w_id, long d_id, long c_id, long ol_cnt,
            int all_local, long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;

  //localcount++;
  //if (localcount % 20 == 0)
  //std::cout << localcount << " " << remotecount << std::endl;

  ////////////////////////////////////////
  //
  // 2-phase New Order Xaction:
  // first we check if all items are valid

  byte** items = new byte*[15];
  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {

    /*===========================================+
    EXEC SQL WHENEVER NOT FOUND GOTO invaliditem;
    EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
    +===========================================*/
  
    if(!item_tree.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return 0;
    }
  }
   
  ///////////////////////////////////////
  //
  // 2nd phase of New Order
  // all items were valid


  /*=======================================================================+
  EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
    INTO :c_discount, :c_last, :c_credit, :w_tax
    FROM customer, warehouse
    WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
  +========================================================================*/
  
  char* w_tuple;
  if (!warehouse_tree.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 4!\n");
    exit(-1);
  }
  float w_tax = *((float*)(w_tuple + sizeof(long)));

  char* c_tuple;
  if (!customer_tree.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
    printf("Lookup error 5!\n");
    exit(-1);
  }
  // c_last and c_credit are not used in this Xaction
  float c_discount = *((float*)(c_tuple + 4 * sizeof(long)));
  

  /*==================================================+
  EXEC SQL SELECT d_next_o_id, d_tax
    INTO :d_next_o_id, :d_tax
    FROM district WHERE d_id = :d_id AND d_w_id = :w_id;
  +===================================================*/

  char* d_tuple;
  if (!district_tree.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 6!\n");
    exit(-1);
  }
  long d_next_o_id = *((long*)(d_tuple + 2 * sizeof(long) + 2 *sizeof(float)));
  float d_tax = *((float*)(d_tuple + 2 * sizeof(long)));


  /*=====================================+
  EXEC SQL UPDATE district
    SET d_next_o_id = :d_next_o_id + 1
    WHERE d_id = :d_id AND d_w_id = :w_id;
  +=====================================*/

  *((long*)(d_tuple + 2 * sizeof(long) + 2 *sizeof(float))) = d_next_o_id + 1;

  long o_id=d_next_o_id;
 

  /*========================================================================================+
  EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
    VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
  +========================================================================================*/

  int o_tuple_size = 7*sizeof(long) + 11;
  byte* temp_tuple = tb->allocateTuple(o_tuple_size);
  int offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = c_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = 0; // o_carrier_id
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = ol_cnt;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = all_local;
  offset += sizeof(long);
  //TODO add timestamp here
  memcpy(temp_tuple+offset, "2007-01-01:", 11);

  int o_key = ORDERSkey(o_id, d_id, w_id);
  orders_tree.insert(o_key, temp_tuple);

  int hashKeySize=3*sizeof(long);
  byte* hashkey = hashKeys->allocateTuple(hashKeySize);
  // hashkey is (o_c_id, o_d_id, o_w_id)
  memcpy(hashkey,temp_tuple + sizeof(long), 3*sizeof(long));

  ordhi->put(hashkey,hashKeySize,temp_tuple, o_tuple_size);

  //XXX: diff from Ben's code; he does not have the following insert
  /*=======================================================+
  EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
  +=======================================================*/

  int no_tuple_size = 3 * sizeof(long);
  temp_tuple = tb->allocateTuple(no_tuple_size);
  offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  //int no_key = NEW_ORDERkey(o_id, d_id, w_id);
  //neworder_tree.insert(no_key, temp_tuple);

  hashKeySize = 3*sizeof(long);
  hashkey = hashKeys->allocateTuple(hashKeySize);
  // hashkey is (o_id, o_d_id, o_w_id)
  memcpy(hashkey,temp_tuple, 3*sizeof(long));
  newordhi->put(hashkey,hashKeySize,temp_tuple, no_tuple_size);

  //XXX: hack
  if (lowest_no_id[(int)(w_id*10+d_id)] < 0)
    lowest_no_id[(int)(w_id*10+d_id)] = o_id;

  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {
     
    long ol_supply_w_id = supware[ol_number-1];
    // XXX: the following is not used anywhere else
    // if(ol_supply_w_id != w_id) all_local = 0;
    long ol_i_id = item_id[ol_number-1];
    long ol_quantity = quantity[ol_number-1];


    /*===========================================+
    EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
    +===========================================*/

    // We have already performed the above retrievals.
    // We just need to read the i_price value.
    // XXX: we don't use i_name (it's used only for printing)

    float i_price = *((float*)(items[ol_number-1] + 2 * sizeof(long)));
    char i_data[51]; 
    int idatasiz = (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 1))
                 - (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float)));
    memcpy(i_data, items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 2, idatasiz);
    i_data[idatasiz] = '\0';

    /*===================================================================+
    EXEC SQL SELECT s_quantity, s_data,
                    s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
                    s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
      INTO :s_quantity, :s_data,
           :s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
           :s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
      FROM stock
      WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;
    +===================================================================*/

    char* s_tuple;
    s_tuple = stockarray[(int)STOCKkey(ol_i_id, ol_supply_w_id)];  

    char* ol_dist_info = s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + (d_id-1)*24;

    //////////////////////////////////////////////////////////
    //                                                      // 
    // Local Version of New-Order:                          //
    // Update on Stock is executed only if it is local      //  
    //                                                      //
    //////////////////////////////////////////////////////////
 
    bool remote = false;
    if (_core == 1) {
      if (ol_supply_w_id > count_ware/2)
        remote = true;
    }
    if (_core == 2) {
      if (ol_supply_w_id <=count_ware/2)
        remote = true;
    }

    if (!remote) {

      long s_quantity = *((long*)(s_tuple + 2*sizeof(long)));
      int sdatasiz = *((int*)(s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + 240))
                     - (3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240);
      char s_data[51];
      memcpy(s_data, s_tuple + 3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240, sdatasiz);
      s_data[sdatasiz] = '\0';
      if ((strstr(i_data, "original") != NULL &&
          strstr(s_data, "original") != NULL))
        bg[ol_number-1] = 'B';
      else
        bg[ol_number-1] = 'G';

      if(s_quantity > ol_quantity)
        s_quantity = s_quantity - ol_quantity;
      else
        s_quantity = s_quantity - ol_quantity + 91;

      /*===============================================+
      EXEC SQL UPDATE stock SET s_quantity = :s_quantity
        WHERE s_i_id = :ol_i_id
        AND s_w_id = :ol_supply_w_id;
      +===============================================*/

      *((long*)(s_tuple + 2*sizeof(long))) = s_quantity;

    }


    int ol_amount = (int)(ol_quantity * i_price * (100 + w_tax + d_tax)
                                      * (100 - c_discount)) / (100*100);
    total += ol_amount;

    /*====================================================+
    EXEC SQL INSERT
      INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_number,
                      ol_i_id, ol_supply_w_id,
                      ol_quantity, ol_amount, ol_dist_info)
      VALUES(:o_id, :d_id, :w_id, :ol_number,
             :ol_i_id, :ol_supply_w_id,
             :ol_quantity, :ol_amount, :ol_dist_info);
    +====================================================*/


    int ol_tuple_size = 7*sizeof(long) + sizeof(float) + 24 + 11;
    ol_table_size += ol_tuple_size;
    temp_tuple = tb->allocateTuple(ol_tuple_size);
    offset = 0;
    *((long*)temp_tuple) = o_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = d_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_number;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_i_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_supply_w_id;
    offset += sizeof(long);
    *((long*)(temp_tuple+offset)) = ol_quantity;
    offset += sizeof(long);
    *((float*)(temp_tuple+offset)) = ol_amount;
    offset += sizeof(float);
    memcpy(temp_tuple+offset, ol_dist_info, 24);
    offset += 24;
    // XXX hack: we represent NULL timestamp as "0"
    char ol_delivery_d[11];
    ol_delivery_d[0] = 0;
    ol_delivery_d[1] = '\0';
    memcpy(temp_tuple+offset, ol_delivery_d, 11);

    int ol_key = ORDER_LINEkey(o_id, d_id, w_id, ol_number);
    orderline_tree.insert(ol_key, temp_tuple);

  }

  /*==================+
  EXEC SQL COMMIT WORK
  +==================*/
  return 1;
}



/*==================================================================+
| ROUTINE NAME
| neword_REMOTE()
+==================================================================*/

int neword_REMOTE(long w_id, long d_id, long c_id, long ol_cnt,
            int all_local, long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;
  //remotecount++;
  //if (remotecount % 20 == 0)
  //std::cout << localcount << " " << remotecount << std::endl;

  ////////////////////////////////////////
  //
  // 2-phase New Order Xaction:
  // first we check if all items are valid

  byte** items = new byte*[15];
  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {

    /*===========================================+
    EXEC SQL WHENEVER NOT FOUND GOTO invaliditem;
    EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
    +===========================================*/
  
    if(!item_tree.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return 0;
    }
  }
   
  ///////////////////////////////////////
  //
  // 2nd phase of New Order
  // all items were valid


  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {
     
    long ol_supply_w_id = supware[ol_number-1];
    long ol_i_id = item_id[ol_number-1];
    long ol_quantity = quantity[ol_number-1];


    //////////////////////////////////////////////////////////
    //                                                      // 
    // Remote Version of New-Order:                         //
    // Remote updates on Stock                              //  
    //                                                      //
    //////////////////////////////////////////////////////////

    bool remote = false;
    if (_core == 1) {
      if (ol_supply_w_id > count_ware/2)
        remote = true;
    }
    if (_core == 2) {
      if (ol_supply_w_id <=count_ware/2)
        remote = true;
    }


    if (remote) {

      char* s_tuple;
      s_tuple = stockarray[(int)STOCKkey(ol_i_id, ol_supply_w_id)];  

      long s_quantity = *((long*)(s_tuple + 2*sizeof(long)));
      int sdatasiz = *((int*)(s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + 240))
                     - (3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240);
      char s_data[51];
      memcpy(s_data, s_tuple + 3*sizeof(long) + sizeof(float) + 3*sizeof(int) + 240, sdatasiz);
      s_data[sdatasiz] = '\0';

      // We have already performed ITEM retrievals.
      // We just need to read the i_data value.
      char i_data[51];
      int idatasiz = (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 1))
                   - (int)(*(items[ol_number-1] + 2 * sizeof(long) + sizeof(float)));
      memcpy(i_data, items[ol_number-1] + 2 * sizeof(long) + sizeof(float) + 2, idatasiz);
      i_data[idatasiz] = '\0';

      if ((strstr(i_data, "original") != NULL &&
          strstr(s_data, "original") != NULL))
        bg[ol_number-1] = 'B';
      else
        bg[ol_number-1] = 'G';

      if(s_quantity > ol_quantity)
        s_quantity = s_quantity - ol_quantity;
      else
        s_quantity = s_quantity - ol_quantity + 91;

      /*===============================================+
      EXEC SQL UPDATE stock SET s_quantity = :s_quantity
        WHERE s_i_id = :ol_i_id
        AND s_w_id = :ol_supply_w_id;
      +===============================================*/

      *((long*)(s_tuple + 2*sizeof(long))) = s_quantity;

    }

  }

  /*==================+
  EXEC SQL COMMIT WORK
  +==================*/
  return 1;
}

// Concatenates a last name, c_d_id and c_w_id into buffer. Returns the bytes copied into buffer.
int concat_customer(char* buffer, const char* c_last, int c_last_len, long c_d_id, long c_w_id) {
    assert(c_last_len <= LASTNAME_LEN);
    memcpy(buffer, c_last, c_last_len);
    int length = c_last_len;

    memcpy(buffer+length, &c_d_id, sizeof(c_d_id));
    length += sizeof(c_d_id);

    memcpy(buffer+length, &c_w_id, sizeof(c_w_id));
    length += sizeof(c_w_id);
    return length;
}


/*==================================================================+
| ROUTINE NAME
| payment()
+==================================================================*/

void payment(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount) {

  /*====================================================+
  EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
  WHERE w_id=:w_id;
  +====================================================*/

  //std::cout << w_id << " " << d_id << " " << c_id << " " << c_w_id << " " << c_d_id << " " << byname << " " << c_last[0] << " " << clastsiz << " " << h_amount << std::endl;

read_inst();
  char* w_tuple;
  if (!warehouse_tree.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 7!\n");
    exit(-1);
  }

  //read_inst();
  float w_ytd = *((float*)(w_tuple + sizeof(long) + sizeof(float)));
  *((float*)(w_tuple + sizeof(long) + sizeof(float))) = w_ytd + (float)h_amount;
 
  /*===================================================================+
  EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
    INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
    FROM warehouse
    WHERE w_id=:w_id;
  +===================================================================*/

  // XXX: only the name of the Warehouse is used later on

  char w_name[11];
  int wnamesiz = (int)(*(w_tuple + sizeof(long) + 2*sizeof(float) + 11))
                -(sizeof(long) + 2*sizeof(float) + 15);
  memcpy(w_name, w_tuple + sizeof(long) + 2*sizeof(float) + 15, wnamesiz);
  w_name[wnamesiz] = '\0';

  /*=====================================================+
  EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
    WHERE d_w_id=:w_id AND d_id=:d_id;
  +=====================================================*/

  char* d_tuple;

  //read_inst();
  if (!district_tree.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 8!\n");
    exit(-1);
  }
  //read_inst();
  float d_ytd = *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float)));
  *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float))) = d_ytd + (float)h_amount;


  /*====================================================================+
  EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
    INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
    FROM district
    WHERE d_w_id=:w_id AND d_id=:d_id;
  +====================================================================*/

  // XXX: only the name of the District is used later on

  char d_name[11];
  int dnamesiz = (int)(*(d_tuple + 3*sizeof(long) + 2*sizeof(float) + 11))
                -(3*sizeof(long) + 2*sizeof(float) + 15);
  memcpy(d_name, d_tuple + 3*sizeof(long) + 2*sizeof(float) + 15, dnamesiz);
  d_name[dnamesiz] = '\0';

  char* c_tuple;

  if (byname) {
    //assert(false);
    /*==========================================================+
    EXEC SQL SELECT count(c_id) INTO :namecnt
      FROM customer
      WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
    +==========================================================*/

    /*==========================================================================+
    EXEC SQL DECLARE c_byname CURSOR FOR
    SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state,
           c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_last=:c_last
      ORDER BY c_first;

    EXEC SQL OPEN c_byname;
    +===========================================================================*/
 
    // here we assume we are not going to have more than 300 duplicates
    char* c_tuples[300];
    char cust_temp[LASTNAME_LEN + sizeof(c_d_id) + sizeof(c_w_id)];
    int key_length = concat_customer(cust_temp, c_last, clastsiz, c_d_id, c_w_id);
    assert(key_length <= sizeof(cust_temp));
    if (!(c_tuples[0]=(char*)custhi->get((byte*)cust_temp, key_length))) {
      printf("Lookup error in secondary index (payment)!\n");
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++) 
      if(!(c_tuples[namecnt]=(char*)custhi->getAgain()))
        break;
    if (namecnt == 300)
      printf("Warning: more than 300 duplicate last names -- increase buffer!\n");

    /*============================================================================+    
    for (n=0; n<namecnt/2; n++) {
      EXEC SQL FETCH c_byname
        INTO :c_first, :c_middle, :c_id,
             :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
             :c_phone, :c_credit, :c_credit_lim, :c_discount, :c_balance, :c_since; 
    }
    EXEC SQL CLOSE c_byname;
    +=============================================================================*/

    // XXX: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  }
  else { 

    /*=====================================================================+
    EXEC SQL SELECT c_first, c_middle, c_last, c_street_1, c_street_2,
                    c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim,
                    c_discount, c_balance, c_since
      INTO :c_first, :c_middle, :c_last, :c_street_1, :c_street_2,
           :c_city, :c_state, :c_zip, :c_phone, :c_credit, :c_credit_lim,
           :c_discount, :c_balance, :c_since
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    +======================================================================*/
    //read_inst();
    if (!customer_tree.find((int)CUSTOMERkey(c_id, c_d_id, c_w_id), (byte**)&c_tuple)) {
      printf("Lookup error 10!\n");
      exit(-1);
    }
    //read_inst();

  } // if by name

  float c_balance = *((float*)(c_tuple + 4*sizeof(long) + sizeof(float)));
  char c_credit[3];
  memcpy(c_credit, c_tuple + 6*sizeof(long) + 3*sizeof(float) + 40, 2); 
  c_balance += h_amount;
  c_credit[2]='\0';

  if (false) {//(strstr(c_credit, "ZC") ) {
    
    /*=====================================================+
    EXEC SQL SELECT c_data
      INTO :c_data
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    +=====================================================*/

    char c_new_data[501];
    sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f",
                          c_id, c_d_id, c_w_id, d_id, w_id, h_amount);
    int cdatasiz = *((int*)(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 47))
                  -(int)((unsigned char)(*(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 46)));
    char* c_data = c_tuple + (int)((unsigned char)(*(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 46)));

    strncat(c_new_data, c_data, cdatasiz - strlen(c_new_data));

    //sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f %12c %24c",
    //       c_id,c_d_id,c_w_id,d_id,w_id,h_amount h_date, h_data);
    //strncat(c_new_data,c_data,500-strlen(c_new_data));

    /*======================================================================+
    EXEC SQL UPDATE customer SET c_balance = :c_balance, c_data = :c_new_data
      WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
    +======================================================================*/
 
    memcpy(c_data, c_new_data, cdatasiz);
  }
  else {

    /*============================================================+
    EXEC SQL UPDATE customer SET c_balance = :c_balance
      WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
    +=============================================================*/

    *((float*)(c_tuple + 4*sizeof(long) + sizeof(float))) = c_balance;   
  }
  //read_inst();
  char h_data[24];
  strncpy(h_data,w_name,10);
  h_data[10]='\0';
  strncat(h_data,d_name,10);
  h_data[20]=' ';
  h_data[21]=' ';
  h_data[22]=' ';
  h_data[23]=' ';

  /*=============================================================================+
  EXEC SQL INSERT INTO
    history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
    VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
  +=============================================================================*/

  // | long H_C_ID | long H_C_D_ID | long H_C_W_ID
  // | long H_W_ID | long H_D_ID | float H_AMOUNT
  // | char[11] H_DATE | char offset | varchar(12-24) H_DATA

  int hdatasiz = 24;
  int h_tuple_size = 5*sizeof(long) + sizeof(float) + 12 + hdatasiz;
  byte* h_temp_tuple = historyBuffer->allocateTuple(h_tuple_size);

  int offset = 0;
  *((long*)h_temp_tuple) = c_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = c_d_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = c_w_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = w_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = d_id;
  offset += sizeof(long);
  *((float*)(h_temp_tuple + offset)) = h_amount;
  offset += sizeof(float);
  //TODO add proper timestamp
  memcpy(h_temp_tuple+offset, "2007-01-01:", 11);
  offset += 11;
  *(h_temp_tuple+offset) = (char)(offset + 1 + hdatasiz);
  offset += 1;
  memcpy(h_temp_tuple+offset, h_data, hdatasiz);
read_inst();
read_inst();
  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  return;
}

/*==================================================================+
| ROUTINE NAME
| payment_LOCAL()
+==================================================================*/

void payment_LOCAL(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount) {

  /*====================================================+
  EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
  WHERE w_id=:w_id;
  +====================================================*/

  char* w_tuple;
  if (!warehouse_tree.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 11!\n");
    exit(-1);
  }
  float w_ytd = *((float*)(w_tuple + sizeof(long) + sizeof(float)));
  *((float*)(w_tuple + sizeof(long) + sizeof(float))) = w_ytd + (float)h_amount;
 
  /*===================================================================+
  EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
    INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
    FROM warehouse
    WHERE w_id=:w_id;
  +===================================================================*/

  // XXX: only the name of the Warehouse is used later on

  char w_name[11];
  int wnamesiz = (int)(*(w_tuple + sizeof(long) + 2*sizeof(float) + 11))
                -(sizeof(long) + 2*sizeof(float) + 15);
  memcpy(w_name, w_tuple + sizeof(long) + 2*sizeof(float) + 15, wnamesiz);
  w_name[wnamesiz] = '\0';

  /*=====================================================+
  EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
    WHERE d_w_id=:w_id AND d_id=:d_id;
  +=====================================================*/

  char* d_tuple;
  if (!district_tree.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 12!\n");
    exit(-1);
  }
  float d_ytd = *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float)));
  *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float))) = d_ytd + (float)h_amount;


  /*====================================================================+
  EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
    INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
    FROM district
    WHERE d_w_id=:w_id AND d_id=:d_id;
  +====================================================================*/

  // XXX: only the name of the District is used later on

  char d_name[11];
  int dnamesiz = (int)(*(d_tuple + 3*sizeof(long) + 2*sizeof(float) + 11))
                -(3*sizeof(long) + 2*sizeof(float) + 15);
  memcpy(d_name, d_tuple + 3*sizeof(long) + 2*sizeof(float) + 15, dnamesiz);
  d_name[dnamesiz] = '\0';

  /////////////////////////////////////////////////////////
  //                                                     //  
  // Update on Customer is executed at the remote site   //
  //                                                     //
  /////////////////////////////////////////////////////////

  char h_data[24];
  strncpy(h_data,w_name,10);
  h_data[10]='\0';
  strncat(h_data,d_name,10);
  h_data[20]=' ';
  h_data[21]=' ';
  h_data[22]=' ';
  h_data[23]=' ';

  /*=============================================================================+
  EXEC SQL INSERT INTO
    history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
    VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
  +=============================================================================*/

  // | long H_C_ID | long H_C_D_ID | long H_C_W_ID
  // | long H_W_ID | long H_D_ID | float H_AMOUNT
  // | char[11] H_DATE | char offset | varchar(12-24) H_DATA

  int hdatasiz = 24;
  int h_tuple_size = 5*sizeof(long) + sizeof(float) + 12 + hdatasiz;
  byte* h_temp_tuple = historyBuffer->allocateTuple(h_tuple_size);

  int offset = 0;
  *((long*)h_temp_tuple) = c_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = c_d_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = c_w_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = w_id;
  offset += sizeof(long);
  *((long*)(h_temp_tuple + offset)) = d_id;
  offset += sizeof(long);
  *((float*)(h_temp_tuple + offset)) = h_amount;
  offset += sizeof(float);
  //TODO add proper timestamp
  memcpy(h_temp_tuple+offset, "2007-01-01:", 11);
  offset += 11;
  *(h_temp_tuple+offset) = (char)(offset + 1 + hdatasiz);
  offset += 1;
  memcpy(h_temp_tuple+offset, h_data, hdatasiz);

  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  return;
}


/*==================================================================+
| ROUTINE NAME
| payment_REMOTE()
+==================================================================*/

void payment_REMOTE(long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
             bool byname, const char* c_last, int clastsiz, long h_amount) {


  /////////////////////////////////////////////////////////////////////
  //                                                                 //  
  // Updates on Warehouse, District are executed at the local site   //
  //                                                                 //
  /////////////////////////////////////////////////////////////////////


  char* c_tuple;

  if (byname) {

    /*==========================================================+
    EXEC SQL SELECT count(c_id) INTO :namecnt
      FROM customer
      WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
    +==========================================================*/

    /*==========================================================================+
    EXEC SQL DECLARE c_byname CURSOR FOR
    SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state,
           c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_last=:c_last
      ORDER BY c_first;

    EXEC SQL OPEN c_byname;
    +===========================================================================*/
 
    // here we assume we are not going to have more than 300 duplicates
    char* c_tuples[300];
    char cust_temp[LASTNAME_LEN + sizeof(c_d_id) + sizeof(c_w_id)];
    int key_length = concat_customer(cust_temp, c_last, clastsiz, c_d_id, c_w_id);
    assert(key_length <= sizeof(cust_temp));
    if (!(c_tuples[0]=(char*)custhi->get((byte*)c_last, key_length))) {
      printf("Lookup error in secondary index (payment remote)!\n");
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++) 
      if(!(c_tuples[namecnt]=(char*)custhi->getAgain()))
        break;
    if (namecnt == 300)
      printf("Warning: more than 300 duplicate last names -- increase buffer!\n");

    /*============================================================================+    
    for (n=0; n<namecnt/2; n++) {
      EXEC SQL FETCH c_byname
        INTO :c_first, :c_middle, :c_id,
             :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
             :c_phone, :c_credit, :c_credit_lim, :c_discount, :c_balance, :c_since; 
    }
    EXEC SQL CLOSE c_byname;
    +=============================================================================*/

    // XXX: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  }
  else { 

    /*=====================================================================+
    EXEC SQL SELECT c_first, c_middle, c_last, c_street_1, c_street_2,
                    c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim,
                    c_discount, c_balance, c_since
      INTO :c_first, :c_middle, :c_last, :c_street_1, :c_street_2,
           :c_city, :c_state, :c_zip, :c_phone, :c_credit, :c_credit_lim,
           :c_discount, :c_balance, :c_since
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    +======================================================================*/

    if (!customer_tree.find((int)CUSTOMERkey(c_id, c_d_id, c_w_id), (byte**)&c_tuple)) {
      printf("Lookup error 14!\n");
      exit(-1);
    }

  } // if by name

  float c_balance = *((float*)(c_tuple + 4*sizeof(long) + sizeof(float)));
  char c_credit[3];
  memcpy(c_credit, c_tuple + 6*sizeof(long) + 3*sizeof(float) + 40, 2); 
  c_balance += h_amount;
  c_credit[2]='\0';

  if (strstr(c_credit, "BC") ) {
    
    /*=====================================================+
    EXEC SQL SELECT c_data
      INTO :c_data
      FROM customer
      WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    +=====================================================*/

    char c_new_data[501];
    sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f",
                          c_id, c_d_id, c_w_id, d_id, w_id, h_amount);
    int cdatasiz = *((int*)(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 47))
                  -(int)((unsigned char)(*(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 46)));
    char* c_data = c_tuple + (int)((unsigned char)(*(c_tuple + 6*sizeof(long) + 3*sizeof(float) + 46)));

    strncat(c_new_data, c_data, cdatasiz - strlen(c_new_data));

    //sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f %12c %24c",
    //       c_id,c_d_id,c_w_id,d_id,w_id,h_amount h_date, h_data);
    //strncat(c_new_data,c_data,500-strlen(c_new_data));

    /*======================================================================+
    EXEC SQL UPDATE customer SET c_balance = :c_balance, c_data = :c_new_data
      WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
    +======================================================================*/
 
    memcpy(c_data, c_new_data, cdatasiz);
  }
  else {

    /*============================================================+
    EXEC SQL UPDATE customer SET c_balance = :c_balance
      WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
    +=============================================================*/

    *((float*)(c_tuple + 4*sizeof(long) + sizeof(float))) = c_balance;   
  }


  /////////////////////////////////////////////////////////
  //                                                     //  
  // Insert in History is executed at the local site     //
  //                                                     //
  /////////////////////////////////////////////////////////


  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  return;
}



/*==================================================================+
| ROUTINE NAME
| ordstat()
+==================================================================*/

void ordstat(long w_id, long d_id, long c_id,
             bool byname, const char* c_last, int clastsiz) {
  char* c_tuple;

  /*
  std::cout << w_id << " " << d_id << " " << c_id << " " << byname << " ";
  for (int i = 0; i < clastsiz; i++) {
    if (c_last[i]) {
      std::cout << c_last[i];
    }
    else {
      break;
    }
  }
  
  std::cout << " " << clastsiz << " " << _core << std::endl;
  */

  if (byname) {

    /*==========================================================+
    EXEC SQL SELECT count(c_id) INTO :namecnt
      FROM customer
      WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
    +==========================================================*/

    /*==========================================================================+
    EXEC SQL SELECT count(c_id) INTO :namecnt
      FROM customer
      WHERE c_last=:c_last AND c_d_id=:d_id AND c_w_id=:w_id;

    EXEC SQL DECLARE c_name CURSOR FOR
      SELECT c_balance, c_first, c_middle, c_id
      FROM customer
      WHERE c_last=:c_last AND c_d_id=:d_id AND c_w_id=:w_id
      ORDER BY c_first;

    EXEC SQL OPEN c_name;
    +===========================================================================*/

    // here we assume we are not going to have more than 300 duplicates
    char* c_tuples[300];
    char cust_temp[LASTNAME_LEN + sizeof(d_id)+sizeof(w_id)];
    int key_length = concat_customer(cust_temp, c_last, clastsiz, d_id, w_id);
    if (!(c_tuples[0]=(char*)custhi->get((byte*)cust_temp, key_length))) {
      printf("Lookup error in secondary index (ordstat name)!\n");
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++)
      if(!(c_tuples[namecnt]=(char*)custhi->getAgain()))
        break;
    if (namecnt == 300)
      printf("Warning: more than 300 duplicate last names -- increase buffer!\n");

    /*============================================================================+
    for (n=0; n<namecnt/2; n++) {
      EXEC SQL FETCH c_name
        INTO :c_balance, :c_first, :c_middle, :c_id;
    }
    EXEC SQL CLOSE c_name;
    +=============================================================================*/

    // XXX: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  }
  else {

    /*===================================================+
    EXEC SQL SELECT c_balance, c_first, c_middle, c_last
      INTO :c_balance, :c_first, :c_middle, :c_last
      FROM customer
      WHERE c_id=:c_id AND c_d_id=:d_id AND c_w_id=:w_id;
    +===================================================*/

    if (!customer_tree.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
      printf("Lookup error 16!\n");
      exit(-1);
    }
  }
  
  // retrieve from customer: balance, first, middle
  // XXX: we have already retrieved the appropriate customer tuple
  //      and so we don't explicitly copy those fields


  // XXX: according to TPC-C specs the following statement
  //      should retrieve all orders placed by the same customer
  /*==================================================+
  EXEC SQL SELECT o_id, o_carrier_id, o_entry_d
    INTO :o_id, :o_carrier_id, :entdate
    FROM orders
    ORDER BY o_id DESC;
  +===================================================*/

  #define O_TUPLES_NUM 8000
  char* o_tuples[O_TUPLES_NUM];
  // we use hash index access to retrieve the tuple with max o_id
  byte* hashkey = new byte[3*sizeof(long)];
  memcpy(hashkey, (byte*)c_tuple, 3*sizeof(long));  
  if (!(o_tuples[0]=(char*)ordhi->get(hashkey, 3*sizeof(long)))) {
    printf("Lookup error in secondary index (ordstat ord)!\n");
    exit(-1);
  }

  int o_index;
  int max_o_index = 0;
  long max_o_id = *((long*)(o_tuples[0])); 
  for (o_index=1; o_index<O_TUPLES_NUM; o_index++) {
    if(!(o_tuples[o_index]=(char*)ordhi->getAgain()))
      break;
    if (max_o_id < *((long*)(o_tuples[o_index]))) {
      max_o_id = *((long*)(o_tuples[o_index]));
      max_o_index = o_index;
    }
  }
  
  delete[] hashkey;
  if (o_index == O_TUPLES_NUM)
    printf("Warning: more than %d orders in hash bucket -- increase buffer!\n",O_TUPLES_NUM);

  char* o_tuple = o_tuples[max_o_index];

  /*====================================================================+
  EXEC SQL DECLARE c_line CURSOR FOR
    SELECT ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d
    FROM order_line
    WHERE ol_o_id=:o_id AND ol_d_id=:d_id AND ol_w_id=:w_id;

  EXEC SQL OPEN c_line;
  EXEC SQL WHENEVER NOT FOUND CONTINUE;
  +====================================================================*/

  /*=========================================================================================+
  i=0;
  while (sql_notfound(FALSE)) {
    i++;
    EXEC SQL FETCH c_line
      INTO :ol_i_id[i], :ol_supply_w_id[i], :ol_quantity[i], :ol_amount[i], :ol_delivery_d[i];
  }
  EXEC SQL CLOSE c_line;
  +=========================================================================================*/

  // XXX: Here we want to retrieve all order lines for a given triplet(o_id, d_id, w_id)
  //      We know that ol_number is in [1..(5-15)], so we do as many retrievals as necessary

  byte* ol_tuples[16];
  int i;
  for (i=1; i<=15; i++) { 
    if (!orderline_tree.find((int)ORDER_LINEkey(max_o_id, d_id, w_id, long(i)), &(ol_tuples[i])))
      break;
  }
  if (i == 1) {
    printf("Lookup error 18!\n");
    exit(-1);
  }
 
  /*==================+
  EXEC SQL COMMIT WORK;
  +==================*/
  return;
}


/*==================================================================+
| ROUTINE NAME
| delivery()
+==================================================================*/

void delivery(long w_id, long o_carrier_id) {


  // gettimestamp(datetime);
  char ol_delivery_d[11];
  //TODO add timestamp here
  memcpy(ol_delivery_d, "2007-01-01:", 11);
 
  /* For each district in warehouse */ 
  for (long d_id=1; d_id<=DIST_PER_WARE; d_id++) { 
    
    /*========================================+
    EXEC SQL DECLARE c_no CURSOR FOR
      SELECT no_o_id
      FROM new_order
      WHERE no_d_id = :d_id AND no_w_id = :w_id
      ORDER BY no_o_id ASC;

    EXEC SQL OPEN c_no;
    EXEC SQL WHENEVER NOT FOUND continue;
    EXEC SQL FETCH c_no INTO :no_o_id;
    +=========================================*/

    // select the lowest no_o_id 
    // if (not_found)
    //   continue;
  
    if (lowest_no_id[(int)(w_id*10+d_id)] < 0) {
      printf("Debug #1: Delivery has caught up with New Order\n");
      continue;
    }
 
    /*===========================+
    EXEC SQL DELETE FROM new_order
      WHERE CURRENT OF c_no;
    EXEC SQL CLOSE c_no;
    +============================*/

    long o_id_low = lowest_no_id[(int)(w_id*10+d_id)];
    byte* hashkey = new byte[3*sizeof(long)];
    // hashkey is (o_id_low, o_d_id, o_w_id)    
    memcpy(hashkey, (byte*)&o_id_low, sizeof(long));
    memcpy(hashkey+sizeof(long), (byte*)&d_id, sizeof(long));   
    memcpy(hashkey+2*sizeof(long), (byte*)&w_id, sizeof(long));   
    byte* no_tuple;
    if (!(no_tuple=(byte*)newordhi->remove(hashkey, 3*sizeof(long)))) {
      printf("Error in deleting from New Order secondary index\n");
      exit(-1);
    }

    //XXX: update lowest_no_id
   
    o_id_low += 1;
    memcpy(hashkey, (byte*)&o_id_low, sizeof(long)); 
    if (!(no_tuple=(byte*)newordhi->get(hashkey, 3*sizeof(long)))) {
      printf("Debug #2: Delivery has caught up with New Order\n");
      lowest_no_id[(int)(w_id*10+d_id)] = -1;
    }
    else {
      lowest_no_id[(int)(w_id*10+d_id)] += 1;
    }

    o_id_low -= 1;
    delete[] hashkey;

    /*===========================================================+
    EXEC SQL SELECT o_c_id
      INTO :c_id
      FROM orders
      WHERE o_id = :no_o_id AND o_d_id = :d_id AND o_w_id = :w_id;

    EXEC SQL UPDATE orders
      SET o_carrier_id = :o_carrier_id
      WHERE o_id = :no_o_id AND o_d_id = :d_id AND o_w_id = :w_id;
    +============================================================*/

    // retrieve tuple from orders, update o_carrier_id
    char* o_tuple;  
    if (!orders_tree.find((int)ORDERSkey(o_id_low, d_id, w_id), (byte**)&o_tuple)) {
      printf("Lookup error 19!\n");
      exit(-1);
    }

    long c_id = *((long*)(o_tuple+sizeof(long)));
    *((long*)(o_tuple+4*sizeof(long))) = o_carrier_id;


    /*================================================================+
    EXEC SQL UPDATE order_line
      SET ol_delivery_d = :datetime
      WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id AND ol_w_id = :w_id;

    EXEC SQL SELECT SUM(ol_amount)
      INTO :ol_total
      FROM order_line
      WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id AND ol_w_id = :w_id;
    +================================================================*/

    // read all order line tuples (1 - 5..15), update dates, compute total amount

    char* ol_tuple;
    float ol_total = 0.0;
    for (int l=1; l<=15; l++) {
      if (!orderline_tree.find((int)ORDER_LINEkey(o_id_low, d_id, w_id, long(l)),
                               (byte**)&(ol_tuple)));
        break;
      memcpy(ol_tuple+ 7*sizeof(long) + sizeof(float) + 24, ol_delivery_d, 11);
      ol_total += *((float*)(ol_tuple+ 7*sizeof(long)));
    }

    /*=======================================================+
    EXEC SQL UPDATE customer
      SET c_balance = c_balance + :ol_total
      WHERE c_id = :c_id AND c_d_id = :d_id AND c_w_id = :w_id;
    +========================================================*/

    char* c_tuple;
    if (!customer_tree.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
      printf("Lookup error 20!\n");
      exit(-1);
    }

    float c_balance = *((float*)(c_tuple + 4*sizeof(long) + sizeof(float)));
    *((float*)(c_tuple + 4*sizeof(long) + sizeof(float))) = c_balance + ol_total;
    /*==================+
    EXEC SQL COMMIT WORK;
    +===================*/
    
    }

  /*==================+
  EXEC SQL COMMIT WORK;
  +===================*/
  return;
}


/*==================================================================+
| ROUTINE NAME
| slev() -- stock level
+==================================================================*/

void slev(long w_id, long d_id, long threshold) {

  /*=================================+
  EXEC SQL SELECT d_next_o_id
    INTO :o_id
    FROM district
    WHERE d_w_id=:w_id AND d_id=:d_id;
  +=================================*/

  char* d_tuple;
  if (!district_tree.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 21!\n");
    exit(-1);
  }
  long o_id = *((long*)(d_tuple + 2*sizeof(long) + 2*sizeof(float)));

  /*============================================================================+
  EXEC SQL SELECT COUNT(DISTINCT (s_i_id))
    INTO :stock_count
    FROM order_line, stock
    WHERE ol_w_id=:w_id AND ol_d_id=:d_id AND ol_o_id<:o_id AND ol_o_id>=:o_id-20
          AND s_w_id=:w_id AND s_i_id=ol_i_id AND s_quantity < :threshold;
  +============================================================================*/

  //TODO: we need to retrieve about 200 tuples from order line,
  //      using ( [o_id-20..o_id) , d_id, w_id, [1..(5-15)])
  //   and for each retrieved tuple, read the corresponding stock tuple
  //      using (ol_i_id, w_id)

  byte* ol_tuples[301];
  int i;
  int l=0;
  for (i=1; i<=20; i++) {
    if (l == 1) {
      printf("Lookup error 22!\n");
      exit(-1);
    }

    for (l=1; l<=15; l++) {
      if (!orderline_tree.find((int)ORDER_LINEkey(o_id - i, d_id, w_id, long(l)),
                               &(ol_tuples[(i-1)*15+l])))
        continue;
      else {
        char* s_tuple;
        long ol_i_id = *((long*)(ol_tuples[(i-1)*15+l] + 4*sizeof(long)));
        s_tuple = stockarray[(int)STOCKkey(ol_i_id, w_id)];
      }
    }
  }

  /*==================+
  EXEC SQL COMMIT WORK;
  +===================*/
  return;
}


/*==================================================================+
| ROUTINE NAME
| MakeAddress()
| DESCRIPTION
| Build an Address
| ARGUMENTS
+==================================================================*/

void MakeAddress(char* str1, char* str2, char* city, char* state, char* zip) {
  MakeAlphaString(10,20,str1); /* Street 1*/
  MakeAlphaString(10,20,str2); /* Street 2*/
  MakeAlphaString(10,20,city); /* City */
  MakeAlphaString(2,2,state); /* State */
  MakeNumberString(9,9,zip); /* Zip */
}


/*==================================================================+
| ROUTINE NAME
| Error()
| DESCRIPTION
| Handles an error from a SQL call.
| ARGUMENTS
+==================================================================*/

void Error() {
  /*======================================+
  printf( "SQL Error %d\n", sqlca.sqlcode);

  EXEC SQL WHENEVER SQLERROR CONTINUE;
  EXEC SQL ROLLBACK WORK RELEASE;

  +=======================================*/
  exit( -1 );
}


/*==================================================================+
| ROUTINE NAME
| InitPermutation
+==================================================================*/

void InitPermutation() {
  int i;
  perm_count = 0;

  // Init with consecutive values
  for(i=0; i < CUST_PER_DIST; i++) {
    perm_c_id[i] = i+1;
  }

  // shuffle
  for(i=0; i < CUST_PER_DIST-1; i++) {
    int j = (int) RandomNumber(i+1, CUST_PER_DIST-1);
    long tmp = perm_c_id[i];
    perm_c_id[i] = perm_c_id[j];
    perm_c_id[j] = tmp;
  }
  return;
}


/*==================================================================+
| ROUTINE NAME
| GetPermutation
+==================================================================*/

long GetPermutation() {
  if(perm_count >= CUST_PER_DIST) {
    // wrapped around, restart at 0
    perm_count = 0;
  }
  return (long) perm_c_id[perm_count++];
}
