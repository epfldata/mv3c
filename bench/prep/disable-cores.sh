#!/bin/bash
for i in `seq 1 47`;
do
        eval "echo 1 | sudo tee /sys/devices/system/cpu/cpu$i/online"
done

for i in `seq 12 47`;
do
        eval "echo 0 | sudo tee /sys/devices/system/cpu/cpu$i/online"
done

for i in `seq 0 47`;
do
        echo "CPU $i: "
        eval "sudo cat /sys/devices/system/cpu/cpu$i/topology/core_id"
done