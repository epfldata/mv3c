#!/bin/bash

#logging in as root
ssh root@iccluster101.iccluster.epfl.ch

#create user
adduser dashti sudo
adduser sachin sudo
mkdir -p /home/dashti/.ssh
mkdir -p /home/sachin/.ssh
exit


cat .ssh/id_rsa.pub | ssh IC 'cat >> .ssh/authorized_keys'


#logging in as root
ssh IC

#to check the OS (ubuntu) version
lsb_release -a 

#update apt and install necessary packages
sudo apt-get update
sudo apt-get install -y vim  htop byobu build-essential mysql-server python-pip libcppunit-dev libcrypto++-dev openssh-server rsync gdb cpufrequtils ant libxml2-dev libxslt-dev python-dev lib32z1-dev
sudo pip install lxml
#install java
sudo apt-get  install -y software-properties-common #needed for add-apt-repository
sudo apt-add-repository ppa:webupd8team/java
sudo apt-get update
sudo apt-get  install -y oracle-java8-installer
java -version
sudo apt-get install -y oracle-java8-set-default
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
sudo apt-get install -y sbt


#add your public key to github
git clone https://github.com/epfldata/mv3c.git



