package ddbt.lib.mvc3t

import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import XactCase1._

object XactCase1 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCase1Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

}

object XactCase1Selector extends XactImplSelector {
	def select(impl: Int) = {
		if (impl == MVCC_IMPL) {
			XactCase1MVCC
		} else if (impl == MVC3T_IMPL) {
			XactCase1MVC3T
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000000, 10000000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

object XactCase1MVCC extends XactImpl {
	def runXact(driver: Driver, t_num: Int, sequence: Int){
		if (sequence == 0) {
			val timeout = TIMEOUT_T1
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
				execXact1()
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

	def execXact1() = {
		// logger.info("MVCC - execXact1")
		Thread.sleep(200)
		1
	}

	def execXact2() = {
		// logger.info("MVCC - execXact2")
		Thread.sleep(1600)
		1
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}
}

object XactCase1MVC3T extends XactImpl {
	def runXact(driver: Driver, t_num: Int, sequence: Int){
		if (sequence == 0) {
			val timeout = TIMEOUT_T1
			driver.execTransaction(t_num, sequence, timeout, XactBench.counting_on) {
				execXact1()
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

	def execXact1() = {
		// logger.info("MVC3T - execXact1")
		Thread.sleep(100)
		1
	}

	def execXact2() = {
		// logger.info("MVC3T - execXact2")
		Thread.sleep(200)
		1
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}
}
