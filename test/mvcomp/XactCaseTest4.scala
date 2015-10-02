//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import ddbt.lib.util.ThreadInfo
import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import XactCaseTest4._

/**
 * This class tests the maximum transactional performance of XactBench
 * (i.e. the maximum number of calls to an empty (read-only) transaction
 * program that executes the begin and commit operations with
 * an empty body) with dummy params (without read-only hint)
 * that each one waits for one nano second
 *
 * For a single thread:
 *   Xact1 xact/sec: 77,642.27
 *   Xact2 xact/sec: 7,764.65
 * For two threads:
 *   Xact1 xact/sec: 137,875.36
 *   Xact2 xact/sec: 13,787.11
 * For four threads:
 *   Xact1 xact/sec: 262,354.59
 *   Xact2 xact/sec: 26,232.90
 */
object XactCaseTest4 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCaseTest4Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

	val NUM_ACCOUNTS = 2
}

object XactCaseTest4Selector extends XactImplSelector {
	def select(impl: Int, numConn: Int) = {
		if (impl == MVCC_IMPL) {
			(new XactCaseTest4MVCC(numConn)).init
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000, 1000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

class XactCaseTest4MVCC(numConn: Int) extends XactImpl {
	import ddbt.lib.mvconcurrent._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(numConn, false)

	def runXact(driver: Driver, tInfo: ThreadInfo, sequence: Int){
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
			java.util.concurrent.locks.LockSupport.parkNanos(1)
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
			java.util.concurrent.locks.LockSupport.parkNanos(1)
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
