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
import ThreadTest1._

object ThreadTest1 {
	val numConn = 8
	val rampupTime = 5
	val measureTime = 10

	val stateChecker = new StateChecker

	class SampleWorker(id:Int, stateChecker:StateChecker) extends Thread {
		var counter = 0L
		// var counter2 = 0
		override def run() {
			println("started " + id)
			while(stateChecker.isActive) {
				// println("stateChecker.isActive = " + stateChecker.isActive)
				if(stateChecker.counting_on) {
					// println("stateChecker.counting_on = " + stateChecker.counting_on)
					counter += 1L
				}
			}

			println("finished " + id)
		}
	}

	class StateChecker {
		@volatile var counting_on = false
		@volatile var isActive = true
	}

	def main(argv:Array[String]) {

		val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("xact-thread"))

		val workers = new Array[SampleWorker](numConn)

		for (i <- 0 until numConn) {
			val worker = new SampleWorker(i, stateChecker)
			workers(i) = worker
			executor.execute(worker)
		}
		println("started")
		println("ramp up")
		Thread.sleep(rampupTime * 1000)
		stateChecker.counting_on = true
		println("counting")
		val startTime = System.currentTimeMillis()
		var runTime = 0L
		while ({(runTime = System.currentTimeMillis() - startTime); runTime} < measureTime * 1000) {
			//println("Current execution time lapse: " + df.format(runTime / 1000f) + " seconds")
			try {
				Thread.sleep(1000)
			} catch {
				case e: InterruptedException => new RuntimeException("Sleep interrupted", e)
			}
		}
		stateChecker.counting_on = false
		stateChecker.isActive = false
		val actualTestTime = System.currentTimeMillis() - startTime
		println("finished")
		var countAll = 0L
		for (i <- 0 until numConn) {
			countAll += workers(i).counter
			println("worker(%d) = %,d".format(i, workers(i).counter))
		}
		println("countAll = %,d".format(countAll))
		println("actualTestTime = %d".format(actualTestTime))
		println("run/sec = %,.0f".format((countAll * 1.0) / (actualTestTime / 1000.0) ))
		executor.shutdown()
		println("trying to shut down")
		try {
			executor.awaitTermination(30, TimeUnit.SECONDS)
		} catch {
			case e: InterruptedException => println("Timed out waiting for executor to terminate")
		}
		println("shut down")
	}
}