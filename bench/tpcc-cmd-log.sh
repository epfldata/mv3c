#!/bin/sh

if [ $# -lt 3 ]; then
    echo "Usage: $0 NUMXACTS WAREHOUSES OUTPUTFILE SHOULD_EXECUTE_IN_MYSQL"
    exit 1
fi

NUMXACTS=$1
WAREHOUSES=$2
EXECUTE=1

if [ -z "$4" ]; then
		EXECUTE=0
fi

echo "Generating $NUMXACTS commands for $WAREHOUSES warehouses"
cd `dirname $0`; BENCH=`pwd`; cd ..; BASE=`pwd`
echo "Writing commands to `pwd`"
eval "sbt package"
eval "bench/bench load $WAREHOUSES"

if [ $EXECUTE -eq 0 ]; then
	#-----------------------------
	#Only commands
	eval "scala -J-Xmx8g -cp .:./lib/*:./target/scala-2.11/tstore_2.11-0.1.jar ddbt.tpcc.loadtest.TpccCommandGen -w $WAREHOUSES -n $NUMXACTS > $3" 
else
	#------------------------
	#Results
	eval "scala -J-Xmx4g -cp .:./lib/*:./target/scala-2.11/tstore_2.11-0.1.jar ddbt.tpcc.loadtest.TpccUnitTest -i -2 -w $WAREHOUSES -n $NUMXACTS 2>$3"
	eval "bench/bench save $WAREHOUSES"
fi
