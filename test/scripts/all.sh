#!/bin/bash

######################  CONFIGURATION ##############################################
export NUMITERS=5   #Number of times an experiment is run to aggregate results
export GENDATA=0    #Generate fresh data for experiments 8 10 11
####################################################################################

CAV_DIR=`readlink -m ../../../ThirdParty/Cavalia`
ST_DIR=`readlink -m ../../../MV3C_SingleThreaded`
ST_SCRIPTS_DIR=`readlink -m $ST_DIR/test/cpp/scripts`
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

source $ST_DIR/bench/prep/hyper-threading.sh 0


MAP_H=`readlink -m ../../src/mapping.h`
echo""
echo "Preparation complete..."

if [ -f MAP_H ]; then
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

  execute/6a.sh
  process/6a.sh
  plot/6a.sh

  execute/6b.sh
  process/6b.sh
  plot/6b.sh


  execute/7a.sh
  process/7a.sh
  plot/7a.sh

  execute/7b.sh
 process/7b.sh
  plot/7b.sh

  $ST_SCRIPTS_DIR/7c.sh
  mv 7c*.csv output/raw/
  process/7c.sh
  plot/7c.sh


execute/8a.sh
$CAV_DIR/scripts/grid-tpcc-a.sh
mv $CAV_DIR/../CavaliaData/tpcc-results-cavalia*.csv output/raw/
process/8a.sh
plot/8a.sh


execute/8b.sh
$CAV_DIR/scripts/grid-tpcc-b.sh
mv $CAV_DIR/../CavaliaData/tpcc-results-cavalia*.csv output/raw/
process/8b.sh
plot/8b.sh

 $ST_SCRIPTS_DIR/10.sh
 mv 10*.csv output/raw/
 process/10.sh
 plot/10.sh

 $ST_SCRIPTS_DIR/11.sh
 mv 11*.csv output/raw/
 process/11.sh
 plot/11.sh
