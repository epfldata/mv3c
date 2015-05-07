#!/bin/bash

export JAVA_HOME=/home/vjovanov/bin/jdk1.7.0_51

WARES="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 24 32" # "2 4 8 16"
impl="-1"
MaxPermSize="30g"
Xmx="30g"
Xms="30g"
Xss="1g"


CP="$HOME/.sbt/boot/scala-2.10.2/lib/scala-library.jar:target/scala-2.10/classes:target/scala-2.10/test-classes:lib/dbtlib.jar:lib/log4j-api-2.0-beta9.jar:lib/log4j-core-2.0-beta9.jar:lib/log4j-slf4j-impl-2.0-beta9.jar:lib/mysql-connector-java-5.1.26-bin.jar:lib/slf4j-api-1.7.2.jar:lib/slf4j-ext-1.7.2.jar"

cd "$(dirname "$0")"
cd ..

sbt compile
sbt test:compile

ten=10
for c in $WARES; do
	echo "Loading database for i = $c"

	./bench2/bench load $c;

	echo "Running for i = $c"
#	if [ "$c" -lt "$ten" ]; then
                echo "    4 sec execution..."
		$JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 4 -i $impl > ./bench2/sstore_"$impl"_"$c"_4exec.txt
                echo "    10 sec execution..."
                $JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 10 -i $impl > ./bench2/sstore_"$impl"_"$c"_10exec.txt
                echo "    30 sec execution..."
                $JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 30 -i $impl > ./bench2/sstore_"$impl"_"$c"_30exec.txt
                echo "    60 sec execution..."
                $JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 60 -i $impl > ./bench2/sstore_"$impl"_"$c"_60exec.txt
                echo "    120 sec execution..."
                $JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 120 -i $impl > ./bench2/sstore_"$impl"_"$c"_120exec.txt
                echo "    180 sec execution..."
                $JAVA_HOME/bin/java -server -Xss$Xss -Xmx$Xmx -Xms$Xms -XX:MaxPermSize=$MaxPermSize -XX:-DontCompileHugeMethods -classpath $CP ddbt.tpcc.tx.TpccInMem -w $c -t 180 -i $impl > ./bench2/sstore_"$impl"_"$c"_180exec.txt
        ./bench2/tpcc_process.sh
done
