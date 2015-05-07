#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <cstring>
typedef unsigned char byte;

// Constants for communicating messages.
#define BEGIN_PORT 30000
#define MASTER_PORT 30000
#define TX_PER_ROUND_PER_CORE 100
#define TX_SIZE (sizeof(long)*49 + 2*sizeof(int))
#define MSG_SIZE (sizeof(int) + TX_PER_ROUND_PER_CORE * TX_SIZE)

// Proportions (%) of different TPC-C transactions in workload.
#define P_ORDSTAT 4
#define P_STOCKLEVEL 4
#define P_DELIVERY 4
#define P_PAYMENT 43
#define P_NEWORDER 45

// More TPC-C Constants.
#define MAXITEMS 100000
#define CUST_PER_DIST 3000
#define DIST_PER_WARE 10
#define ORD_PER_DIST 3000
// Defined in TPC-C section 1.3.1 (page 13) and 4.3.3.1 (page 65)
#define FIRSTNAME_MINLEN 8
#define FIRSTNAME_LEN 16
#define MIDDLE_LEN 16
#define LASTNAME_LEN 16
#define BYNAME_PROB 60
#define SLEV_THRESHOLD_LL 10
#define SLEV_THRESHOLD_UL 20
#define NUM_CARRIERS 10
#define MIN_PAYMENT_AMOUNT 1
#define MAX_PAYMENT_AMOUNT 5000
#define PAYMENT_REMOTE_PROPORTION 15  // % of remote payment txns.
#define NEWORDER_MIN_ITEMS 5
#define NEWORDER_MAX_ITEMS 15
#define PERCENT_BAD_ITEMS 1
#define NEWORDER_REMOTE_PROPORTION 1 // % of remote neworder txns.
#define MIN_QUANTITY 1
#define MAX_QUANTITY 10

// Opcodes we use for TPC-C transactions.
#define ORDSTAT_OPCODE 1
#define SLEV_OPCODE 2
#define DELIVERY_OPCODE 3
#define PAYMENT_LOCAL_OPCODE 4
#define PAYMENT_REMOTE_OPCODE 5
#define NEWORDER_LOCAL_OPCODE 6
#define NEWORDER_REMOTE_OPCODE 7
#define PAYMENT_REMOTE_HOME 8
#define PAYMENT_REMOTE_AWAY 9
#define NEWORDER_REMOTE_HOME 10
#define NEWORDER_REMOTE_AWAY 11
#define QUIT_OPCODE 12

#define MAXRECV (MSG_SIZE+100)
#define SOCK_BATCH_SIZE MAXRECV

// Convenience method.
inline void relay_copy(char *dest, size_t *dest_offset,
                       void *src, size_t len) {
  memmove(dest+*dest_offset, src, len);
  *dest_offset = *dest_offset + len;
}

#endif // CONSTANTS_H
