#!/bin/bash -x

export LANG=en_US

[ -z "$1" ] && echo "Enter the argument with the policy (e.g., performance)!!!" && exit 1
 
cpufreq-info | grep analyzing | cut -f3 -d' ' | cut -f1 -d: | sudo xargs -n1 cpufreq-set -g $1 -c 


cpufreq-info | egrep 'analyzing|current CPU'
