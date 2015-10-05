//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.nio.charset.Charset
import java.text.DecimalFormat
import java.util.Properties
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import ddbt.tpcc.loadtest.Driver
import java.util.concurrent._
import ddbt.tpcc.loadtest.{RtHist, Util, NamedThreadFactory}
import ThreadTest3._
import ddbt.lib.util.ThreadInfo
import ddbt.lib.mvconcurrent._
import TransactionManager._

/**
 * This class tests the maximum performance of multi-threaded app
 * (i.e. the maximum number of calls to an empty function),
 * by signaling the exec thread via @volatile booleans inside
 * the exec thread
 *
 * The results for a machine with 1 CPU with 12 cores
 *   For the case of a only begin operation + setting the current (null) xact (via activeXacts array):
 *     1  thread :   126,462,980 xact/sec
 *     2  threads:    66,375,109 xact/sec
 *     4  threads:    80,213,871 xact/sec
 *     8  threads:   134,023,810 xact/sec
 *     12 threads:   210,861,234 xact/sec
 *     16 threads:   271,575,770 xact/sec
 *     24 threads:   418,764,375 xact/sec
 *   For the case of a only begin operation + setting the current (null) xact (via ThreadInfo):
 *     1  thread :   126,340,389 xact/sec
 *     2  threads:   252,283,362 xact/sec
 *     4  threads:   197,042,930 xact/sec
 *     8  threads:   244,165,767 xact/sec
 *     12 threads:   208,543,010 xact/sec
 *     16 threads:   296,191,759 xact/sec
 *     24 threads:   477,033,163 xact/sec
 *   For the case of a only begin operation + creating an empty transaction object:
 *     1  thread :   541,810,148 xact/sec
 *     2  threads: 1,084,959,339 xact/sec
 *     4  threads: 1,978,495,881 xact/sec
 *     8  threads: 3,819,698,198 xact/sec
 *     12 threads: 5,726,755,825 xact/sec
 *     16 threads: 5,725,844,688 xact/sec
 *     24 threads: 5,725,458,128 xact/sec
 *   For the case of a only begin operation + setting the current (non-null, but empty) xact (via activeXacts array):
 *     1  thread :    52,460,615 xact/sec
 *     2  threads:    30,189,358 xact/sec
 *     4  threads:    38,595,221 xact/sec
 *     8  threads:    46,321,790 xact/sec
 *     12 threads:    39,862,235 xact/sec
 *     16 threads:    44,808,081 xact/sec
 *     24 threads:    36,189,501 xact/sec
 *   For the case of a only begin operation + setting the current (non-null, but empty) xact (via ThreadInfo):
 *     1  thread :    55,490,355 xact/sec
 *     2  threads:    78,975,037 xact/sec
 *     4  threads:    40,072,635 xact/sec
 *     8  threads:    43,386,622 xact/sec
 *     12 threads:   117,169,999 xact/sec
 *     16 threads:   254,725,196 xact/sec
 *     24 threads:   130,685,287 xact/sec
 *   For the case of a only begin operation + getAndIncrement on startAndCommitTimestampGen as AtomicLong (without timestampLock) + creating an empty transaction object (with a correct start TS):
 *     1  thread :   126,407,353 xact/sec
 *     2  threads:    55,598,296 xact/sec
 *     4  threads:    48,759,885 xact/sec
 *     8  threads:    49,195,878 xact/sec
 *     12 threads:    49,229,183 xact/sec
 *     16 threads:    49,142,449 xact/sec
 *     24 threads:    49,838,579 xact/sec
 *   For the case of a only begin operation + getAndIncrement on startAndCommitTimestampGen as BackoffAtomicLong (without timestampLock) + creating an empty transaction object (with a correct start TS):
 *     1  thread :   101,965,179 xact/sec
 *     2  threads:    45,258,845 xact/sec
 *     4  threads:    35,649,251 xact/sec
 *     8  threads:    33,416,584 xact/sec
 *     12 threads:    32,311,750 xact/sec
 *     16 threads:    30,348,829 xact/sec
 *     24 threads:    28,649,091 xact/sec
 *   For the case of a only begin operation + getAndIncrement on startAndCommitTimestampGen as AtomicLong (with timestampLock) + creating an empty transaction object (with a correct start TS):
 *     1  thread :    46,942,499 xact/sec
 *     2  threads:     8,578,467 xact/sec
 *     4  threads:     7,372,538 xact/sec
 *     8  threads:     7,233,321 xact/sec
 *     12 threads:     8,708,049 xact/sec
 *     16 threads:    10,667,376 xact/sec
 *     24 threads:    10,175,014 xact/sec
 *   For the case of a only begin operation + getAndIncrement on startAndCommitTimestampGen as volatile variable (with timestampLock) + creating an empty transaction object (with a correct start TS):
 *     1  thread :    43,824,397 xact/sec
 *     2  threads:     8,199,790 xact/sec
 *     4  threads:     7,107,507 xact/sec
 *     8  threads:     7,332,674 xact/sec
 *     12 threads:    10,235,128 xact/sec
 *     16 threads:     9,535,717 xact/sec
 *     24 threads:    10,416,183 xact/sec
 *   For the case of a only begin operation + getAndIncrement on transactionIdGen + getAndIncrement on startAndCommitTimestampGen as AtomicLong (with timestampLock) + creating an empty transaction object (with a correct start TS):
 *     1  thread :    37,294,120 xact/sec
 *     2  threads:     7,110,754 xact/sec
 *     4  threads:     6,823,667 xact/sec
 *     8  threads:     7,002,666 xact/sec
 *     12 threads:     9,467,667 xact/sec
 *     16 threads:     9,699,644 xact/sec
 *     24 threads:     8,897,522 xact/sec
 *   For the case of a full begin operation (without read-only hint):
 *     1  thread :     17,698,560 xact/sec
 *     2  threads:      4,997,113 xact/sec
 *     4  threads:      4,601,611 xact/sec
 *     8  threads:      4,838,480 xact/sec
 *     12 threads:      6,161,817 xact/sec
 *     16 threads:      5,918,039 xact/sec
 *     24 threads:      5,712,544 xact/sec
 *   For the case of a full begin operation (with read-only hint):
 *     1  thread :     18,550,545 xact/sec
 *     2  threads:      6,306,725 xact/sec
 *     4  threads:      5,825,917 xact/sec
 *     8  threads:      5,122,738 xact/sec
 *     12 threads:      7,218,744 xact/sec
 *     16 threads:      6,560,245 xact/sec
 *     24 threads:      6,341,557 xact/sec
 *   For the case of a full begin and commit operations:
 *     1  thread :     4,122,311 xact/sec
 *     2  threads:     6,329,151 xact/sec
 *     4  threads:     1,247,336 xact/sec
 *     8  threads:     1,105,749 xact/sec
 *     12 threads:     1,157,761 xact/sec
 *     16 threads:     1,139,353 xact/sec
 *     24 threads:     1,428,866 xact/sec
 */
object ThreadTest3 {
	val rampupTime = 10
	val measureTime = 10

	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val stateChecker = new StateChecker

	class SampleWorker(tm:TransactionManager, id:Int, numConn:Int, stateChecker:StateChecker) extends Thread {
		var counter = 0L
		val tInfo = new ThreadInfo(id)
		// var counter2 = 0
		override def run() {
			// println("started " + id)
			while(stateChecker.isActive) {
				// println("stateChecker.isActive = " + stateChecker.isActive)
				implicit val xact = tm.begin("Xact1", true)(tInfo)
				try {
					// val fromAccNum = tInfo.tid
					// val toAccNum = tInfo.tid+numConn
					// val amount = 2

					// val fromAcc = accountsTbl.getEntry(fromAccNum)
					// fromAcc.setEntryValue(Tuple1(fromAcc.row._1 - amount))

					// val toAcc = accountsTbl.getEntry(toAccNum)
					// toAcc.setEntryValue(Tuple1(toAcc.row._1 + amount))
					// tm.commit
				} catch {
					case e: Throwable => tm.rollback
				}
				if(stateChecker.counting_on) {
					// println("stateChecker.counting_on = " + stateChecker.counting_on)
					counter += 1L
				}
			}

			// println("finished " + id)
		}
	}

	class StateChecker {
		@volatile var counting_on = false
		@volatile var isActive = true
	}

	def main(argv:Array[String]) {
		if(argv.length == 0) throw new RuntimeException("Please specify the number of connections")
		val numConn =  Integer.parseInt(argv(0))

		val tm = new TransactionManager(numConn, false)

		val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("xact-thread"))

		val workers = new Array[SampleWorker](numConn)

		init(tm, numConn)

		for (i <- 0 until numConn) {
			val worker = new SampleWorker(tm, i, numConn, stateChecker)
			workers(i) = worker
			executor.execute(worker)
		}
		// println("started")
		// println("ramp up")
		Thread.sleep(rampupTime * 1000)
		stateChecker.counting_on = true
		// println("counting")
		val startTime = System.currentTimeMillis()
		try {
			Thread.sleep(measureTime * 1000)
		} catch {
			case e: InterruptedException => new RuntimeException("Sleep interrupted", e)
		}
		stateChecker.counting_on = false
		stateChecker.isActive = false
		val actualTestTime = System.currentTimeMillis() - startTime
		// println("finished")
		var countAll = 0L
		for (i <- 0 until numConn) {
			countAll += workers(i).counter
			// println("worker(%d) = %,d".format(i, workers(i).counter))
		}
		// println("countAll = %,d".format(countAll))
		// println("actualTestTime = %d".format(actualTestTime))
		println("run/sec = %,.0f".format((countAll * 1.0) / (actualTestTime / 1000.0) ))
		executor.shutdown()
		// println("trying to shut down")
		try {
			executor.awaitTermination(30, TimeUnit.SECONDS)
		} catch {
			case e: InterruptedException => println("Timed out waiting for executor to terminate")
		}
		// println("shut down")
	}

	def init(tm:TransactionManager, numConn:Int) = {
		// implicit val xact = tm.begin("InitXact")(new ThreadInfo(0))
		// for(i <- 0 until (numConn*2)) accountsTbl += (i, Tuple1(i*3L))
		// xact.commit
		this
	}
}