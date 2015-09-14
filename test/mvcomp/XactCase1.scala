//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import scala.util.Random
import XactCase1._

object XactCase1 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	val RANDOM_SEED = 2015

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCase1Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

	val NUM_ACCOUNTS = 1000000
}

object XactCase1Selector extends XactImplSelector {
	def select(impl: Int) = {
		if (impl == MVCC_IMPL) {
			(new XactCase1MVCC()).init
		} else if (impl == MVC3T_IMPL) {
			(new XactCase1MVC3T()).init
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000000, 1000000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

class XactCase1MVCC extends XactImpl {
	import ddbt.lib.mvconcurrent._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(false)

	val rnd = new Random(RANDOM_SEED)

	def runXact(driver: Driver, t_num: Int, sequence: Int){
		if (sequence == 0) {
			val timeout = TIMEOUT_T1
			val fromAcc = rnd.nextInt(NUM_ACCOUNTS)
			val toAcc = rnd.nextInt(NUM_ACCOUNTS)
			val amount = rnd.nextInt(5)+1
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
				execXact1(fromAcc,toAcc,amount)
			}
		} else if (sequence == 1) {
			val timeout = TIMEOUT_T2
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
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
			val fromAcc = accountsTbl.getEntry(fromAccNum)
			fromAcc.setEntryValue(Tuple1(fromAcc.row._1 - amount))

			val toAcc = accountsTbl.getEntry(toAccNum)
			toAcc.setEntryValue(Tuple1(toAcc.row._1 + amount))
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
			val actualSum = 3L * (NUM_ACCOUNTS * (NUM_ACCOUNTS - 1L)) / 2L
			var computedSum = 0L
			accountsTbl.foreach{ case (_,Tuple1(accBal)) =>
				computedSum += accBal
			}
			if(computedSum != actualSum) {
				logger.error("There was an error in the computation => computedSum (%d) != actualSum (%d)".format(computedSum, actualSum))
				// throw new RuntimeException("There was an error in the computation")
			}
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

class XactCase1MVC3T extends XactImpl {
	import ddbt.lib.mvc3t._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVC3T[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(false)

	val rnd = new Random(RANDOM_SEED)
	var amt = 0
	def runXact(driver: Driver, t_num: Int, sequence: Int){
		if (sequence == 0) {
			val timeout = TIMEOUT_T1
			val fromAcc = rnd.nextInt(NUM_ACCOUNTS)
			val toAcc = rnd.nextInt(NUM_ACCOUNTS)
			amt += 1
			val amount = amt
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
				execXact1(fromAcc,toAcc,amount)
			}
		} else if (sequence == 1) {
			val timeout = TIMEOUT_T2
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
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
			if(fromAccNum != toAccNum) {
				// logger.error(xact+" > Move %d from (%d) to (%d).".format(amount, fromAccNum, toAccNum))
				accountsTbl.getPred(fromAccNum).getEntry({ case (fromAcc, getPred) =>
					accountsTbl.updatePred(fromAccNum).action(fromAcc, {
						// logger.error(xact+" AccFrom #%d = %d -> %d".format(fromAcc.getKey, fromAcc.row._1, fromAcc.row._1 - amount))
						Tuple1(fromAcc.row._1 - amount)
					}, List(getPred))
				}, Nil)
				accountsTbl.getPred(toAccNum).getEntry({ case (toAcc, getPred) =>
					accountsTbl.updatePred(toAccNum).action(toAcc, {
						// logger.error(xact+" AccTo   #%d = %d -> %d".format(toAcc.getKey, toAcc.row._1, toAcc.row._1 + amount))
						Tuple1(toAcc.row._1 + amount)
					}, List(getPred))
				}, Nil)
			}
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

	def execXact2() = {
		// logger.info("MVCC - execXact2 - Sum Balances")
		implicit val xact = tm.begin("Xact2")
		try {
			val actualSum = 3L * (NUM_ACCOUNTS * (NUM_ACCOUNTS - 1L)) / 2L
			var computedSum = 0L
			accountsTbl.foreachPred.apply({ case (dv, _) =>
				val accBal = dv.row._1
				// logger.error(xact+" Acc #%d = %d".format(dv.getKey, dv.row._1))
				computedSum += accBal
			}, Nil)
			if(computedSum != actualSum) {
				logger.error(xact+" > There was an error in the computation => computedSum (%d) != actualSum (%d) -> difference is %d".format(computedSum, actualSum, Math.abs(computedSum - actualSum)))
				System.exit(0)
				// throw new RuntimeException("There was an error in the computation")
			}
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
