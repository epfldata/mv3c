#!/bin/bash

[ -z "$1" ] && echo "$0 1|0" && exit 1

for CPU in /sys/devices/system/cpu/cpu[0-9]*; do
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
        eval "COREENABLE=\"\${core${COREID}enable}\"";
        if ${COREENABLE:-true}; then        
                echo "${CPU} core=${COREID} -> enable"
                eval "core${COREID}enable='false'";
        else
                echo "$CPU core=${COREID} -> disable"; 
                echo "0" > "$CPU/online"; 
        fi;
    fi;
done;
