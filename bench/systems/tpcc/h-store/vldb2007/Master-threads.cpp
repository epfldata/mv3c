/* Multithreaded master server.
 *
 * Maintains a pool of a fixed number of threads, each of which is
 * either busy (processing a previous txn) or idle (available for
 * execution). In the initial design, each thread is pinned to a
 * particular partition of the data.
 *
 * Accepts transactions from driver and despatches them to individual
 * threads depending on which customers/warehouses they access, which
 * each run in their own threads.
 *
 * TODO: Figure out what to do if master thread becomes a bottleneck.
 * TODO: Ideally need to pin threads to CPUs for predictability.
 * TODO: Optimize/debug condition variables or remove them.
 */

#include <iostream>
#include <pthread.h>
#include <queue>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

using boost::scoped_ptr;
using boost::scoped_array;

// High-performance Intel queue.
#include "tbb/concurrent_queue.h"
using tbb::concurrent_queue;

#include "Constants.h"

#include <math.h>
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

// B-Tree constants.
#define BTree_N 8
#define BTree_M 8

// Attempt at fine grained profiling.
#define rdtsc(t) asm __volatile__ ("rdtsc" : "=A" (t))

// Maximum number of transactions.
#define MAX_TRANSACTIONS 1000000

using namespace std;

// Info to provide to each thread on startup.
struct ThreadInfo {
  // Total # of warehouses.
  // Required to generate warehouse data for each partition.
  int num_warehouses_;

  // Total # of partitions.
  // Required to decide how much memory to allocate.
  int num_parts_;

  int p_; // Partition number.

  // Condition which indicates loading complete.
  bool load_complete_; 

  //scoped_ptr<queue<char*> > in_queue_; // Address of input queue.
  scoped_ptr<concurrent_queue<char*> > in_queue_; // Address of input queue.

  // Condition var for input queue.
  pthread_cond_t *queue_cond_;
  pthread_mutex_t *queue_mutex_;

  // Address of stock table shared
  // across all threads.
  char **stock_array_;
  long stock_table_size_;
};

// Key definitions.
#define STOCKkey(s_i_id, s_w_id) (s_w_id*MAXITEMS + s_i_id)
#define DISTRICTkey(d_id, d_w_id) (d_w_id*DIST_PER_WARE + d_id)
#define CUSTOMERkey(c_id, c_d_id, c_w_id) ((c_w_id*DIST_PER_WARE+c_d_id) * \
                                           CUST_PER_DIST+c_id)
#define NEW_ORDERkey(no_o_id, no_d_id, no_w_id) \
  ((no_w_id*DIST_PER_WARE+no_d_id) * ORD_PER_DIST + no_o_id)
#define ORDERSkey(o_id, o_d_id, o_w_id) \
  ((o_w_id*DIST_PER_WARE+o_d_id) * ORD_PER_DIST + o_id)
#define ORDER_LINEkey(ol_o_id, ol_d_id, ol_w_id, ol_number) \
  (((ol_w_id*DIST_PER_WARE+ol_d_id) * ORD_PER_DIST + ol_o_id) * 15 + ol_number)

// Stores all state associated with a worker thread.
class WorkerThread {
 public:
  // Constructor.
  WorkerThread(ThreadInfo *thread_info) :
      thread_info_(thread_info), commit_count_(0),
      item_table_size_(0), warehouse_table_size_(0),
      history_table_size_(0), district_table_size_(0),
      neworder_table_size_(0), order_table_size_(0),
      orderline_table_size_(0), customer_table_size_(0) {}

  // Run this worker thread.
  void* Run();

 private:
  // Generate data on this partition.
  void GenerateData();
  
  // Methods to generate table data.

  // Load items table.
  void LoadItems();

  // Load warehouses table.
  void LoadWarehouses();

  // Load customers table.
  void LoadCustomers();

  // Load orders table.
  void LoadOrders();

  // Load individual districts for a given warehouse.
  long LoadDistrict(long w_id);

  // Load individual customer for a given warehouse and district.
  long LoadCustomer(long w_id, long d_id);

  // Initialize permutations.
  void InitPermutation();

  // Return next permutation.
  long GetPermutation();

  // Load individual orders for a given warehouse and district.
  void LoadIndividualOrders(long w_id, long d_id);

  // Methods to execute transactions.
  bool ExecuteLocalNewOrderTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity);

  bool ExecuteRemoteNewOrderHomeTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity);

  bool ExecuteRemoteNewOrderAwayTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity);

  void ExecuteLocalPaymentTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount);

  void ExecuteRemotePaymentHomeTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount);

  void ExecuteRemotePaymentAwayTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount);

  void ExecuteOrdstatTx(
    long w_id, long d_id, long c_id,
    bool byname, char* c_last, int c_last_size);

  void ExecuteDeliveryTx(long w_id, long o_carrier_id);

  void ExecuteSlevTx(long w_id, long d_id, long threshold);

  // Shared info with master thread (and hence all threads).
  ThreadInfo *thread_info_;

  // Private info for this thread.

  // Total # of txns committed.
  int commit_count_;

  // TPC-C Tables (hash tables, B trees) and all state.
  BPlusTree< int, byte*, BTree_N, BTree_M > item_tree_;
  long item_table_size_;

  BPlusTree< int, byte*, BTree_N, BTree_M > warehouse_tree_;
  long warehouse_table_size_;

  // Optional: Stock if replicated.
  // char **stock_array_; // array index is STOCK key (s_i_id, s_w_id)
  // long stock_table_size_;

  BPlusTree< int, byte*, BTree_N, BTree_M > district_tree_;
  long district_table_size_;

  BPlusTree< int, byte*, BTree_N, BTree_M > customer_tree_;
  scoped_ptr<HashIndex> cust_hi_;
  long customer_table_size_;
  long history_table_size_;

  scoped_ptr<HashIndex> neword_hi_;
  long neworder_table_size_;

  BPlusTree< int, byte*, BTree_N, BTree_M > orders_tree_;
  scoped_ptr<HashIndex> ord_hi_; // Hash key is (o_c_id, o_d_id, o_w_id)
  long order_table_size_;

  // Hack to keep lowest new order id for each district.
  scoped_array<long> lowest_neworder_id_;

  BPlusTree< int, byte*, BTree_N, BTree_M > orderline_tree_;
  long orderline_table_size_;

  /* Hash function to use for hash indices. */
  scoped_ptr<StringHashFunction> hf_;

  /* Permutation state for orders. */
  scoped_array<long> perm_c_id_;
  int perm_count_;

  // In-memory buffers to store tuples.
  scoped_ptr<TupleBuffer> tb_;
  scoped_ptr<TupleBuffer> history_buffer_;
  scoped_ptr<TupleBuffer> hash_keys_;
};

/***************************************************************
  Following code taken from Dan and Stavros's H-Store prototype
 ***************************************************************/

// Local transaction execution methods.
// Only the NewOrder tx can abort, so it returns bool:
// all others always commit and hence return void.

// Execute new order tx confined to a single site.
bool WorkerThread::ExecuteLocalNewOrderTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;
  // read_inst(); // Old PAPI Code

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

    if(!item_tree_.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return false;
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
  if (!warehouse_tree_.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 1!%d\n", w_id);
    exit(-1);
  }
  float w_tax = *((float*)(w_tuple + sizeof(long)));

  char* c_tuple;
  if (!customer_tree_.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
    printf("Lookup error 2: could not find customer key(%ld, %ld, %ld)\n", c_id, d_id, w_id);
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
  if (!district_tree_.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
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
  byte* temp_tuple = tb_->allocateTuple(o_tuple_size);
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
  orders_tree_.insert(o_key, temp_tuple);

  int hashKeySize=3*sizeof(long);
  byte* hash_key = hash_keys_->allocateTuple(hashKeySize);
  // hashkey is (o_c_id, o_d_id, o_w_id)
  memcpy(hash_key,temp_tuple + sizeof(long), 3*sizeof(long));

  ord_hi_->put(hash_key,hashKeySize,temp_tuple, o_tuple_size);

  //TODO: diff from Ben's code; he does not have the following insert
  /*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
   +=======================================================*/

  int no_tuple_size = 3 * sizeof(long);
  temp_tuple = tb_->allocateTuple(no_tuple_size);
  offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  //int no_key = NEW_ORDERkey(o_id, d_id, w_id);
  //neworder_tree.insert(no_key, temp_tuple);

  hashKeySize = 3*sizeof(long);
  hash_key = hash_keys_->allocateTuple(hashKeySize);
  // hashkey is (o_id, o_d_id, o_w_id)
  memcpy(hash_key,temp_tuple, 3*sizeof(long));
  neword_hi_->put(hash_key,hashKeySize,temp_tuple, no_tuple_size);

  //TODO: hack
  if (lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] < 0) {
    lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] = o_id;
  }


  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {

    long ol_supply_w_id = supware[ol_number-1];
    // TODO: the following is not used anywhere else
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
    // TODO: we don't use i_name (it's used only for printing)

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
    s_tuple = thread_info_->stock_array_[(int)STOCKkey(ol_i_id, ol_supply_w_id)];

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
    orderline_table_size_ += ol_tuple_size;
    temp_tuple = tb_->allocateTuple(ol_tuple_size);
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
    // TODO hack: we represent NULL timestamp as "0"
    char ol_delivery_d[11];
    ol_delivery_d[0] = 0;
    ol_delivery_d[1] = '\0';
    memcpy(temp_tuple+offset, ol_delivery_d, 11);

    int ol_key = ORDER_LINEkey(o_id, d_id, w_id, ol_number);
    orderline_tree_.insert(ol_key, temp_tuple);

  }
  // read_inst(); // Old PAPI code
  // read_inst(); // Old PAPI code
  /*==================+
    EXEC SQL COMMIT WORK
   +==================*/
  return true;
}

// Execute local portion of a new order tx spread across multiple sites.
bool WorkerThread::ExecuteRemoteNewOrderHomeTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;

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

    if(!item_tree_.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return false;
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
  if (!warehouse_tree_.find((int)w_id, (byte**)&w_tuple)) {
    printf("Lookup error 4!\n");
    exit(-1);
  }
  float w_tax = *((float*)(w_tuple + sizeof(long)));

  char* c_tuple;
  if (!customer_tree_.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
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
  if (!district_tree_.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
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
  byte* temp_tuple = tb_->allocateTuple(o_tuple_size);
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
  orders_tree_.insert(o_key, temp_tuple);

  int hashKeySize=3*sizeof(long);
  byte* hash_key = hash_keys_->allocateTuple(hashKeySize);
  // hashkey is (o_c_id, o_d_id, o_w_id)
  memcpy(hash_key, temp_tuple + sizeof(long), 3*sizeof(long));

  ord_hi_->put(hash_key,hashKeySize, temp_tuple, o_tuple_size);

  //TODO: diff from Ben's code; he does not have the following insert
  /*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
   +=======================================================*/

  int no_tuple_size = 3 * sizeof(long);
  temp_tuple = tb_->allocateTuple(no_tuple_size);
  offset = 0;
  *((long*)temp_tuple) = o_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = d_id;
  offset += sizeof(long);
  *((long*)(temp_tuple+offset)) = w_id;
  //int no_key = NEW_ORDERkey(o_id, d_id, w_id);
  //neworder_tree.insert(no_key, temp_tuple);

  hashKeySize = 3*sizeof(long);
  hash_key = hash_keys_->allocateTuple(hashKeySize);
  // hashkey is (o_id, o_d_id, o_w_id)
  memcpy(hash_key, temp_tuple, 3*sizeof(long));
  neword_hi_->put(hash_key, hashKeySize, temp_tuple, no_tuple_size);

  //TODO: hack
  if (lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] < 0) {
    lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] = o_id;
  }

  for (int ol_number=1; ol_number<=ol_cnt; ol_number++) {

    long ol_supply_w_id = supware[ol_number-1];
    // TODO: the following is not used anywhere else
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
    // TODO: we don't use i_name (it's used only for printing)

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
    s_tuple = thread_info_->stock_array_[(int)STOCKkey(ol_i_id, ol_supply_w_id)];  

    char* ol_dist_info = s_tuple + 3*sizeof(long) + sizeof(float) + 2*sizeof(int) + (d_id-1)*24;

    //////////////////////////////////////////////////////////
    //                                                      // 
    // Local Version of New-Order:                          //
    // Update on Stock is executed only if it is local      //  
    //                                                      //
    //////////////////////////////////////////////////////////
    
    bool remote;
    long wares_per_core = thread_info_->num_warehouses_ /
                          thread_info_->num_parts_;
    if (ol_supply_w_id <= thread_info_->p_ * wares_per_core ||
        ol_supply_w_id > (thread_info_->p_ + 1) * wares_per_core) {
      remote = true;
    }

    // Only bother if not remote.
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
    orderline_table_size_ += ol_tuple_size;
    temp_tuple = tb_->allocateTuple(ol_tuple_size);
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
    // TODO hack: we represent NULL timestamp as "0"
    char ol_delivery_d[11];
    ol_delivery_d[0] = 0;
    ol_delivery_d[1] = '\0';
    memcpy(temp_tuple+offset, ol_delivery_d, 11);

    int ol_key = ORDER_LINEkey(o_id, d_id, w_id, ol_number);
    orderline_tree_.insert(ol_key, temp_tuple);
  }

  /*==================+
    EXEC SQL COMMIT WORK
   +==================*/
  return true;
}

// Execute remote portion of a new order tx spread across multiple sites.
bool WorkerThread::ExecuteRemoteNewOrderAwayTx(
    long w_id, long d_id, long c_id, long ol_cnt, bool all_local,
    long* item_id, long* supware, long* quantity) {

  char* bg = new char[15];
  int total = 0;

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

    if(!item_tree_.find((int)item_id[ol_number-1], &(items[ol_number-1]))) {
      // abort
      return false;
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
    long wares_per_core = thread_info_->num_warehouses_ /
                          thread_info_->num_parts_;
    if (ol_supply_w_id <= thread_info_->p_ * wares_per_core ||
        ol_supply_w_id > (thread_info_->p_ + 1) * wares_per_core) {
      remote = true;
    }

    // Only bother if this is the remote warehouse.
    if (remote) {

      char* s_tuple;
      s_tuple = thread_info_->stock_array_[(int)STOCKkey(ol_i_id, ol_supply_w_id)];  

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
  return true;
}

// Execute payment tx confined to a single site.
void WorkerThread::ExecuteLocalPaymentTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount) {

  /*====================================================+
    EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
    WHERE w_id=:w_id;
   +====================================================*/

  // read_inst(); // Old PAPI code
  char* w_tuple;
  if (!warehouse_tree_.find((int)w_id, (byte**)&w_tuple)) {
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

  // TODO: only the name of the Warehouse is used later on

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

  //read_inst(); // Old PAPI code
  if (!district_tree_.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
    printf("Lookup error 8!\n");
    exit(-1);
  }
  //read_inst(); // Old PAPI code
  float d_ytd = *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float)));
  *((float*)(d_tuple + 2 * sizeof(long) + sizeof(float))) = d_ytd + (float)h_amount;

  /*====================================================================+
    EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
    INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
    FROM district
    WHERE d_w_id=:w_id AND d_id=:d_id;
   +====================================================================*/

  // TODO: only the name of the District is used later on

  char d_name[11];
  int dnamesiz = (int)(*(d_tuple + 3*sizeof(long) + 2*sizeof(float) + 11))
    -(3*sizeof(long) + 2*sizeof(float) + 15);
  memcpy(d_name, d_tuple + 3*sizeof(long) + 2*sizeof(float) + 15, dnamesiz);
  d_name[dnamesiz] = '\0';

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
    scoped_ptr<char> c_last_key(new char[c_last_size + 2*sizeof(long)]);
    memmove(c_last_key.get(), c_last, c_last_size);
    memmove(c_last_key.get() + c_last_size,  &c_d_id, sizeof(long));
    memmove(c_last_key.get() + c_last_size + sizeof(long),
            &c_w_id, sizeof(long));
    if (!(c_tuples[0]=(char*)cust_hi_->get((byte*)c_last_key.get(),
                                           c_last_size+2*sizeof(long)))) {
      printf("Thread %d: Lookup error in secondary index (payment): d_id, w_id, c_id, c_d_id, c_w_id = %ld %ld %ld %ld %ld\n",
              thread_info_->p_, d_id, w_id, c_id, c_d_id, c_w_id);
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++)
      if(!(c_tuples[namecnt]=(char*)cust_hi_->getAgain()))
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

    // TODO: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  } else {
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
    //read_inst(); // Old PAPI code
    if (!customer_tree_.find((int)CUSTOMERkey(c_id, c_d_id, c_w_id), (byte**)&c_tuple)) {
      printf("Lookup error 10!\n");
      exit(-1);
    }
    //read_inst(); // Old PAPI code
  } // if by name

  float c_balance = *((float*)(c_tuple + 4*sizeof(long) + sizeof(float)));
  char c_credit[3];
  memcpy(c_credit, c_tuple + 6*sizeof(long) + 3*sizeof(float) + 40, 2); 
  c_balance += h_amount;
  c_credit[2]='\0';

  /*============================================================+
    EXEC SQL UPDATE customer SET c_balance = :c_balance
    WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
   +=============================================================*/

  *((float*)(c_tuple + 4*sizeof(long) + sizeof(float))) = c_balance;

  //read_inst(); // Old PAPI code
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
  byte* h_temp_tuple = history_buffer_->allocateTuple(h_tuple_size);

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
  //read_inst(); // Old PAPI code
  //read_inst(); // Old PAPI code
  /*==================+
    EXEC SQL COMMIT WORK;
   +==================*/
}

// Execute local portion of a payment tx spread across multiple sites.
void WorkerThread::ExecuteRemotePaymentHomeTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount) {
  /*====================================================+
    EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
    WHERE w_id=:w_id;
   +====================================================*/

  char* w_tuple;
  if (!warehouse_tree_.find((int)w_id, (byte**)&w_tuple)) {
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

  // TODO: only the name of the Warehouse is used later on

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
  if (!district_tree_.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
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

  // TODO: only the name of the District is used later on

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
  byte* h_temp_tuple = history_buffer_->allocateTuple(h_tuple_size);

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

// Execute remote portion of a payment tx spread across multiple sites.
void WorkerThread::ExecuteRemotePaymentAwayTx(
    long w_id, long d_id, long c_id, long c_w_id, long c_d_id,
    bool byname, char* c_last, int c_last_size, long h_amount) {

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
    scoped_ptr<char> c_last_key(new char[c_last_size + 2*sizeof(long)]);
    memmove(c_last_key.get(), c_last, c_last_size);
    memmove(c_last_key.get() + c_last_size,  &c_d_id, sizeof(long));
    memmove(c_last_key.get() + c_last_size + sizeof(long),
            &c_w_id, sizeof(long));
    if (!(c_tuples[0]=(char*)cust_hi_->get((byte*)c_last_key.get(),
                                           c_last_size+2*sizeof(long)))) {
      printf("Thread %d: Lookup error in secondary index (payment remote)! c_d_id = %ld c_w_id = %ld\n", thread_info_->p_, c_d_id, c_w_id);
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++) 
      if(!(c_tuples[namecnt]=(char*)cust_hi_->getAgain()))
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

    // TODO: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  } else { 

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

    if (!customer_tree_.find((int)CUSTOMERkey(c_id, c_d_id, c_w_id), (byte**)&c_tuple)) {
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
  } else {
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

// Execute order status tx.
void WorkerThread::ExecuteOrdstatTx(
    long w_id, long d_id, long c_id,
    bool byname, char* c_last, int c_last_size) {

  char* c_tuple;
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
    scoped_ptr<char> c_last_key(new char[c_last_size + 2*sizeof(long)]);
    memmove(c_last_key.get(), c_last, c_last_size);
    memmove(c_last_key.get() + c_last_size,  &d_id, sizeof(long));
    memmove(c_last_key.get() + c_last_size + sizeof(long),
            &w_id, sizeof(long));
    if (!(c_tuples[0]=(char*)cust_hi_->get((byte*)c_last_key.get(),
                                           c_last_size+2*sizeof(long)))) {
      printf("Thread %d: Lookup error in secondary index (ordstat name): c_id, d_id, w_id = %ld %ld %ld!\n",
              thread_info_->p_, c_id, d_id, w_id);
      exit(-1);
    }

    int namecnt;

    for (namecnt=1; namecnt<300; namecnt++)
      if(!(c_tuples[namecnt]=(char*)cust_hi_->getAgain()))
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

    // TODO: we don't retrieve all the info, just the tuple we are interested in
    c_tuple = c_tuples[(int)(namecnt/2)];
  } else {
    /*===================================================+
      EXEC SQL SELECT c_balance, c_first, c_middle, c_last
      INTO :c_balance, :c_first, :c_middle, :c_last
      FROM customer
      WHERE c_id=:c_id AND c_d_id=:d_id AND c_w_id=:w_id;
     +===================================================*/
    if (!customer_tree_.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
      printf("Lookup error 16!\n");
      exit(-1);
    }
  }

  // retrieve from customer: balance, first, middle
  // TODO: we have already retrieved the appropriate customer tuple
  //      and so we don't explicitly copy those fields


  // TODO: according to TPC-C specs the following statement
  //      should retrieve all orders placed by the same customer
  /*==================================================+
    EXEC SQL SELECT o_id, o_carrier_id, o_entry_d
    INTO :o_id, :o_carrier_id, :entdate
    FROM orders
    ORDER BY o_id DESC;
   +===================================================*/

  char* o_tuples[300];
  // we use hash index access to retrieve the tuple with max o_id
  byte* hashkey = new byte[3*sizeof(long)];
  memcpy(hashkey, (byte*)c_tuple, 3*sizeof(long));

  //printf("Looking up hashkey %ld %ld %ld\n", ((long*)hashkey)[0],
  //        ((long*)hashkey)[1], ((long*)hashkey)[2]);
  if (!(o_tuples[0]=(char*)ord_hi_->get(hashkey, 3*sizeof(long)))) {
    printf("Lookup error in secondary index (ordstat ord)!\n");
    exit(-1);
  }

  int o_index;
  int max_o_index = 0;
  long max_o_id = *((long*)(o_tuples[0])); 
  for (o_index=1; o_index<100; o_index++) {
    if(!(o_tuples[o_index]=(char*)ord_hi_->getAgain()))
      break;
    if (max_o_id < *((long*)(o_tuples[o_index]))) {
      max_o_id = *((long*)(o_tuples[o_index]));
      max_o_index = o_index;
    }
  }

  delete[] hashkey;
  if (o_index == 300)
    printf("Warning: more than 300 orders in hash bucket -- increase buffer!\n");

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

  // TODO: Here we want to retrieve all order lines for a given triplet(o_id, d_id, w_id)
  //      We know that ol_number is in [1..(5-15)], so we do as many retrievals as necessary

  byte* ol_tuples[16];
  int i;
  for (i=1; i<=15; i++) { 
    if (!orderline_tree_.find((int)ORDER_LINEkey(max_o_id, d_id, w_id, long(i)), &(ol_tuples[i])))
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

// Execute delivery tx.
void WorkerThread::ExecuteDeliveryTx(long w_id, long o_carrier_id) {
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

    if (lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] < 0) {
      printf("Debug #1: Delivery has caught up with New Order\n");
      continue;
    }

    /*===========================+
      EXEC SQL DELETE FROM new_order
      WHERE CURRENT OF c_no;
      EXEC SQL CLOSE c_no;
     +============================*/
    
    long o_id_low = lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)];
    //printf("lowest_neworder_id_[%ld*%d+%ld = %ld] = %ld\n",
    //        w_id, DIST_PER_WARE, d_id, w_id*DIST_PER_WARE+d_id,
    //        o_id_low);
    byte* hashkey = new byte[3*sizeof(long)];
    // hashkey is (o_id_low, o_d_id, o_w_id)    
    memcpy(hashkey, (byte*)&o_id_low, sizeof(long));
    memcpy(hashkey+sizeof(long), (byte*)&d_id, sizeof(long));   
    memcpy(hashkey+2*sizeof(long), (byte*)&w_id, sizeof(long));
    //printf("hashkey = %ld %ld %ld\n", ((long*)hashkey)[0], ((long*)hashkey)[1],
    //        ((long*)hashkey)[2]);

    byte* no_tuple;
    if (!(no_tuple=(byte*)neword_hi_->remove(hashkey, 3*sizeof(long)))) {
      printf("Error in deleting from New Order secondary index\n");
      exit(-1);
    }

    //TODO: update lowest_neworder_id_

    o_id_low += 1;
    memcpy(hashkey, (byte*)&o_id_low, sizeof(long)); 
    if (!(no_tuple=(byte*)neword_hi_->get(hashkey, 3*sizeof(long)))) {
      printf("Debug #2: Delivery has caught up with New Order\n");
      lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] = -1;
    } else {
      lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)] += 1;
      //printf("Incremented lowest_neworder_id_[%ld*%d+%ld = %ld] to %ld\n",
      //    w_id, DIST_PER_WARE, d_id, w_id*DIST_PER_WARE+d_id,
      //    lowest_neworder_id_[(int)(w_id*DIST_PER_WARE+d_id)]);
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
    if (!orders_tree_.find((int)ORDERSkey(o_id_low, d_id, w_id), (byte**)&o_tuple)) {
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
      if (!orderline_tree_.find((int)ORDER_LINEkey(o_id_low, d_id, w_id, long(l)),
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
    if (!customer_tree_.find((int)CUSTOMERkey(c_id, d_id, w_id), (byte**)&c_tuple)) {
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

// Execute stock level tx.
void WorkerThread::ExecuteSlevTx(long w_id, long d_id, long threshold) {
  /*=================================+
    EXEC SQL SELECT d_next_o_id
    INTO :o_id
    FROM district
    WHERE d_w_id=:w_id AND d_id=:d_id;
   +=================================*/

  char* d_tuple;
  if (!district_tree_.find((int)DISTRICTkey(d_id, w_id), (byte**)&d_tuple)) {
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
      if (!orderline_tree_.find((int)ORDER_LINEkey(o_id - i, d_id, w_id, long(l)),
            &(ol_tuples[(i-1)*15+l]))) {
        continue;
      } else {
        char* s_tuple;
        long ol_i_id = *((long*)(ol_tuples[(i-1)*15+l] + 4*sizeof(long)));
        s_tuple = thread_info_->stock_array_[(int)STOCKkey(ol_i_id, w_id)];
      }
    }
  }

  /*==================+
    EXEC SQL COMMIT WORK;
   +===================*/
  return;
}

// Load items table.
void WorkerThread::LoadItems() {
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
  byte* temp_tuple;

  /*===========================+
    EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
   +============================*/
  for (i = 0; i < MAXITEMS; i++) {
    orig[i]=0;
  }

  for (i = 0; i < MAXITEMS/10; i++) {
    do {
      pos = RandomNumber(0L, MAXITEMS - 1); // TODO: diff from TPC-C
    } while (orig[pos]);
    orig[pos] = 1;
  }

  for (i_id=1; i_id<=MAXITEMS; i_id++) {

    /* Generate Item Data */

    inamesiz = MakeAlphaString( 14, 24, i_name);
    i_price=((float) RandomNumber(100L,10000L))/100.0;
    idatasiz=MakeAlphaString(26,50,i_data);
    if (orig[i_id-1]) {  // TODO: diff from TPC-C
      pos = RandomNumber(0L, idatasiz - 8);
      i_data[pos]='o';
      i_data[pos+1]='r';
      i_data[pos+2]='i';
      i_data[pos+3]='g';
      i_data[pos+4]='i';
      i_data[pos+5]='n';
      i_data[pos+6]='a';
      i_data[pos+7]='l';
    }

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
    item_table_size_ += tuple_size;

    temp_tuple = tb_->allocateTuple(tuple_size);

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
    item_tree_.insert(key, temp_tuple);
  }

  /*==================+
    EXEC SQL COMMIT WORK;
   +===================*/
  cout << "Partition " << thread_info_->p_ << ": "
       << "Done loading items (size in bytes: " << item_table_size_ << ")."
       << "\n";
}

// Load warehouses table and districts table (but not stock).
void WorkerThread::LoadWarehouses() {
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
  long district_table_size_ = 0;
  long warehouse_table_size_ = 0;
  byte* temp_tuple;

  long start_val = 1L;
  long end_val = thread_info_->num_warehouses_;
  long wares_per_thread = thread_info_->num_warehouses_ /
                        thread_info_->num_parts_;
  start_val = 1L + thread_info_->p_ * wares_per_thread;
  end_val = (thread_info_->p_ + 1) * wares_per_thread;

  for (w_id=start_val; w_id<=end_val; w_id++) {
    /* Generate Warehouse Data */
    int wnamesiz = MakeAlphaString(6, 10, w_name);
    // MakeAddress( w_street_1, w_street_2, w_city, w_state, w_zip );     
    int wstreet1siz = MakeAlphaString(10, 20, w_street_1); /* Street 1*/
    int wstreet2siz = MakeAlphaString(10, 20, w_street_2); /* Street 2*/
    int wcitysiz = MakeAlphaString(10, 20, w_city); /* City */
    MakeAlphaString(2, 2, w_state); /* State */
    MakeNumberString(9, 9, w_zip); /* Zip */

    w_tax=((float)RandomNumber(10L,20L))/100.0;
    w_ytd=3000000.00;

    /*====================================================================+
      EXEC SQL INSERT INTO warehouse (w_id, w_name,
      w_street_1, w_street_2, w_city, w_state, w_zip, w_tax, w_ytd)
      values (:w_id, :w_name, :w_street_1,
      :w_street_2, :w_city, :w_state, :w_zip, :w_tax, :w_ytd);
     +=====================================================================*/

    // WAREHOUSE tuple
    // | long W_ID | float W_TAX | float W_YTD | char[2] W_STATE
    // | char[9] W_ZIP    
    // | char offset #1 |  char offset #2 | char offset #3 | char offset #4
    // | varchar(6-10) W_NAME | varchar(10-20) W_STREET_1
    // | varchar(10-20) W_STREET_2 | varchar(10-20) W_CITY 

    /*==========================================+
      Horizontica INSERT INTO
     +==========================================*/

    int tuple_size = sizeof(long) + 2*sizeof(float) + 15 + 
      wnamesiz + wstreet1siz + wstreet2siz + wcitysiz;
    warehouse_table_size_ += tuple_size;

    temp_tuple = tb_->allocateTuple(tuple_size);

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
    *(temp_tuple+offset) = (char)(offset + 2 + wnamesiz +
                                  wstreet1siz + wstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + wnamesiz + wstreet1siz +
                                  wstreet2siz + wcitysiz);
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
    warehouse_tree_.insert(key, temp_tuple);

    /* Generate district info for this w_id. */
    district_table_size_ += LoadDistrict(w_id);

    /*==================+
      EXEC SQL COMMIT WORK;
     +===================*/
  }

  // Optional: Load stock here if replicated.

  cout << "Partition " << thread_info_->p_ << ": "
       << "Done loading warehouses (size in bytes: " << warehouse_table_size_
       << ")." << "\n"
       << "Done loading districts (size in bytes: " << district_table_size_
       << ")." << "\n";
}

// Load customers (and history) table.
void WorkerThread::LoadCustomers() {
  /*===========================+
    EXEC SQL BEGIN DECLARE SECTION;
    EXEC SQL END DECLARE SECTION;
   +============================*/
  long w_id;
  long d_id;
  long customer_table_size_ = 0;

  /*====================================+
    EXEC SQL WHENEVER SQLERROR GOTO sqlerr;
   +=====================================*/
  long start_val = 1L;
  long end_val = thread_info_->num_warehouses_;
  long wares_per_thread = thread_info_->num_warehouses_ /
                        thread_info_->num_parts_;
  start_val = 1L + thread_info_->p_ * wares_per_thread;
  end_val = (thread_info_->p_ + 1) * wares_per_thread;

  // Load individual customers, in each district
  // supplied by each warehouse.
  for (w_id = start_val; w_id <= end_val; w_id++) {
    for (d_id = 1L; d_id <= DIST_PER_WARE; d_id++) {
      customer_table_size_ += LoadCustomer(w_id, d_id);
    }
  }

  /*====================================+
    EXEC SQL COMMIT WORK;
   +=====================================*/
  cout << "Partition " << thread_info_->p_ << ": "
       << "Done loading warehouses (size in bytes: " << customer_table_size_
       << ")." << "\n"
       << "Done loading history (size in bytes: " << history_table_size_
       << ")." << "\n";
}

// Load orders table.
void WorkerThread::LoadOrders() {
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

  long start_val = 1L;
  long end_val = thread_info_->num_warehouses_;
  long wares_per_thread = thread_info_->num_warehouses_ /
                          thread_info_->num_parts_;
  start_val = 1L + thread_info_->p_ * wares_per_thread;
  end_val = (thread_info_->p_ + 1) * wares_per_thread;

  for (w_id = start_val; w_id <= end_val; w_id++) {
    for (d_id=1L; d_id <= DIST_PER_WARE; d_id++) {
      LoadIndividualOrders(w_id, d_id);
    }
  }

  /*==================+
    EXEC SQL COMMIT WORK;
   +==================*/
  cout << "Partition " << thread_info_->p_ << ": "
       << "Done loading orders (size in bytes: " << order_table_size_
       << ")." << "\n"
       << "Done loading new orders (size in bytes: " << neworder_table_size_
       << ")." << "\n"
       << "Done loading order lines (size in bytes: " << orderline_table_size_
       << ")." << "\n";
}

// Load individual districts for a given warehouse.
long WorkerThread::LoadDistrict(long w_id) {
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

    temp_tuple = tb_->allocateTuple(tuple_size);

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
    *(temp_tuple+offset) = (char)(offset + 2 + dnamesiz + 
                                  dstreet1siz + dstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + dnamesiz + dstreet1siz +
                                  dstreet2siz + dcitysiz);
    offset += 1;
    memcpy(temp_tuple+offset, d_name, dnamesiz);
    offset += dnamesiz;
    memcpy(temp_tuple+offset, d_street_1, dstreet1siz);
    offset += dstreet1siz;
    memcpy(temp_tuple+offset, d_street_2, dstreet2siz);
    offset += dstreet2siz;
    memcpy(temp_tuple+offset, d_city, dcitysiz);

    int key = DISTRICTkey(d_id, d_w_id);
    district_tree_.insert(key, temp_tuple);
  }

  /*===========================+
    EXEC SQL COMMIT WORK;
   +===========================*/
  return table_size;
}

// Load individual customer for a given warehouse and district.
long WorkerThread::LoadCustomer(long w_id, long d_id) {
  /*===========================+
    EXEC SQL BEGIN DECLARE SECTION;
   +============================*/
  long c_id;
  long c_d_id;
  long c_w_id;
  char c_first[16];
  char c_middle[2];
  char c_last[16];
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

  for (c_id=1; c_id<=CUST_PER_DIST; c_id++) {

    /* Generate Customer Data */
    c_d_id = d_id;
    c_w_id = w_id;
    int cfirstsiz = MakeAlphaString( 8, 16, c_first );
    c_middle[0]='O';
    c_middle[1]='E';
    int c_last_size;
    if (c_id <= 1000)
      c_last_size = Lastname(c_id-1,c_last);
    else
      c_last_size = Lastname(NURand(255,0,999),c_last);
    //MakeAddress( c_street_1, c_street_2, c_city, c_state, c_zip );
    int cstreet1siz = MakeAlphaString(10, 20, c_street_1); /* Street 1*/
    int cstreet2siz = MakeAlphaString(10, 20, c_street_2); /* Street 2*/
    int ccitysiz = MakeAlphaString(10, 20, c_city); /* City */
    MakeAlphaString(2, 2, c_state); /* State */
    MakeNumberString(9, 9, c_zip); /* Zip */

    MakeNumberString( 16, 16, c_phone );
    // TODO add timestamp here.
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

    // CUSTOMER tuple
    // | long C_ID | long C_D_ID | long C_W_ID 
    // | long C_CREDIT_LIM | float C_DISCOUNT | float C_BALANCE
    // | float C_YTD_PAYMENT | long C_CNT_PAYMENT | long C_CNT_DELIVERY
    // | char[2] C_MIDDLE | char[2] C_STATE | char[9] C_ZIP
    // | char[16] C_PHONE | char[11] C_SINCE | char[2] C_CREDIT
    // | char offset #1 | char offset #2 | char offset #3
    // | char offset #4 | char offset #5 | int offset #6
    // | varchar(8-16) C_FIRST | varchar(10-16) C_LAST
    // | varchar(10-20) C_STREET_1 | varchar(10-20) C_STREET_2
    // | varchar(10-20) C_CITY
    // | varchar(300-500) C_DATA

    /*==========================================+
      Horizontica INSERT INTO
     +==========================================*/

    int tuple_size = 6*sizeof(long) + 3*sizeof(float) + 47 + sizeof(int) +
      cfirstsiz + c_last_size + cstreet1siz + cstreet2siz + ccitysiz + cdatasiz;
    table_size += tuple_size;

    temp_tuple = tb_->allocateTuple(tuple_size);

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
    memcpy(temp_tuple+offset, c_middle, 2);
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
    *(temp_tuple+offset) = (char)(offset + 4 + sizeof(int) +
                                  cfirstsiz + c_last_size);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 3 + sizeof(int) + cfirstsiz +
                                  c_last_size + cstreet1siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 2 + sizeof(int) + cfirstsiz +
                                  c_last_size + cstreet1siz + cstreet2siz);
    offset += 1;
    *(temp_tuple+offset) = (char)(offset + 1 + sizeof(int) + cfirstsiz +
                                  c_last_size + cstreet1siz + cstreet2siz + 
                                  ccitysiz);
    offset += 1;
    *((int*)(temp_tuple+offset)) = offset + sizeof(int) + cfirstsiz +
                                   c_last_size + cstreet1siz + cstreet2siz +
                                   ccitysiz + cdatasiz;
    offset += sizeof(int);
    memcpy(temp_tuple+offset, c_first, cfirstsiz);
    offset += cfirstsiz;
    memcpy(temp_tuple+offset, c_last, c_last_size);
    offset += c_last_size;
    memcpy(temp_tuple+offset, c_street_1, cstreet1siz);
    offset += cstreet1siz;
    memcpy(temp_tuple+offset, c_street_2, cstreet2siz);
    offset += cstreet2siz;
    memcpy(temp_tuple+offset, c_city, ccitysiz);
    offset += ccitysiz;
    memcpy(temp_tuple+offset, c_data, cdatasiz);

    int key = CUSTOMERkey(c_id, c_d_id, c_w_id);
    customer_tree_.insert(key, temp_tuple);

    int hashKeySize = c_last_size + sizeof(long) + sizeof(long);
    byte* hash_key = hash_keys_->allocateTuple(hashKeySize);
    memcpy(hash_key, c_last, c_last_size);
    memcpy(hash_key + c_last_size, temp_tuple + sizeof(long), 2*sizeof(long));
    cust_hi_->put(hash_key, hashKeySize, temp_tuple, tuple_size);

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
    history_table_size_ += h_tuple_size;

    byte* h_temp_tuple = history_buffer_->allocateTuple(h_tuple_size);

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

    // TODO: add proper timestamp.
    memcpy(h_temp_tuple+offset, c_since, 11);
    offset += 11;
    *(h_temp_tuple+offset) = (char)(offset + 1 + hdatasiz);
    offset += 1;
    memcpy(h_temp_tuple+offset, h_data, hdatasiz);

    // Do not insert into B tree since no unique key.
  }

  return table_size;
}

void WorkerThread::InitPermutation() {
  int i;
  perm_count_ = 0;

  // Init with consecutive values
  for(i=0; i < CUST_PER_DIST; i++) {
    perm_c_id_[i] = i+1;
  }

  // shuffle
  for(i=0; i < CUST_PER_DIST-1; i++) {
    int j = (int) RandomNumber(i+1, CUST_PER_DIST-1);
    long tmp = perm_c_id_[i];
    perm_c_id_[i] = perm_c_id_[j];
    perm_c_id_[j] = tmp;
  }
}

long WorkerThread::GetPermutation() {
  if(perm_count_ >= CUST_PER_DIST) {
    // wrapped around, restart at 0
    perm_count_ = 0;
  }
  return (long) perm_c_id_[perm_count_++];
}

// Load individual orders for a given warehouse and district.
void WorkerThread::LoadIndividualOrders(long w_id, long d_id) {
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

  o_d_id=d_id;
  o_w_id=w_id;
  InitPermutation(); /* initialize permutation of customer numbers */

  // TODO: Hack.
  lowest_neworder_id_[(int)(w_id * DIST_PER_WARE + d_id)] = 2101;

  for (o_id = 1; o_id <= ORD_PER_DIST; o_id++) {
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

      // TODO (hack): we represent NULL as 0
      o_carrier_id = 0;

      // NEW_ORDER tuple
      // | long NO_O_ID | long NO_D_ID | long NO_W_ID

      /*==========================================+
        Horizontica INSERT INTO
       +==========================================*/

      int no_tuple_size = 3 * sizeof(long);
      neworder_table_size_ += no_tuple_size;  

      temp_tuple = tb_->allocateTuple(no_tuple_size);

      offset = 0;
      *((long*)temp_tuple) = o_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = o_d_id;
      offset += sizeof(long);
      *((long*)(temp_tuple+offset)) = o_w_id;

      int hashKeySize = 3 * sizeof(long);
      byte* hash_key = hash_keys_->allocateTuple(hashKeySize);
      // hash_key is (o_id, o_d_id, o_w_id)
      memcpy(hash_key, temp_tuple, 3 * sizeof(long));
      neword_hi_->put(hash_key, hashKeySize, temp_tuple, no_tuple_size);
    } else {}
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
    order_table_size_ += o_tuple_size;

    temp_tuple = tb_->allocateTuple(o_tuple_size);

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
    // TODO add timestamp here.
    memcpy(temp_tuple+offset, "2007-01-01:", 11);

    int o_key = ORDERSkey(o_id, o_d_id, o_w_id);
    orders_tree_.insert(o_key, temp_tuple);

    int hashKeySize = 3 * sizeof(long);
    byte* hash_key = hash_keys_->allocateTuple(hashKeySize);
    // hash_key is (o_c_id, o_d_id, o_w_id)
    memcpy(hash_key, temp_tuple + sizeof(long), 3 * sizeof(long));
    ord_hi_->put(hash_key, hashKeySize, temp_tuple, o_tuple_size);

    for (ol=1; ol<=o_ol_cnt; ol++) {
      /* Generate Order Line Data */
      ol_i_id=RandomNumber(1L,MAXITEMS);
      ol_supply_w_id=o_w_id;
      ol_quantity=5;
      ol_amount=0.0; 
      MakeAlphaString(24,24,ol_dist_info);
      char ol_delivery_d[11];
      // TODO add timestamp here.
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

        // TODO (hack): we represent NULL timestamp as "0"
        ol_delivery_d[0] = 0;  
        ol_delivery_d[1] = '\0';  
      } else {
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
      // | long OL_I_ID | long OL_SUPPLY_W_ID | long OL_QUANTITY
      // | float OL_AMOUNT
      // | char[24] OL_DIST_INFO | char[11] OL_DELIVERY_D 

      /*==========================================+
        Horizontica INSERT INTO
       +==========================================*/

      int ol_tuple_size = 7*sizeof(long) + sizeof(float) + 24 + 11;
      orderline_table_size_ += ol_tuple_size;

      temp_tuple = tb_->allocateTuple(ol_tuple_size);

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
      orderline_tree_.insert(ol_key, temp_tuple);
    }
  }
}

// Pthread run method for all worker threads.
void *RunWorker(void *thread_info) {
  WorkerThread new_worker((ThreadInfo*)thread_info);
  return new_worker.Run();
}

// Global (shared) method:
// Load individual stocks for a warehouse into shared Stock table.
long LoadIndividualStocks(long w_id, char **stock_array,
                          TupleBuffer *global_tb) {
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
  s_w_id=w_id;

  for (i=0; i<MAXITEMS; i++)
    orig[i]=0;
  for (i=0; i<MAXITEMS/10; i++) {
    do {
      pos=RandomNumber(0L,MAXITEMS-1); // TODO: diff from TPC-C
    } while (orig[pos]);
    orig[pos] = 1;
  }

  for (s_i_id=1; s_i_id<=MAXITEMS; s_i_id++) {
    /* Generate Stock Data */
    s_quantity = RandomNumber(10L,100L);
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
    sdatasiz = MakeAlphaString(26,50,s_data);

    if (orig[s_i_id - 1]) { // TODO: diff from TPC-C
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

    // STOCK tuple
    // | long S_I_ID | long S_W_ID | long S_QUANTITY
    // | float S_YTD | int S_CNT_ORDER | int S_CNT_REMOTE
    // | char[24] S_DIST_01 | char[24] S_DIST_02 | char[24] S_DIST_03
    // | char[24] S_DIST_04
    // | char[24] S_DIST_05 | char[24] S_DIST_06 | char[24] S_DIST_07
    // | char[24] S_DIST_08
    // | char[24] S_DIST_09 | char[24] S_DIST_10 | int offset
    // | varchar(26-50) S_DATA 

    /*==========================================+
      Horizontica INSERT INTO
     +==========================================*/

    int tuple_size = 3 * sizeof(long) + sizeof(float) + 
                     3 * sizeof(int) + 240 + sdatasiz;
    table_size += tuple_size;

    temp_tuple = global_tb->allocateTuple(tuple_size);

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

    stock_array[(int)STOCKkey(s_i_id, s_w_id)] = (char*)temp_tuple;
  }

  return table_size;
}

long SafeRound(long N, int p) {
  return (long)ceil(N/p);
}

// Generate table data on each partition.
void WorkerThread::GenerateData() {
  // Init permutation state.
  perm_count_ = 0;
  perm_c_id_.reset(new long[CUST_PER_DIST]);

  // Allocate buffers.
  tb_.reset(new TupleBuffer(
      SafeRound(1600000000, thread_info_->num_parts_), 1));
  history_buffer_.reset(new TupleBuffer(
      SafeRound(400000000, thread_info_->num_parts_), 2));
  hash_keys_.reset(new TupleBuffer(
      SafeRound(100000000, thread_info_->num_parts_), 3));

  // Prepare hash indices.
  cust_hi_.reset(new HashIndex(
      SafeRound(thread_info_->num_warehouses_*120000, 
                thread_info_->num_parts_)));
  ord_hi_.reset(new HashIndex(
      SafeRound(thread_info_->num_warehouses_*600000,
                thread_info_->num_parts_)));
  neword_hi_.reset(new HashIndex(
      SafeRound(thread_info_->num_warehouses_*180000,
                thread_info_->num_parts_)));

  hf_.reset(new StringHashFunction());
  cust_hi_->setHashFunction(hf_.get());
  ord_hi_->setHashFunction(hf_.get());
  neword_hi_->setHashFunction(hf_.get());

  // Load lowest new order id table (hack: TODO fix).
  lowest_neworder_id_.reset(
      new long[(thread_info_->num_warehouses_ + 1) * DIST_PER_WARE]);

  // Load all tables (except possibly Stock table).
  cout << "Partition " << thread_info_->p_ << ": Loading items" << "\n";
  LoadItems();

  cout << "Partition " << thread_info_->p_ << ": Loading warehouses" << "\n";
  LoadWarehouses();

  cout << "Partition " << thread_info_->p_ << ": Loading customers" << "\n";
  LoadCustomers();
  
  cout << "Partition " << thread_info_->p_ << ": Loading orders" << "\n";
  LoadOrders();
}

// Load data, notify master, and then wait for transactions.
void* WorkerThread::Run() {
  // Generate table data for this partition according to TPC-C.
  GenerateData();

  cout << "Partition " << thread_info_->p_ 
       << ": Done generating data; signalling master..." << "\n";

  // Done loading: signal master.
  thread_info_->load_complete_ = true;

  // Wait on input queue in loop, consume each burst of txns
  // and process them in order.
  //pthread_mutex_lock(thread_info_->queue_mutex_);
  //while (true)
    // Process next burst of txns from master.
    //while (!thread_info_->in_queue_->empty())
    //  char *recv_buffer = thread_info_->in_queue_->front();
    //  thread_info_->in_queue_->pop();
    //  pthread_mutex_unlock(thread_info_->queue_mutex_);
    while (true) {
      char *recv_buffer;
      thread_info_->in_queue_->pop(recv_buffer);
      // Process transaction tx_command.
      // Again, several cases depending on opcode, and whether
      // this is the local or remote thread being targeted.
      int msg_len = ((int*)recv_buffer)[0];
      int opcode = ((int*)recv_buffer)[1];
      long w_id = *((long*)(recv_buffer + 2 * sizeof(int)));

      //printf("Debug: Thread %d: recv_buffer %p len=%d\n",
      //        thread_info_->p_, recv_buffer, ((int*)recv_buffer)[0]);
      switch (opcode) {
        case ORDSTAT_OPCODE: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          bool byname = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          char *c_last = recv_buffer + 2 * sizeof(int) + 
                         3 * sizeof(long) + sizeof(bool);
          int c_last_size = ((int*)(recv_buffer + msg_len))[0];
          //printf("Thread %d: msg_len=%d opcode=%d buffer=%p ExecuteOrdstatTx(w_id=%ld, d_id=%ld, c_id=%ld)\n",
          //        thread_info_->p_, msg_len, opcode, recv_buffer, w_id, d_id, c_id);
          ExecuteOrdstatTx(w_id, d_id, c_id, byname, c_last, c_last_size);
        }
        break;

        case SLEV_OPCODE: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long threshold = *((long*)(recv_buffer + 2 * sizeof(int) +
                                     2 * sizeof(long)));
          ExecuteSlevTx(w_id, d_id, threshold);
        }
        break;

        case DELIVERY_OPCODE: {
          long o_carrier_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                        sizeof(long)));
          ExecuteDeliveryTx(w_id, o_carrier_id);
        }
        break;

        case PAYMENT_LOCAL_OPCODE: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long c_w_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          long c_d_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long)));
          bool byname = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                  5 * sizeof(long)));
          char *c_last = recv_buffer + 2 * sizeof(int) +
                         5 * sizeof(long) + sizeof(bool);
          int c_last_size = *((int*)(recv_buffer + msg_len - sizeof(long)));
          long h_amount = *((long*)(recv_buffer + msg_len +
                                    sizeof(int) - sizeof(long)));
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteLocalPaymentTx(w_id=%ld, d_id=%ld, c_id=%ld)\n",
          //        thread_info_->p_, msg_len, recv_buffer, w_id, d_id, c_id);
          ExecuteLocalPaymentTx(w_id, d_id, c_id, c_w_id, c_d_id,
                                byname, c_last, c_last_size, h_amount);
        }
        break;

        case PAYMENT_REMOTE_HOME: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long c_w_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          long c_d_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long)));
          bool byname = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                  5 * sizeof(long)));
          char *c_last = recv_buffer + 2 * sizeof(int) +
                         5 * sizeof(long) + sizeof(bool);
          int c_last_size = *((int*)(recv_buffer + msg_len - sizeof(long)));
          long h_amount = *((long*)(recv_buffer + msg_len + 
                                    sizeof(int) - sizeof(long)));
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteRemotePaymentHomeTx(w_id=%ld, d_id=%ld, c_id=%ld, c_w_id=%ld, c_d_id=%ld)\n",
          //        thread_info_->p_, msg_len, recv_buffer, w_id, d_id, c_id, c_w_id, c_d_id);
          ExecuteRemotePaymentHomeTx(w_id, d_id, c_id, c_w_id, c_d_id,
                                     byname, c_last, c_last_size, h_amount);
        }
        break;

        case PAYMENT_REMOTE_AWAY: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long c_w_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          long c_d_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long)));
          bool byname = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                  5 * sizeof(long)));
          char *c_last = recv_buffer + 2 * sizeof(int) +
                         5 * sizeof(long) + sizeof(bool);
          int c_last_size = *((int*)(recv_buffer + msg_len - sizeof(long)));
          long h_amount = *((long*)(recv_buffer + msg_len +
                                    sizeof(int) - sizeof(long)));
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteRemotePaymentAwayTx(w_id=%ld, d_id=%ld, c_id=%ld, c_w_id=%ld, c_d_id=%ld)\n",
          //        thread_info_->p_, msg_len, recv_buffer, w_id, d_id, c_id, c_w_id, c_d_id);
          ExecuteRemotePaymentAwayTx(w_id, d_id, c_id, c_w_id, c_d_id,
                                     byname, c_last, c_last_size, h_amount);
        }
        break;

        case NEWORDER_LOCAL_OPCODE: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long ol_cnt = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          bool all_local = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                     4 * sizeof(long)));
          long *item_id = (long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long) + sizeof(bool));
          long *supware = item_id + ol_cnt;
          long *quantity = supware + ol_cnt;

          // Increment count of committed txns if this returns true.
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteLocalNewOrderTx\n",
          //        thread_info_->p_, msg_len, recv_buffer);
          if (ExecuteLocalNewOrderTx(w_id, d_id, c_id, ol_cnt, all_local,
                                     item_id, supware, quantity)) {
            commit_count_++;
          }
        }
        break;

        case NEWORDER_REMOTE_HOME: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long ol_cnt = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          bool all_local = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                     4 * sizeof(long)));
          long *item_id = (long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long) + sizeof(bool));
          long *supware = item_id + ol_cnt;
          long *quantity = supware + ol_cnt;

          // Increment count of committed txns if this returns true.
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteRemoteNewOrderHomeTx\n",
          //        thread_info_->p_, msg_len, recv_buffer);
          if (ExecuteRemoteNewOrderHomeTx(w_id, d_id, c_id, ol_cnt, all_local,
                                          item_id, supware, quantity)) {
            commit_count_++;
          }
        }
        break;

        case NEWORDER_REMOTE_AWAY: {
          long d_id = *((long*)(recv_buffer + 2 * sizeof(int) + sizeof(long)));
          long c_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                                2 * sizeof(long)));
          long ol_cnt = *((long*)(recv_buffer + 2 * sizeof(int) +
                                  3 * sizeof(long)));
          bool all_local = *((bool*)(recv_buffer + 2 * sizeof(int) +
                                     4 * sizeof(long)));
          long *item_id = (long*)(recv_buffer + 2 * sizeof(int) +
                                  4 * sizeof(long) + sizeof(bool));
          long *supware = item_id + ol_cnt;
          long *quantity = supware + ol_cnt;
          //printf("Thread %d: msg_len=%d buffer=%p ExecuteRemoteNewOrderAwayTx\n",
          //        thread_info_->p_, msg_len, recv_buffer);
          ExecuteRemoteNewOrderAwayTx(w_id, d_id, c_id, ol_cnt, all_local,
                                      item_id, supware, quantity);
        }
        break;

        case QUIT_OPCODE: {
          //printf("Partition %d: saw QUIT\n", thread_info_->p_);
          // Sanity check: input queue should be empty.
          assert (thread_info_->in_queue_->empty());
          // Return commit count, terminate thread.
          //printf("Partition %d returning commit count %d\n", thread_info_->p_, commit_count_);
          return new int(commit_count_);
        }
      }

      // pthread_mutex_lock(thread_info_->queue_mutex_);
    }

    //cout << "Partition " << thread_info_->p_ << ": Input queue empty, waiting on CV";
    //printf(" %p\n", thread_info_->queue_cond_);
    // Queue empty: wait on master to give us more.
    //pthread_cond_wait(thread_info_->queue_cond_,
    //                  thread_info_->queue_mutex_);
    //cout << "Partition " << thread_info_->p_ << ": Woke up on CV signal" << "\n";
  // Add curly brace here
}

// Producer process: add buffer atomically to queue and signal thread.
inline void AddToQueue(ThreadInfo *thread_info, int p, char *buffer) {
  //pthread_mutex_lock(thread_info[p].queue_mutex_);
  // Transfer ownership of buffer to remote thread.
  (thread_info[p].in_queue_)->push(buffer);
  //pthread_cond_signal(thread_info[p].queue_cond_);
  //pthread_mutex_unlock(thread_info[p].queue_mutex_);
}

// Main Master Server Program.
int main(int argc, char **argv) {
  const int LOAD_WAIT_SLEEP = 5;

  // Read the following parameters from the master command line:
  // 1. Number of TPC-C warehouses.
  // 2. Number of partitions for data (= #threads to run).
  if (argc != 3) {
    cout << "Usage - ./master-threads "
         << "[#warehouses (must match value on driver)] "
         << "[#partitions (threads)]"
         << "\n";
    return -1;
  }
  
  int num_warehouses = atoi(argv[1]);
  int num_partitions = atoi(argv[2]);
  assert (num_warehouses > 0 && num_partitions > 0 &&
          num_warehouses % num_partitions == 0);

  int warehouses_per_partition = num_warehouses/num_partitions;

  // Start num_partitions threads and generate random warehouse data
  // as per TPC-C spec. Each thread needs to maintain an input queue
  // of transactions to process. Each transaction in the queue is
  // executed to completion by the thread it is assigned to.
  scoped_array<pthread_t> worker_threads(new pthread_t[num_partitions]);
  ThreadInfo *thread_info = new ThreadInfo[num_partitions];

  // Create STOCK table: array index is STOCK key (s_i_id, s_w_id)
  scoped_array<char *> stock_array(
      new char *[(num_warehouses + 1) * MAXITEMS + 1]);

  // Load data in shared stock table.
  long stock_table_size = 0;
  scoped_ptr<TupleBuffer> global_tb(new TupleBuffer(500000000, 0));
  for (long w_id = 1L; w_id <= num_warehouses; ++w_id) {
    stock_table_size +=
        LoadIndividualStocks(w_id, stock_array.get(), global_tb.get());
  }

  // Load thread_info structures and create specified # of threads.
  for (int p = 0; p < num_partitions; ++p) {
    thread_info[p].num_warehouses_ = num_warehouses;
    thread_info[p].num_parts_ = num_partitions;
    thread_info[p].p_ = p;
    thread_info[p].stock_array_ = stock_array.get();
    thread_info[p].stock_table_size_ = stock_table_size;
    thread_info[p].load_complete_ = false;
    //thread_info[p].in_queue_.reset(new queue<char*>);
    thread_info[p].in_queue_.reset(new concurrent_queue<char*>);
    thread_info[p].queue_cond_ = new pthread_cond_t;
    pthread_cond_init(thread_info[p].queue_cond_, NULL);
    thread_info[p].queue_mutex_ = new pthread_mutex_t;
    pthread_mutex_init(thread_info[p].queue_mutex_, NULL);
    pthread_create(worker_threads.get() + p, NULL,
                   RunWorker, (thread_info + p));
  }

  // Wait for data to load in all threads.
  cout << "Master: waiting for data to load in all threads" << "\n";
  while (true) {
    bool all_loads_complete = true;
    for (int p = 0; p < num_partitions; ++p) {
      if (!thread_info[p].load_complete_) {
        all_loads_complete = false;
        break;
      }
    }
    if (all_loads_complete) break;
    sleep(LOAD_WAIT_SLEEP);
  }

  // Start server socket, listen for transaction messages from
  // driver, decode them and despatch tasks to appropriate
  // thread's input queue. Also split up transactions that touch
  // multiple partitions into task pieces that run on each
  // partition they touch.
  cout << "Master: All threads have finished loading data." << "\n"
       << "Master: Starting server to wait for requests..." << "\n";

  scoped_ptr<StopWatch> sw(new StopWatch());
  int64_t start_time = 0, end_time = 0;

  // Tuple Buffer for received messages.
  scoped_ptr<TupleBuffer> msg_buffer(
      new TupleBuffer(MAX_TRANSACTIONS*2000, 4));

  try {
    scoped_ptr<ServerSocket> server(new ServerSocket(MASTER_PORT));
    ServerSocket connection_to_driver;
    server->accept(connection_to_driver);
    sw->start();

    int cnt = 0;
    bool saw_quit = false;
    while (true) {
      char *recv_buffer = (char*)msg_buffer->allocateTuple(SOCK_BATCH_SIZE);
      int num_tx = connection_to_driver.recvN(recv_buffer);
      //assert (num_tx >= 1);

      for (int t = 0; t < num_tx; ++t) {
        int msg_len = ((int*)recv_buffer)[0] + sizeof(int);
        int opcode = ((int*)recv_buffer)[1];

        //printf("Master: Received tx %d buffer %p: opcode %d len %d\n", cnt++, recv_buffer, opcode, msg_len);
        if (opcode == QUIT_OPCODE) {
          // Signal all threads that they should compute stats and quit.
          for (int p = 0; p < num_partitions; ++p) {
            char *recv_buffer_copy =
                (char*)msg_buffer->allocateTuple(2*sizeof(int));
            memmove(recv_buffer_copy, recv_buffer, 2*sizeof(int));
            //cout << "Master: QUIT_OPCODE: Signalling CV " << p << "\n";
            AddToQueue(thread_info, p, recv_buffer_copy);
          }
          saw_quit = true;
          break;
        }

        long w_id = *((long*)(recv_buffer + 2 * sizeof(int)));
        int p = (int)((w_id-1)/warehouses_per_partition);
        //printf("tx %d: w_id = %ld p = %d\n", cnt-1, w_id, p);
        //printf("Debug: tx %d: recv_buffer %p len=%d\n", cnt-1, recv_buffer, ((int*)recv_buffer)[0]);
        // Route received transaction to appropriate thread(s).
        // Most txns will be confined to a single thread, but some
        // will necessarily span multiple threads.
        if (opcode == PAYMENT_REMOTE_OPCODE) {
          // Remote tx: Split, and route to remote queue.
          ((int*)recv_buffer)[1] = PAYMENT_REMOTE_HOME;
          // There is only one remote queue for a remote payment tx.
          // And it could be the same as the local queue.
          // TODO: Optimize this if necessary.
          long remote_w_id = *((long*)(recv_buffer + 2 * sizeof(int) +
                3 * sizeof(long)));
          int remote_p = (int)((remote_w_id-1)/warehouses_per_partition);
          char *recv_buffer_copy =
              (char*)msg_buffer->allocateTuple(msg_len);
          memmove(recv_buffer_copy, recv_buffer, msg_len);
          ((int*)recv_buffer_copy)[1] = PAYMENT_REMOTE_AWAY;
          AddToQueue(thread_info, remote_p, recv_buffer_copy);
        } else if (opcode == NEWORDER_REMOTE_OPCODE) {
          // Split, and route to all remote queues affected by this tx.
          ((int*)recv_buffer)[1] = NEWORDER_REMOTE_HOME;
          // Right now we have multiple messages for each thread, unfortunately,
          // because multiple warehouses can be managed by one thread.
          // TODO: Combine these somehow? Seems like a low-level optimization
          // but could potentially be important if malloc overhead is high.
          size_t common_size = 2 * sizeof(int) + 4 * sizeof(long) + sizeof(bool);
          long ol_cnt = *((long*)(recv_buffer + common_size - 
                sizeof(long) - sizeof(bool)));
          long *item_id = (long*)(recv_buffer + common_size);
          long *supware = item_id + ol_cnt;
          long *quantity = supware + ol_cnt;

          for (long i = 0; i < ol_cnt; ++i) {
            char *recv_buffer_copy =
                (char*)msg_buffer->allocateTuple(msg_len);
            memmove(recv_buffer_copy, recv_buffer, msg_len);
            ((int*)recv_buffer_copy)[1] = NEWORDER_REMOTE_AWAY;
            int remote_p = (int)((supware[i]-1)/warehouses_per_partition);
            AddToQueue(thread_info, remote_p, recv_buffer_copy);
          }
        }

        // Always need to route to local thread.
        // Transfer ownership of recv_buffer to thread p.
        //printf("Debug: tx %d: recv_buffer %p len=%d\n", cnt-1, recv_buffer, ((int*)recv_buffer)[0]);
        AddToQueue(thread_info, p, recv_buffer);

        // Move receive buffer.
        //printf("Inc recv_buffer %p by %d to %p\n", recv_buffer, msg_len,
        //        recv_buffer+msg_len);
        recv_buffer += msg_len;
      }

      if (saw_quit) break;
    }

    // Wait for all threads to terminate, and collect stats.
    // Return commit count to driver.
    scoped_array<int> commit_count_msg(new int[2]);
    commit_count_msg[0] = sizeof(int);
    commit_count_msg[1] = 0;
    printf("Master: waiting for join...\n");
    for (int p = 0; p < num_partitions; ++p) {
      void *committed_tx;
      //pthread_cond_signal(thread_info[p].queue_cond_);
      pthread_join(worker_threads[p], &committed_tx);
      commit_count_msg[1] += *((int*)committed_tx);
      delete ((int*)committed_tx);
    }
    connection_to_driver << ((char*)commit_count_msg.get());

  } catch ( SocketException& e ) {
    cout << "Caught socket exception:" << e.description() << "\n";
    exit(-1);
  }

  sw->stop();

  for (int p = 0; p < num_partitions; ++p) {
    delete thread_info[p].queue_cond_;
    delete thread_info[p].queue_mutex_;
  }

  delete [] thread_info;

  printf("Master: start time = %lld end = %lld diff = %lld\n", 
      start_time, end_time, end_time - start_time);
  return 0;
}
