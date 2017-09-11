#!/bin/bash

[ -z "$1" ] && echo "$0 1|0" && exit 1
declare -a ENABLEDCORES
CTR=0
NUMCORES=0
PHYSICALID_FIRST="$(cat /sys/devices/system/cpu/cpu0/topology/physical_package_id)";
for CPUFILE in /sys/devices/system/cpu/cpu[0-9]*; do
    CPU="/sys/devices/system/cpu/cpu$CTR"
    CPUID=$(basename $CPU)
    echo -n "CPU: $CPUID";
    if test -e $CPU/online; then
            echo "1" > $CPU/online;
            if [ "$1" = "1" ]; then
                echo "${CPU} core=${COREID} -> enable"
                NUMCORES=$((NUMCORES+1))
            fi;
    fi;

    if [ "$1" = "0" ]; then
        COREID="$(cat $CPU/topology/core_id)";
        PHYSICALID="$(cat $CPU/topology/physical_package_id)";
        eval "COREENABLE=\"\${core${PHYSICALID}AND${COREID}enable}\"";
        if  ${COREENABLE:-true} && [ $PHYSICALID = $PHYSICALID_FIRST ]; then
                echo " physical=${PHYSICALID} core=${COREID} -> enable"
                eval "core${PHYSICALID}AND${COREID}enable='false'";
                cpufreq-set -g performance -c $CTR 
                NUMCORES=$((NUMCORES+1))
                ENABLEDCORES+=($CTR)
        else
                echo " physical=${PHYSICALID} core=${COREID} -> disable";
                echo "0" > "$CPU/online";
        fi;
    fi;
    CTR=$((CTR+1))
done;

echo "Total number of enabled cores = $NUMCORES"

FLAG1=1
FLAG2=1
FLAG4=1

for c in `seq $NUMCORES`
do
    i=$((c-1))
    FLAG1=$((FLAG1 && ENABLEDCORES[i]==i))
    FLAG2=$((FLAG2 && ENABLEDCORES[i]==2*i))
    FLAG4=$((FLAG4 && ENABLEDCORES[i]==4*i))
done

export FLAG1
export FLAG2
export FLAG4
export NUMCORES