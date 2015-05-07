Execution:
- Modify the BOOST variable in Makefile to point to the correct location (if needed)
- Go to the folder and run 'make exec' to execute benchmark
- Multiply the 'new order TP' by 60 to obtain TpmC (cf. Mohammad)

Issues:
V Increase the running time (have more TXs running).
V Incorrect implementation of B+Tree (height).
V Disabling a costly and necessary condition branch (Payment)
V Replacing a minimum lookup query with a dummy value (Delivery)
I Using fixed size structures to store unbounded relations (OrderStatus)
  Argument: because execution is single-thread, we could have a static area allocated
  for this purpose and realloc it only when it grows: after few initial transactions,
  it would have grown to appropriate size (by doubling size at every call), hence its
  performance would be that of a regular (heap or stack) array.
V Simplifying a count-distinct into a traversal of the collection, without finding distinct elements (Stock-level)
V Avoiding the creation of output data (NewOrder and OrderStatus)
