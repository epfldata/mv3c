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
import ThreadTest4._
import ddbt.lib.util.ThreadInfo
import ddbt.lib.mvconcurrent._
import TransactionManager._

import java.util.concurrent.atomic.AtomicReference

/**
 * This class tests the maximum performance of multi-threaded app
 * (i.e. the maximum number of calls to an empty function),
 * and tries to set the value in an AtomicReference object in
 * a shared array
 *
 * The results for a machine with 1 CPU with 12 cores
 *   For 1  thread : 91,022,488 xact/sec
 *   For 2  threads: 50,766,466 xact/sec
 *   For 4  threads: 37,515,555 xact/sec
 *   For 8  threads: 32,468,476 xact/sec
 *   For 12 threads: 31,118,825 xact/sec
 *   For 16 threads: 36,673,251 xact/sec
 *   For 24 threads: 37,089,124 xact/sec
 */
object ThreadTest4 {
	val rampupTime = 5
	val measureTime = 10

	val stateChecker = new StateChecker

	class SampleWorker(tm:TransactionManager, id:Int, val activeXacts:Array[AtomicReference[Object]], stateChecker:StateChecker) extends Thread {
		var counter = 0L
		val threadInfo = new ThreadInfo(id)
		// var counter2 = 0
		override def run() {
			// println("started " + id)
			while(stateChecker.isActive) {
				// println("stateChecker.isActive = " + stateChecker.isActive)
				activeXacts(threadInfo.tid).set(new Object())
				// atomic.set(new Object())
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

		val activeXacts = new Array[AtomicReference[Object]](numConn)

		{
			var i = 0
			while(i < activeXacts.length) {
				activeXacts(i) = new AtomicReference[Object]()
				i += 1
			}
		}

		val tm = new TransactionManager(numConn, false)

		val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("xact-thread"))

		val workers = new Array[SampleWorker](numConn)

		for (i <- 0 until numConn) {
			val worker = new SampleWorker(tm, i, activeXacts, stateChecker)
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
			println("worker(%d) = %,d".format(i, workers(i).counter))
		}
		// println("countAll = %,d".format(countAll))
		println("actualTestTime = %d".format(actualTestTime))
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
}