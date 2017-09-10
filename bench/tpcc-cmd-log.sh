#!/bin/sh

if [ $# -lt 3 ]; then
    echo "Usage: $0 WAREHOUSES NUMXACTS OUTPUTFILE"
    exit 1
fi

WAREHOUSES=$1
NUMXACTS=$2

cd `dirname $0`; BENCH=`pwd`; cd ..; BASE=`pwd`

#eval "sbt 'run-main ddbt.tpcc.loadtest.TpccCommandGen -w $WAREHOUSES -n $NUMXACTS'"

eval "sbt package"

echo "Creating $NUMXACTS commands for $WAREHOUSES warehouses."

eval "scala -cp .:./lib/*:./target/scala-2.11/tstore_2.11-0.1.jar ddbt.tpcc.loadtest.TpccCommandGen -w $WAREHOUSES -n $NUMXACTS" > $3

echo "The commands are generated in $3."
