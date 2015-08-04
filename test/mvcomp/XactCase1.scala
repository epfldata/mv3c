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

	val tm = new TransactionManager(false)
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

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
		val fromAccBal = accountsTbl(fromAccNum)._1
		if(fromAccBal > amount) {
			accountsTbl(fromAccNum) = Tuple1(fromAccBal - amount)
			accountsTbl.update(toAccNum, toAccBal => Tuple1(toAccBal._1 + amount))
		}
		xact.commit
		1
	}

	def execXact2() = {
		// logger.info("MVCC - execXact2 - Sum Balances")
		implicit val xact = tm.begin("Xact2")
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
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}

	def init = {
		implicit val xact = tm.begin("InitXact")
		for(i <- 1 until NUM_ACCOUNTS) accountsTbl += (i, Tuple1(i*3L))
		xact.commit
		this
	}
}

class XactCase1MVC3T extends XactImpl {
	import ddbt.lib.mvc3t._

	val tm = new TransactionManager(false)
	val accountsTbl = new ConcurrentSHMapMVC3T[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

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
		val fromAccBal = accountsTbl(fromAccNum)._1
		if(fromAccBal > amount) {
			accountsTbl(fromAccNum) = Tuple1(fromAccBal - amount)
			accountsTbl.update(toAccNum, toAccBal => Tuple1(toAccBal._1 + amount))
		}
		xact.commit
		1
	}

	def execXact2() = {
		// logger.info("MVCC - execXact2 - Sum Balances")
		implicit val xact = tm.begin("Xact2")
		val actualSum = 3L * (NUM_ACCOUNTS * (NUM_ACCOUNTS - 1L)) / 2L
		var computedSum = 0L
		accountsTbl.foreach{ case (_,Tuple1(accBal)) =>
			computedSum += accBal
		}
		if(computedSum != actualSum) {
			throw new RuntimeException("There was an error in the computation")
		}
		xact.commit
		1
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}

	def init = {
		implicit val xact = tm.begin("InitXact")
		for(i <- 1 until NUM_ACCOUNTS) accountsTbl += (i, Tuple1(i*3L))
		xact.commit
		this
	}
}
