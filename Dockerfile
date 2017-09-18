#If runnning in privileged mode, apparmor profile for mysql in the HOST machine should be disabled

FROM ubuntu:16.04

WORKDIR /mv3c

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    apt-utils \
 && rm -rf /var/lib/apt/lists/*

 RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    software-properties-common \
    curl \
 && rm -rf /var/lib/apt/lists/* \
 && echo "\nexport TERM=xterm" >> /etc/bash.bashrc

RUN  apt-add-repository -y ppa:webupd8team/java

RUN  echo "mysql-server-5.7 mysql-server/root_password password ROOT" |  debconf-set-selections  && \
     echo "mysql-server-5.7 mysql-server/root_password_again password ROOT" |  debconf-set-selections && \
     echo debconf shared/accepted-oracle-license-v1-1 select true |  debconf-set-selections && \
     echo debconf shared/accepted-oracle-license-v1-1 seen true |  debconf-set-selections && \
     echo "ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true" | debconf-set-selections


RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \ 
unzip \ 
apt-transport-https \
build-essential \
automake \
autoconf \
libtool \
python \
python-pip \
libcrypto++-dev \
cpufrequtils \
ant \
libxml2-dev \
libxslt-dev \
python-dev \
lib32z1-dev \
libboost-all-dev \
libboost-thread-dev \
libjemalloc-dev \
libpapi-dev \
liblz4-dev \
libzmq5-dev \
mysql-server-5.7 \
mysql-client \
oracle-java8-installer \
gnuplot \
ttf-mscorefonts-installer \
libcppunit-dev \
vim \
 && rm -rf /var/lib/apt/lists/*

RUN echo secure_file_priv=\"\" |  tee --append /etc/mysql/mysql.conf.d/mysqld.cnf && \
    echo "skip-grant-tables"   |  tee --append /etc/mysql/mysql.conf.d/mysqld.cnf && \ 
    service mysql start && \
    mysql -u root -pROOT -e "create database tpcctest"

RUN  wget www.scala-lang.org/files/archive/scala-2.11.7.deb && \
     dpkg -i scala-2.11.7.deb && \
     rm scala-2.11.7.deb

RUN wget https://dl.bintray.com/sbt/debian/sbt-0.13.15.deb && \
	dpkg -i sbt-0.13.15.deb && \
	rm sbt-0.13.15.deb

RUN wget https://github.com/efficient/libcuckoo/archive/v0.1.zip && \
	unzip v0.1.zip && \
	cd libcuckoo-0.1 && \
	autoreconf -fis && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	cd .. && \
	rm v0.1.zip &&  \
	rm -rf libcuckoo-0.1

RUN pip install lxml


COPY . /mv3c
RUN cp /mv3c/conf/tpcc.properties.example /mv3c/conf/tpcc.properties

WORKDIR /mv3c/test/scripts
ENTRYPOINT service mysql start && /bin/bash
