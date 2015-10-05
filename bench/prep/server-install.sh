#!/bin/bash

#logging in as root
ssh root@iccluster026.iccluster.epfl.ch

#create user
sudo useradd -d /home/dashti -m dashti
usermod -s /bin/bash dashti
passwd dashti

#add root priviledges to it
/usr/sbin/visudo
#dashti  ALL=(ALL:ALL) ALL

exit


#on your local machine
#create alias to the server in your client
alias ic='ssh dashti@iccluster026.iccluster.epfl.ch'
ic mkdir -p .ssh
cat .ssh/id_rsa.pub | ic 'cat >> .ssh/authorized_keys'


#logging in as root
ic

#to check the OS (ubuntu) version
lsb_release -a 

#to check whether hyperthreading is enabled
sudo dmidecode
# and then check for
# 'Status: Populated, Enabled' -> hyper-threading enabled
# 'Status: Unpopulated' -> hyper-threading disabled
# also you can check for:
#   Core Count: 12
#   Core Enabled: 12
#   Thread Count: 24
# which here means hyper-threading is enabled
cat /proc/cpuinfo | grep "cpu cores" | uniq -c | xargs | cut -d\  -f1,5| awk '{print "hyper-threading",($1!=$2)?"on":"off"}'

#to disable hyper-threading
sudo nano /etc/rc.local
# place this near the end before the "exit 0"
#   for CPU in /sys/devices/system/cpu/cpu[0-9]*; do
#       CPUID=$(basename $CPU)
#       echo "CPU: $CPUID";
#       if test -e $CPU/online; then
#               echo "1" > $CPU/online;
#       fi;
#   
#       COREID="$(cat $CPU/topology/core_id)";
#       eval "COREENABLE=\"\${core${COREID}enable}\"";
#       if ${COREENABLE:-true}; then        
#               echo "${CPU} core=${COREID} -> enable"
#               eval "core${COREID}enable='false'";
#       else
#               echo "$CPU core=${COREID} -> disable"; 
#               echo "0" > "$CPU/online"; 
#       fi;
#   done;

#or another approach for completely disabling one CPU is:
sudo nano /etc/rc.local
# place this near the end before the "exit 0"
#   for i in `seq 1 47`;
#   do
#       eval "echo 1 | sudo tee /sys/devices/system/cpu/cpu$i/online"
#   done
#   for i in `seq 12 47`;
#   do
#       eval "echo 0 | sudo tee /sys/devices/system/cpu/cpu$i/online"
#   done
#   for i in `seq 0 47`;
#   do
#       echo "CPU $i: "
#       eval "sudo cat /sys/devices/system/cpu/cpu$i/topology/core_id"
#   done

#to check cpu info
lscpu
cat /proc/cpuinfo
cat /proc/cpuinfo | grep "MHz" #for cpu frequency

#update apt and install necessary packages
sudo apt-get update
sudo apt-get install vim
sudo apt-get install htop
sudo apt-get install screen
sudo apt-get install g++
#install java
sudo apt-get install software-properties-common #needed for add-apt-repository
sudo apt-add-repository ppa:webupd8team/java
sudo apt-get update
sudo apt-get install oracle-java8-installer
java -version
sudo apt-get install oracle-java8-set-default
#install scala
sudo apt-get remove scala-library scala
sudo wget www.scala-lang.org/files/archive/scala-2.11.7.deb
sudo dpkg -i scala-2.11.7.deb
sudo apt-get update
sudo apt-get install scala
scala -version
#install sbt
echo "deb https://dl.bintray.com/sbt/debian /" | sudo tee -a /etc/apt/sources.list.d/sbt.list
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 642AC823
sudo apt-get update
sudo apt-get install sbt

#add useful commands
printf '\nalias "ll=ls -a -l"\n' >> ~/.bashrc
# printf 'export PS1="$(whoami)@$(hostname):$(pwd)$ "\n' >> ~/.bashrc
# printf 'export PS1="\u@\h:\w$ "\n' >> ~/.bashrc
# printf 'export JAVA_HOME=/usr/lib/jvm/java-8-oracle\n' >> ~/.bashrc

#generating ssh key for github
ssh-keygen -t rsa
cat ~/.ssh/id_rsa.pub
#add your public key to github
git config --global user.email "mdashti@gmail.com"
git config --global user.name "Mohammad Dashti"
git clone git@github.com:mdashti/TStore.git
mv TStore SStore_MVC3T
cd SStore_MVC3T
sbt compile

#in order to sync your local folder
alias icsync='rsync --exclude test-output/ --exclude .idea/ --exclude .git/ --exclude project/project/ --exclude project/target/ --exclude target/ -aziP /Users/dashti/Dropbox/workspaces/SStore_MVC3T/ dashti@iccluster026.iccluster.epfl.ch:/home/dashti/SStore_MVC3T'
#to run the test.sh from client
alias ictest='FILEID=1 ; if ssh dashti@iccluster026.iccluster.epfl.ch stat /home/dashti/SStore_MVC3T/screenlog.0 \> /dev/null 2\>\&1 ; then while true; do if ssh dashti@iccluster026.iccluster.epfl.ch stat /home/dashti/SStore_MVC3T/test-output/result_$FILEID.txt \> /dev/null 2\>\&1 ; then FILEID=$((FILEID+1)) ; else break ; fi ; done ; ssh dashti@iccluster026.iccluster.epfl.ch mv /home/dashti/SStore_MVC3T/screenlog.0 /home/dashti/SStore_MVC3T/test-output/result_$FILEID.txt ; else echo "No previous screen output available" ; fi ; ic "cd /home/dashti/SStore_MVC3T && screen -d -m -L /home/dashti/SStore_MVC3T/test.sh"'
#get the results of a screen command
alias icres='FILEID=1 ; if ssh dashti@iccluster026.iccluster.epfl.ch stat /home/dashti/SStore_MVC3T/screenlog.0 \> /dev/null 2\>\&1 ; then while true; do if [ ! -f /Users/dashti/Dropbox/workspaces/SStore_MVC3T/test-output/result_$FILEID.txt ] ; then eval "rsync -azP dashti@iccluster026.iccluster.epfl.ch:/home/dashti/SStore_MVC3T/screenlog.0 /Users/dashti/Dropbox/workspaces/SStore_MVC3T/test-output/result_$FILEID.txt && break" ; else FILEID=$((FILEID+1)) ; fi ; done ; else echo "No screen output available" ; fi'

