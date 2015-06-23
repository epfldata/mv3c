ddbt.tpcc.tx1: contains base implementation for TPC-C
ddbt.tpcc.tx2: implementation with indices on key (by adding slice operation on maps)
ddbt.tpcc.tx3: removing duplicate accesses to an element in the HashMap (for reading and then updating a tuple)
ddbt.tpcc.tx4: implementation with indices on the combination of key and value (for more efficient slice operations)
ddbt.tpcc.tx5: specific data-structures for Min-Max queries (using SortedSet instead of HashSet)
ddbt.tpcc.tx6: using SharedData only through its interface
ddbt.tpcc.tx7: again, using generic data structures for implementing MVCC. This version only contains the stubs for implementing MVCC.
ddbt.tpcc.tx8: tx7 with MVCC implementation - not complete yet
ddbt.tpcc.tx9: tx7 with the initial version of ConcurrentSHMap implementation 
ddbt.tpcc.tx10: tx9 with MVCC implementation 
ddbt.tpcc.tx11: tx10 with MVC3T implementation 