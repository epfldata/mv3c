#!/bin/bash

[ -z "$1" ] && echo "$0 1|0" && exit 1

CTR=0

for CPUFILE in /sys/devices/system/cpu/cpu[0-9]*; do
    CPU="/sys/devices/system/cpu/cpu$CTR"
    CPUID=$(basename $CPU)
    echo "CPU: $CPUID";
    if test -e $CPU/online; then
            echo "1" > $CPU/online;
            if [ "$1" = "1" ]; then
                echo "${CPU} core=${COREID} -> enable"
            fi;
    fi;

    if [ "$1" = "0" ]; then
        COREID="$(cat $CPU/topology/core_id)";
        PHYSICALID="$(cat $CPU/topology/physical_package_id)";
        eval "COREENABLE=\"\${core${PHYSICALID}AND${COREID}enable}\"";
        if ${COREENABLE:-true}; then
                echo "${CPU} physical=${PHYSICALID} core=${COREID} -> enable"
                eval "core${PHYSICALID}AND${COREID}enable='false'";
        else
                echo "$CPU physical=${PHYSICALID} core=${COREID} -> disable";
                echo "0" > "$CPU/online";
        fi;
    fi;
    CTR=$((CTR+1))
done;
