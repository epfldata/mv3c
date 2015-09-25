//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import ddbt.lib.util.ThreadInfo
import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import java.util.concurrent.ThreadLocalRandom
import XactCaseTest3._

/**
 * This class tests the maximum transactional performance of XactBench
 * (i.e. the maximum number of calls to an empty (read-only) transaction
 * program that executes the begin and commit operations with
 * an empty body) with dummy params (without read-only hint)
 *
 * For a single thread:
 *   Xact1 xact/sec: 29,741,308.00
 *   Xact2 xact/sec: 2,974,131.50
 * For two threads:
 *   Xact1 xact/sec: 21,173,984.00
 *   Xact2 xact/sec: 2,117,169.75
 * For four threads:
 *   Xact1 xact/sec: 22,347,884.00
 *   Xact2 xact/sec: 2,233,995.75
 */
object XactCaseTest3 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCaseTest3Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

	val NUM_ACCOUNTS = 2
}

object XactCaseTest3Selector extends XactImplSelector {
	def select(impl: Int, numConn: Int) = {
		if (impl == MVCC_IMPL) {
			(new XactCaseTest3MVCC(numConn)).init
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000, 1000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

class XactCaseTest3MVCC(numConn: Int) extends XactImpl {
	import ddbt.lib.mvconcurrent._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(numConn, false)

	def runXact(driver: Driver, tInfo: ThreadInfo, sequence: Int, rnd: ThreadLocalRandom){
		if (sequence == 0) {
			driver.execTransaction(tInfo, sequence, 0, isCountingOn) { tInfo:ThreadInfo =>
				execXact1(0,0,0)(tInfo)
			}
		} else if (sequence == 1) {
			driver.execTransaction(tInfo, sequence, 0, isCountingOn) { tInfo:ThreadInfo =>
				execXact2(tInfo)
			}
		} else {
			throw new IllegalStateException("Error - Unknown sequence")
		}
	}

	def execXact1(fromAccNum:Int, toAccNum:Int, amount:Int)(implicit tInfo: ThreadInfo) = {
		// logger.info("MVCC - execXact1 - Move Money")
		implicit val xact = tm.begin("Xact1")
		try {
			// val fromAcc = accountsTbl.getEntry(fromAccNum)
			// fromAcc.setEntryValue(Tuple1(fromAcc.row._1 - amount))

			// val toAcc = accountsTbl.getEntry(toAccNum)
			// toAcc.setEntryValue(Tuple1(toAcc.row._1 + amount))
			tm.commit
			1
		} catch {
			case me: MVCCException => {
				error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred")
				error(me)
				tm.rollback
				0
			}
			case e: Throwable => {
				logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred")
				e.printStackTrace
				logger.error(e.toString)
				if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
				tm.rollback
				0
			}
		}
	}

	def execXact2(implicit tInfo: ThreadInfo) = {
		// logger.info("MVCC - execXact2 - Sum Balances")
		implicit val xact = tm.begin("Xact2")
		try {
			// val actualSum = 3L * (NUM_ACCOUNTS * (NUM_ACCOUNTS - 1L)) / 2L
			// var computedSum = 0L
			// accountsTbl.foreach{ case (_,Tuple1(accBal)) =>
			// 	computedSum += accBal
			// }
			// if(computedSum != actualSum) {
			// 	logger.error("There was an error in the computation => computedSum (%d) != actualSum (%d)".format(computedSum, actualSum))
			// 	// throw new RuntimeException("There was an error in the computation")
			// }
			tm.commit
			1
		} catch {
			case me: MVCCException => {
				error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred")
				error(me)
				tm.rollback
				0
			}
			case e: Throwable => {
				logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred")
				logger.error(e.toString)
				if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
				tm.rollback
				0
			}
		}
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand])(implicit tInfo: ThreadInfo): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}

	def init = {
		// implicit val xact = tm.begin("InitXact")(new ThreadInfo(0))
		// for(i <- 0 until NUM_ACCOUNTS) accountsTbl += (i, Tuple1(i*3L))
		// xact.commit
		this
	}
}
