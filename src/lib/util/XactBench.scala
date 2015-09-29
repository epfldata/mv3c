package ddbt.lib.util

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
import java.util.concurrent.ThreadLocalRandom

object XactBench {

	val logger = LoggerFactory.getLogger(classOf[XactBench])

	val DEBUG = logger.isDebugEnabled

	private val PROPERTIESFILE = "./conf/tpcc.properties"

	private val WAREHOUSECOUNT = "WAREHOUSECOUNT"

	private val CONNECTIONS = "CONNECTIONS"

	private val RAMPUPTIME = "RAMPUPTIME"

	private val DURATION = "DURATION"

	private var NUM_ITERATIONS = 1

	@volatile var counting_on: Boolean = false

	@volatile var activate_transaction: Boolean = true

	val RANDOM_SEED = 2015

	def main(argv:Array[String]) {

	}

}

import XactBench._

class XactBench(val xactSelector: XactImplSelector) {

	private val TRANSACTION_NAME = xactSelector.xactNames

	private val TRANSACTION_COUNT = TRANSACTION_NAME.size

	private var implVersionUnderTest = 0

	private var scaleFactor: Int = _

	private var numConn: Int = _

	private var rampupTime: Int = _

	private var measureTime: Int = _

	private var num_node: Int = _

	var result: Seq[Long] = Array[Long]()

	private var port: Int = 3306

	private var properties: Properties = _

	private var inputStream: InputStream = _

	def init() {
		logger.info("Loading properties from: " + PROPERTIESFILE)
		properties = new Properties()
		inputStream = new FileInputStream(PROPERTIESFILE)
		properties.load(inputStream)
	}

	def runBenchmark(overridePropertiesFile: Boolean, argv: Array[String]): Int = {
		println("***************************************")
		println("******   Transaction Bechmark    ******")
		println("***************************************")

		scaleFactor = Integer.parseInt(properties.getProperty(WAREHOUSECOUNT))
		numConn = Integer.parseInt(properties.getProperty(CONNECTIONS))
		rampupTime = Integer.parseInt(properties.getProperty(RAMPUPTIME))
		measureTime = Integer.parseInt(properties.getProperty(DURATION))
		implVersionUnderTest = 0
		if (overridePropertiesFile) {
			var i = 0
			while (i < argv.length) {
				if (argv(i) == "-w") {
					scaleFactor = Integer.parseInt(argv(i + 1))
				} else if (argv(i) == "-c") {
					numConn = Integer.parseInt(argv(i + 1))
				} else if (argv(i) == "-r") {
					rampupTime = Integer.parseInt(argv(i + 1))
				} else if (argv(i) == "-t") {
					measureTime = Integer.parseInt(argv(i + 1))
				} else if (argv(i) == "-i") {
					implVersionUnderTest = Integer.parseInt(argv(i + 1))
				} else {
					println("Incorrect Argument: " + argv(i))
					println("The possible arguments are as follows: ")
					println("-w [number of warehouses]")
					println("-c [number of connections]")
					println("-r [ramp up time]")
					println("-t [duration of the benchmark (sec)]")
					println("-i [implementation version under test]")
					System.exit(-1)
				}
				i = i + 2
			}
		}

		if (num_node > 0) {
			if (scaleFactor % num_node != 0) {
				logger.error(" [warehouse] value must be devided by [num_node].")
				return 1
			}
			if (numConn % num_node != 0) {
				logger.error("[connection] value must be devided by [num_node].")
				return 1
			}
		}
		if (scaleFactor < 1) {
			throw new RuntimeException("Warehouse count has to be greater than or equal to 1.")
		}
		if (numConn < 1) {
			throw new RuntimeException("Connections has to be greater than or equal to 1.")
		}
		if (rampupTime < 1) {
			throw new RuntimeException("Rampup time has to be greater than or equal to 1.")
		}
		if (measureTime < 1) {
			throw new RuntimeException("Duration has to be greater than or equal to 1.")
		}

		var iter = 0
		while(iter < NUM_ITERATIONS) {
			val xactImpl = xactSelector.select(implVersionUnderTest, numConn)

			RtHist.histInit()
			activate_transaction = true
			num_node = 0
			System.out.print("<Parameters>\n")
			System.out.print("  [warehouse]: %d\n".format(scaleFactor))
			System.out.print(" [connection]: %d\n".format(numConn))
			System.out.print("     [rampup]: %d (sec.)\n".format(rampupTime))
			System.out.print("    [measure]: %d (sec.)\n".format(measureTime))
			System.out.print("       [impl]: tx%d (%s)\n".format(implVersionUnderTest, xactImpl.getClass.getSimpleName.replace("$", "")))
			xactSelector.seqInit(10, 10, 1, 1, 1)
			if (DEBUG) logger.debug("Creating XactThread")

			val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("xact-thread"))

			counting_on = false

			val workers = new Array[XactThread](numConn)
			for (i <- 0 until numConn) {
				val worker = new XactThread(new ThreadInfo(i), scaleFactor, numConn, TRANSACTION_COUNT, activate_transaction, xactImpl)
				workers(i) = worker
				executor.execute(worker)
			}
			if (rampupTime > 0) {
				System.out.print("\nRAMPUP START.\n\n")
				try {
					Thread.sleep(rampupTime * 1000)
				} catch {
					case e: InterruptedException => logger.error("Rampup wait interrupted", e)
				}
				System.out.print("\nRAMPUP END.\n\n")
			}
			System.out.print("\nMEASURING START.\n\n")
			val df = new DecimalFormat("#,##0.0")
			var runTime = 0L
			System.gc()
			Thread.sleep(1000)
			System.gc()
			println("Start measurements....")
			Thread.sleep(1000)
			counting_on = true
			xactImpl.isCountingOn = true
			val startTime = System.currentTimeMillis()
			Thread.sleep(measureTime * 1000)
			// while ({(runTime = System.currentTimeMillis() - startTime); runTime} < measureTime * 1000) {
			// 	//println("Current execution time lapse: " + df.format(runTime / 1000f) + " seconds")
			// 	try {
			// 		Thread.sleep(1000)
			// 	} catch {
			// 		case e: InterruptedException => logger.error("Sleep interrupted", e)
			// 	}
			// }
			activate_transaction = false
			xactImpl.isCountingOn = false
			counting_on = false
			val actualTestTime = System.currentTimeMillis() - startTime
			println("---------------------------------------------------")
			println("<Raw Results>")

			val success = new Array[Long](TRANSACTION_COUNT)
			val late = new Array[Long](TRANSACTION_COUNT)
			val retry = new Array[Long](TRANSACTION_COUNT)
			val failure = new Array[Long](TRANSACTION_COUNT)
			val success2 = Array.ofDim[Long](TRANSACTION_COUNT, numConn)
			val late2 = Array.ofDim[Long](TRANSACTION_COUNT, numConn)
			val retry2 = Array.ofDim[Long](TRANSACTION_COUNT, numConn)
			val failure2 = Array.ofDim[Long](TRANSACTION_COUNT, numConn)
			var success2_sum: Array[Long] = new Array[Long](TRANSACTION_COUNT)
			var late2_sum: Array[Long] = new Array[Long](TRANSACTION_COUNT)
			var retry2_sum: Array[Long] = new Array[Long](TRANSACTION_COUNT)
			var failure2_sum: Array[Long] = new Array[Long](TRANSACTION_COUNT)

			java.util.Arrays.fill(success, 0)
			java.util.Arrays.fill(late, 0)
			java.util.Arrays.fill(retry, 0)
			java.util.Arrays.fill(failure, 0)
			java.util.Arrays.fill(success2_sum, 0)
			java.util.Arrays.fill(late2_sum, 0)
			java.util.Arrays.fill(retry2_sum, 0)
			java.util.Arrays.fill(failure2_sum, 0)
			for (i <- 0 until TRANSACTION_COUNT) {
				success(i) = 0
				late(i) = 0
				retry(i) = 0
				failure(i) = 0
			}

			var totalCounter = 0L
			for (i <- 0 until numConn) {
				val driver = workers(i).driver
				System.out.print("worker(%d).counter = %,d\n".format(i+1, driver.counter.value))
				totalCounter += driver.counter.value
				for (j <- 0 until TRANSACTION_COUNT) {
					System.out.print("	|%s| sc:%,d	lt:%,d	rt:%,d	fl:%,d <= worker(%,d)\n".format(TRANSACTION_NAME(j), driver.success(j), driver.late(j), 
						driver.retry(j), driver.failure(j), i+1))
					success(j) += driver.success(j)
					late(j) += driver.late(j)
					retry(j) += driver.retry(j)
					failure(j) += driver.failure(j)
					success2(j)(i) += driver.success(j)
					late2(j)(i) += driver.late(j)
					retry2(j)(i) += driver.retry(j)
					failure2(j)(i) += driver.failure(j)
				}
			}
			System.out.print("total counter = %,d\n".format(totalCounter))
			for (i <- 0 until TRANSACTION_COUNT) {
				System.out.print("	|%s| sc:%,d	lt:%,d	rt:%,d	fl:%,d \n".format(TRANSACTION_NAME(i), success(i), late(i), 
					retry(i), failure(i)))
			}
			System.out.print(" in %f sec.\n".format(actualTestTime / 1000f))
			println("<Raw Results2(sum ver.)>")
			for (i <- 0 until TRANSACTION_COUNT) {
				success2_sum(i) = 0
				late2_sum(i) = 0
				retry2_sum(i) = 0
				failure2_sum(i) = 0
				for (k <- 0 until numConn) {
					success2_sum(i) += success2(i)(k)
					late2_sum(i) += late2(i)(k)
					retry2_sum(i) += retry2(i)(k)
					failure2_sum(i) += failure2(i)(k)
				}
			}
			for (i <- 0 until TRANSACTION_COUNT) {
				System.out.print("	|%s| sc:%,d	lt:%,d	rt:%,d	fl:%,d \n".format(TRANSACTION_NAME(i), success2_sum(i), 
					late2_sum(i), retry2_sum(i), failure2_sum(i)))
			}
			println("<Constraint Check> (all must be [OK])\n [transaction percentage]")
			var j = 0L
			var i: Int = 0
			i = 0
			while (i < TRANSACTION_COUNT) {
				j += (success(i) + late(i))
				i += 1
			}
			i = 0
			while (i < TRANSACTION_COUNT) {
				var f = 100f * (success(i) + late(i)).toFloat / j.toFloat
				System.out.print("				%s: %f%%\n".format(TRANSACTION_NAME(i), f))
				i += 1
			}
			System.out.print(" [response time (at least 90%% passed)]\n")
			for (n <- 0 until TRANSACTION_NAME.length) {
				val f = 100f * success(n).toFloat / (success(n) + late(n)).toFloat
				if (DEBUG) logger.debug("f: " + f + " success[" + n + "]: " + success(n) + " late[" + 
					n + 
					"]: " + 
					late(n))
				System.out.print("			%s: %f%% ".format(TRANSACTION_NAME(n), f))
				if (f >= 90f) {
					System.out.print(" [OK]\n")
				} else {
					System.out.print(" [NG] *\n")
				}
			}
			var total = 0f
			var k = 0
			while (k < TRANSACTION_COUNT) {
				total = total + success(k) + late(k)
				println(" " + TRANSACTION_NAME(k) + " xact/sec: %,.2f (for %,.2f sec)".format((success(k) + late(k)) / (actualTestTime / 1000f), (actualTestTime / 1000f)))
				k += 1
			}
			System.out.print("\nSTOPPING THREADS\n")
			executor.shutdown()
			try {
				executor.awaitTermination(30, TimeUnit.SECONDS)
			} catch {
				case e: InterruptedException => println("Timed out waiting for executor to terminate")
			}
			iter += 1
		}
		0
	}
}

class XactThread(val tInfo: ThreadInfo, 
	scaleFactor: Int, 
	num_conn: Int, 
	TRANSACTION_COUNT: Int,
	loopConditionChecker: => Boolean,
	xactImpl: XactImpl) extends Thread /*with DatabaseConnector*/{
	/**
	 * Dedicated JDBC connection for this thread.
	 */
	// var conn: Connection = connectToDatabase

	val driver = new XactDriver(tInfo,TRANSACTION_COUNT, xactImpl)
	override def run() {
		try {
			if (DEBUG) {
				logger.debug("Starting driver with: tInfo: " + tInfo + " scaleFactor: " + 
					scaleFactor + 
					" num_conn: " + 
					num_conn)
			}
			driver.runAllTransactions(tInfo, scaleFactor, num_conn, loopConditionChecker)
		} catch {
			case e: Throwable => logger.error("Unhandled exception", e)
		}
	}

	// private def connectToDatabase:Connection = connectToDB(driverClassName, jdbcUrl, db_user, db_password)
}

class XactDriver(
		tInfo: ThreadInfo,
		TRANSACTION_COUNT: Int,
		xactImpl: XactImpl) extends Driver(null, -1, TRANSACTION_COUNT) {

	override def doNextTransaction(tInfo: ThreadInfo, sequence: Int) {
		// counter.value += 1L
		xactImpl.runXact(this, tInfo, sequence, ThreadLocalRandom.current)
	}

	override def runCommandSeq(commandSeq:Seq[ddbt.lib.util.XactCommand])(implicit tInfo: ThreadInfo) = xactImpl.runXactSeq(this, commandSeq)
}

abstract class XactImplSelector {
	def select(impl: Int, numConn: Int): XactImpl
	def xactNames: Array[String]
	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int): Unit
}

abstract class XactImpl {
	@volatile var isCountingOn = false
	def runXact(driver: Driver, tInfo: ThreadInfo, sequence: Int, rnd:ThreadLocalRandom): Unit
	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand])(implicit tInfo: ThreadInfo): Unit
}
