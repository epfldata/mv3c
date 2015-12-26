#!/bin/sh

WAREHOUSES="2"
NUMXACTS="100"

cd `dirname $0`; BENCH=`pwd`; cd ..; BASE=`pwd`

eval "bench/bench load $WAREHOUSES"

#eval "sbt 'run-main ddbt.tpcc.loadtest.TpccCommandGen -w $WAREHOUSES -n $NUMXACTS'"
eval "sbt package"

# eval "scala -cp .:./lib/*:./target/scala-2.11/tstore_2.11-0.1.jar ddbt.tpcc.loadtest.TpccCommandGen -w $WAREHOUSES -n $NUMXACTS"

eval "scala -cp .:./lib/*:./target/scala-2.11/tstore_2.11-0.1.jar ddbt.tpcc.loadtest.TpccUnitTest -i -2 -w $WAREHOUSES -n $NUMXACTS >/dev/null"

eval "bench/bench save $WAREHOUSES"