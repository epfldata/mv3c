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
import ThreadTest0._
/**
 * This class tests the maximum performance of multi-threaded app
 * (i.e. the maximum number of calls to an empty function),
 * by checking the test time inside the exec thread
 *
 * The results for a machine with 1 CPU with 12 cores
 *   For 1  thread :  46,974,000 xact/sec
 *   For 2  threads:  96,550,321 xact/sec
 *   For 4  threads: 175,439,303 xact/sec
 *   For 8  threads: 339,674,272 xact/sec
 *   For 12 threads: 495,592,806 xact/sec
 *   For 16 threads: 495,515,713 xact/sec
 *   For 24 threads: 495,676,649 xact/sec
 */
object ThreadTest0 {
	val rampupTime = 10
	val measureTime = 10

	val stateChecker = new StateChecker

	class SampleWorker(id:Int, stateChecker:StateChecker) extends Thread {
		val rampupStart = System.currentTimeMillis()
		var startTime = rampupStart + (rampupTime * 1000)
		var finishTime = startTime + (measureTime * 1000)
		var counter = 0L
		// var counter2 = 0
		override def run() {
			println("started " + id)
			var now = System.currentTimeMillis()
			while(now < finishTime) {
				// println("stateChecker.isActive = " + stateChecker.isActive)
				if(now > startTime) {
					// println("stateChecker.counting_on = " + stateChecker.counting_on)
					counter += 1L
				}
				now = System.currentTimeMillis()
			}
			finishTime = now

			println("finished " + id)
		}
	}

	class StateChecker {
		@volatile var counting_on = false
		@volatile var isActive = true
	}

	def main(argv:Array[String]) {
		if(argv.length == 0) throw new RuntimeException("Please specify the number of connections")
		val numConn =  Integer.parseInt(argv(0))

		val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("xact-thread"))

		val workers = new Array[SampleWorker](numConn)

		for (i <- 0 until numConn) {
			val worker = new SampleWorker(i, stateChecker)
			workers(i) = worker
			executor.execute(worker)
		}
		
		executor.shutdown()
		try {
			executor.awaitTermination(120, TimeUnit.SECONDS)
		} catch {
			case e: InterruptedException => println("Timed out waiting for executor to terminate")
		}
		var averageAll = 0.0
		for (i <- 0 until numConn) {
			val actualTestTime = workers(i).finishTime - workers(i).startTime
			averageAll += workers(i).counter / (actualTestTime / 1000.0)
			println("worker(%d) = %,d in %.2f sec".format(i, workers(i).counter, actualTestTime / 1000.0))
		}
		println("run/sec = %,.0f".format(averageAll))
		
	}
}