/* TPC-C driver that issues transactions to multithreaded master.
 * This driver does not automatically load-balance across partitions.
 * Instead, it randomly generates transactions according to the
 * TPC-C spec, and sends them to the master, which dispatches them to
 * individual cores/partitions to handle.
 *
 * This reflects reality more than the current implementation,
 * and also cleanly separates actions which need to be benchmarked
 * from others (like random record generation) which should not be.
 */

#include <iostream>
#include <string>
#include <assert.h>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

using boost::scoped_ptr;
using boost::scoped_array;

#include "ClientSocket.h"
#include "SocketException.h"
#include "Constants.h"
#include "RandStuff.h"
#include "StopWatch.h"

using namespace std;

// Methods to generate transactions in accordance with TPC-C.

// Generate ordstat transaction.
void GenOrdstatTx(int num_warehouses, char* tx_command) {
  int opcode = ORDSTAT_OPCODE;

  long w_id = RandomNumber(1, num_warehouses);
  long d_id = RandomNumber(1, DIST_PER_WARE);
  long c_id = NURand(1023, 1, CUST_PER_DIST);
  
  char c_last[LASTNAME_LEN];
  memset(c_last, 0, LASTNAME_LEN);
  int c_last_size;
  bool byname;

  long y = RandomNumber(1, 100);
  if (y <= BYNAME_PROB) {
    // Order status selection by last name.
    byname = true;

    // Generate last name, find its size.
    c_last_size = Lastname(NURand(255, 0, 999), c_last);
  } else {
    // Order status selection by customer id.
    byname = false;
  }

  size_t cp_offset = sizeof(int);
  relay_copy(tx_command, &cp_offset, &opcode, sizeof(int));
  relay_copy(tx_command, &cp_offset, &w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &d_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &c_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &byname, sizeof(bool));
  relay_copy(tx_command, &cp_offset, c_last, LASTNAME_LEN);
  relay_copy(tx_command, &cp_offset, &c_last_size, sizeof(int));

  // Fill in size (INCLUDING opcode).
  *((int*)tx_command) = cp_offset - sizeof(int);
}

// Generate stock level transaction.
void GenSlevTx(int num_warehouses, char* tx_command) {
  int opcode = SLEV_OPCODE;

  long w_id = RandomNumber(1, num_warehouses);
  long d_id = RandomNumber(1, DIST_PER_WARE);
  long threshold = RandomNumber(SLEV_THRESHOLD_LL, SLEV_THRESHOLD_UL);

  size_t cp_offset = sizeof(int);
  relay_copy(tx_command, &cp_offset, &opcode, sizeof(int));
  relay_copy(tx_command, &cp_offset, &w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &d_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &threshold, sizeof(long));

  // Fill in size (INCLUDING opcode).
  *((int*)tx_command) = cp_offset - sizeof(int);
}

// Generate delivery transaction.
void GenDeliveryTx(int num_warehouses, char* tx_command) {
  int opcode = DELIVERY_OPCODE;

  long w_id = RandomNumber(1, num_warehouses);
  long o_carrier_id = RandomNumber(1, NUM_CARRIERS);

  size_t cp_offset = sizeof(int);
  relay_copy(tx_command, &cp_offset, &opcode, sizeof(int));
  relay_copy(tx_command, &cp_offset, &w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &o_carrier_id, sizeof(long));

  // Fill in size (INCLUDING opcode).
  *((int*)tx_command) = cp_offset - sizeof(int);
}

// Generate payment transaction.
void GenPaymentTx(int num_warehouses, char* tx_command) {
  long w_id = RandomNumber(1, num_warehouses);
  long d_id = RandomNumber(1, DIST_PER_WARE);
  long c_id = NURand(1023, 1, CUST_PER_DIST);
  long h_amount = RandomNumber(MIN_PAYMENT_AMOUNT, MAX_PAYMENT_AMOUNT);
  
  // Choose remote vs local and id vs last name selection.
  long x = RandomNumber(1, 100);
  long y = RandomNumber(1, 100);
  
  long c_w_id;
  long c_d_id;
  char c_last[LASTNAME_LEN];
  memset(c_last, 0, LASTNAME_LEN);
  int c_last_size;
  bool byname, all_local = true;

  if (x <= 100 - PAYMENT_REMOTE_PROPORTION) {
    // Local: confined to a single site.
    c_d_id = d_id;
    c_w_id = w_id;
  } else {
    // Remote: multiple sites.
    c_d_id = RandomNumber(1, DIST_PER_WARE);
    if (num_warehouses > 1) {
      while ((c_w_id = RandomNumber(1, num_warehouses)) == w_id);
      all_local = false;
    }
  }

  int opcode = (all_local) ? PAYMENT_LOCAL_OPCODE : PAYMENT_REMOTE_OPCODE;

  if (y <= BYNAME_PROB) {
    // Order status selection by last name.
    byname = true;

    // Generate last name, find its size.
    c_last_size = Lastname(NURand(255, 0, 999), c_last);
  } else {
    // Order status selection by customer id.
    byname = false;
  }

  size_t cp_offset = sizeof(int);
  relay_copy(tx_command, &cp_offset, &opcode, sizeof(int));
  relay_copy(tx_command, &cp_offset, &w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &d_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &c_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &c_w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &c_d_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &byname, sizeof(bool));
  relay_copy(tx_command, &cp_offset, c_last, LASTNAME_LEN);
  relay_copy(tx_command, &cp_offset, &c_last_size, sizeof(int));
  relay_copy(tx_command, &cp_offset, &h_amount, sizeof(long));

  // Fill in size (INCLUDING opcode).
  *((int*)tx_command) = cp_offset - sizeof(int);
}

// Generate new order transaction.
void GenNewOrderTx(int num_warehouses, char* tx_command) {
  long w_id = RandomNumber(1, num_warehouses);
  long d_id = RandomNumber(1, DIST_PER_WARE);
  long c_id = NURand(1023, 1, CUST_PER_DIST);
  long ol_cnt = RandomNumber(NEWORDER_MIN_ITEMS, NEWORDER_MAX_ITEMS);

  scoped_array<long> item_id(new long[NEWORDER_MAX_ITEMS]);
  scoped_array<long> supware(new long[NEWORDER_MAX_ITEMS]);
  scoped_array<long> quantity(new long[NEWORDER_MAX_ITEMS]);

  // Simulate transactions with erroneous retry.
  int rbk = (int)RandomNumber(1, 100);
  long bad_id = MAXITEMS + 1;
  bool all_local = true;

  for (long i = 0; i < ol_cnt; ++i) {
    if (i == (ol_cnt - 1) && rbk <= PERCENT_BAD_ITEMS) {
      item_id[i] = bad_id;
    } else {
      item_id[i] = NURand(8191, 1, MAXITEMS);
    }

    // Choose remote vs local warehouse.
    if (RandomNumber(1, 100) > NEWORDER_REMOTE_PROPORTION) {
      supware[i] = w_id;
    } else {
      // Remote warehouse.
      if (num_warehouses == 1) {
        supware[i] = w_id;
      } else {
        while ((supware[i] = RandomNumber(1, num_warehouses)) == w_id);
        all_local = false;
      }
    }

    quantity[i] = RandomNumber(MIN_QUANTITY, MAX_QUANTITY);
  }

  int opcode = (all_local) ? NEWORDER_LOCAL_OPCODE : NEWORDER_REMOTE_OPCODE;

  size_t cp_offset = sizeof(int);
  relay_copy(tx_command, &cp_offset, &opcode, sizeof(int));
  relay_copy(tx_command, &cp_offset, &w_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &d_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &c_id, sizeof(long));
  relay_copy(tx_command, &cp_offset, &ol_cnt, sizeof(long));
  relay_copy(tx_command, &cp_offset, &all_local, sizeof(bool));
  relay_copy(tx_command, &cp_offset, item_id.get(), sizeof(long)*ol_cnt);
  relay_copy(tx_command, &cp_offset, supware.get(), sizeof(long)*ol_cnt);
  relay_copy(tx_command, &cp_offset, quantity.get(), sizeof(long)*ol_cnt);

  // Fill in size (INCLUDING opcode).
  *((int*)tx_command) = cp_offset - sizeof(int);
}

// Main Driver Program.
int main(int argc, char **argv) {
  // Read the following parameters from the driver command line:
  // 1. Number of TPC-C warehouses.
  // 2. Total number of TPC-C transactions to send to the master.
  if (argc != 3) {
    cout << "Usage - ./driver-threads "
         << "[#warehouses (must match value on master server)] "
         << "[#transactions]"
         << "\n";
    return -1;
  }

  int num_warehouses = atoi(argv[1]);
  int num_transactions = atoi(argv[2]);
  assert (num_warehouses > 0 && num_transactions > 0);

  // TODO: Precompute the required number of random numbers here.

  // Pregenerate transaction messages according to TPC-C distribution.
  scoped_array<char*> tx_commands(new char*[num_transactions+1]);

  for (int t = 0; t < num_transactions; ++t) {
    int random_tx = RandomNumber(1, 100);
    tx_commands[t] = new char[TX_SIZE];
    if (random_tx <= P_ORDSTAT) {
      /* Ordstat transaction. */
      GenOrdstatTx(num_warehouses, tx_commands[t]);
    } else if (random_tx <= P_ORDSTAT + P_STOCKLEVEL) {
      /* Stock level transaction. */
      GenSlevTx(num_warehouses, tx_commands[t]);
    } else if (random_tx <= 100 - P_NEWORDER - P_PAYMENT) {
      /* Delivery transaction. */
      GenDeliveryTx(num_warehouses, tx_commands[t]);
    } else if (random_tx <= 100 - P_NEWORDER) {
      /* Payment transaction. */
      GenPaymentTx(num_warehouses, tx_commands[t]);
    } else {
      /* New order transaction. */
      GenNewOrderTx(num_warehouses, tx_commands[t]);
    }
  }

  // Last tx is a command to quit.
  tx_commands[num_transactions] = new char[2*sizeof(int)];
  ((int*)(tx_commands[num_transactions]))[0] = sizeof(int);
  ((int*)(tx_commands[num_transactions]))[1] = QUIT_OPCODE;

  // Open a client socket to the master server.
  // Send pregenerated transactions in encoded form to the master.
  try {
    scoped_ptr<ClientSocket> client_socket(
        new ClientSocket("localhost", MASTER_PORT));

    scoped_array<char> reply(new char[2*sizeof(int)]);
    int committed = 0;

    // Start stopwatch.
    scoped_ptr<StopWatch> sw(new StopWatch());
    sw->start();

    // Send transactions.
    for (int t = 0; t <= num_transactions; ++t) {
      //printf("Sending tx %d with opcode %d, len %d\n", t,
      //        ((int*)tx_commands[t])[1],
      //        ((int*)tx_commands[t])[0] + sizeof(int));
      (*client_socket) << tx_commands[t];
    }

    // Wait for reply.
    (*client_socket) >> (reply.get());
    committed = ((int*)reply.get())[1];

    // End stopwatch, print measured TPC-C throughput.
    double elapsed = sw->stop();
    cout << "Committed " << committed << " transactions, TP = "
         << (committed*1000.0/elapsed) << "\n";

    // End of program.
  } catch (SocketException &e) {
    cout << "Exception was caught: " << e.description() << "\n";
    return -1;
  }

  return 0;
}
