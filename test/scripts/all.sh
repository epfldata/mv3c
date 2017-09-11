#!/bin/bash

######################  CONFIGURATION ##############################################
export NUMITERS=5   #Number of times an experiment is run to aggregate results
export GENDATA=0    #Generate fresh data for experiments 8 10 11
####################################################################################

ROOT_DIR=`readlink -m ../..`
echo ""
echo "Disabling hyper-threading and setting cpu governer to performance"

RUN() {
  ctr=1
  while : ; do
    timeout --foreground 15m $1 > $2
    ret=$?
    [[ $ret -eq 124 ]] || break 
    #echo -n "F"
    [[ $ctr -lt 5 ]] || break
    ctr=$((ctr+1))
  done
}
export -f RUN

source $ROOT_DIR/bench/prep/hyper-threading.sh 0


MAP_H=$ROOT_DIR/src/mapping.h
echo""
echo "Preparation complete..."

if [ -f $MAP_H ]; then
  echo "Using manually defined mapping function"
else
  if [ $FLAG1 -eq 1 ]; then
  	echo "#define THREAD_TO_CORE_MAPPING(x) x" > $MAP_H
  fi

  if [ $FLAG2 -eq 1 ]; then
  	echo "#define THREAD_TO_CORE_MAPPING(x) x<<1" > $MAP_H
  fi

  if [ $FLAG4 -eq  1 ]; then
  	echo "#define THREAD_TO_CORE_MAPPING(x) x<<2" > $MAP_H
  fi
  if [ $FLAG1 -eq 1 -o $FLAG2 -eq 1 -o $FLAG4 -eq 1 ]; then
    echo "Verify automatically inferred core mapping function in $MAP_H"
  else
    echo "Automatic inferring of core mapping failed. Manually add it in $MAP_H"  
    echo "#define THREAD_TO_CORE_MAPPING(x) x" > $MAP_H
  fi
fi
echo " "
cat $MAP_H
echo " "
echo "Stop experiment and edit file if core mapping is incorrect"
echo " "


secs=$((10))
while [ $secs -gt 0 ]; do
   echo -ne "Beginning experiments in $secs\033[0K seconds\r"
   sleep 1
   : $((secs--))
done

execute/trading-thread.sh
process/trading-thread.sh
plot/trading-thread.sh

execute/trading-conflict.sh
process/trading-conflict.sh
plot/trading-conflict.sh


execute/banking-thread.sh
process/banking-thread.sh
plot/banking-thread.sh

execute/banking-conflict.sh
process/banking-conflict.sh
plot/banking-conflict.sh


execute/tpcc-thread.sh
process/tpcc-thread.sh
plot/tpcc-thread.sh


execute/tpcc-conflict.sh
process/tpcc-conflict.sh
plot/tpcc-conflict.sh
