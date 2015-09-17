//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import java.util.concurrent.ThreadLocalRandom
import XactCaseTest2._

object XactCaseTest2 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCaseTest2Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

	val NUM_ACCOUNTS = 2
}

object XactCaseTest2Selector extends XactImplSelector {
	def select(impl: Int) = {
		if (impl == MVCC_IMPL) {
			(new XactCaseTest2MVCC()).init
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000000, 1000000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

class XactCaseTest2MVCC extends XactImpl {
	import ddbt.lib.mvconcurrent._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(false)

	def runXact(driver: Driver, t_num: Int, sequence: Int, rnd: ThreadLocalRandom){
		if (sequence == 0) {
			val timeout = TIMEOUT_T1
			val fromAcc = rnd.nextInt(NUM_ACCOUNTS)
			val toAcc = rnd.nextInt(NUM_ACCOUNTS)
			val amount = rnd.nextInt(5)+1
			driver.execTransaction(t_num, sequence, timeout, isCountingOn) {
				execXact1(fromAcc,toAcc,amount)
			}
		} else if (sequence == 1) {
			val timeout = TIMEOUT_T2
			driver.execTransaction(t_num, sequence, timeout, isCountingOn) {
				execXact2()
			}
		} else {
			throw new IllegalStateException("Error - Unknown sequence")
		}
	}

	def execXact1(fromAccNum:Int, toAccNum:Int, amount:Int) = {
		// logger.info("MVCC - execXact1 - Move Money")
		implicit val xact = tm.begin("Xact1")
		try {
			// val fromAcc = accountsTbl.getEntry(fromAccNum)
			// fromAcc.setEntryValue(Tuple1(fromAcc.row._1 - amount))

			// val toAcc = accountsTbl.getEntry(toAccNum)
			// toAcc.setEntryValue(Tuple1(toAcc.row._1 + amount))
			xact.commit
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

	def execXact2() = {
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
			xact.commit
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

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}

	def init = {
		implicit val xact = tm.begin("InitXact")
		for(i <- 0 until NUM_ACCOUNTS) accountsTbl += (i, Tuple1(i*3L))
		xact.commit
		this
	}
}
