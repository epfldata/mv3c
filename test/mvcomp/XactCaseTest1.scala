//MVCC vs. MVC3T
package ddbt.lib.mvcomp

import ddbt.lib.util.ThreadInfo
import ddbt.lib.util.XactImpl
import ddbt.lib.util.XactImplSelector
import ddbt.lib.util.XactBench
import XactBench._
import ddbt.tpcc.loadtest.Driver
import ddbt.tpcc.loadtest.Util
import XactCaseTest1._

/**
 * This class tests the maximum performance of XactBench
 * (i.e. the maximum number of calls to an empty function
 * alognside with generating random parameters for the call)
 *
 * For a single thread:
 *   Xact1 xact/sec: 93,775,784.00
 *   Xact2 xact/sec: 9,377,556.00
 * For two threads:
 *   Xact1 xact/sec: 182,120,416.00
 *   Xact2 xact/sec: 18,211,978.00
 * For four threads:
 *   Xact1 xact/sec: 341,656,480.00
 *   Xact2 xact/sec: 34,165,612.00
 */
object XactCaseTest1 {

	val MVCC_IMPL = 1
	val MVC3T_IMPL = 2

	val TIMEOUT_T1 = 5000
	val TIMEOUT_T2 = 5000

	def main(argv:Array[String]) {
		val xactRunner = new XactBench(XactCaseTest1Selector)
		xactRunner.init
		xactRunner.runBenchmark(true, argv)
	}

	val NUM_ACCOUNTS = 2
}

object XactCaseTest1Selector extends XactImplSelector {
	def select(impl: Int, numConn: Int) = {
		if (impl == MVCC_IMPL) {
			(new XactCaseTest1MVCC(numConn)).init
		} else {
			throw new RuntimeException("No implementation selected.")
		}
	}

	def seqInit(t1: Int, t2: Int, t3: Int, t4: Int, t5: Int) {
		Util.seqInit(10000, 1000)
	}

	def xactNames = Array("Xact1", "Xact2")
}

class XactCaseTest1MVCC(numConn: Int) extends XactImpl {
	import ddbt.lib.mvconcurrent._
	import TransactionManager._
	val accountsTbl = new ConcurrentSHMapMVCC[Int /*Account Number*/,Tuple1[Long] /*Boolean*/]("ACCOUNTS_TBL")

	val tm = new TransactionManager(numConn, false)

	def runXact(driver: Driver, tInfo: ThreadInfo, sequence: Int){
		val rnd = java.util.concurrent.ThreadLocalRandom.current
		if (sequence == 0) {
			val fromAcc = rnd.nextInt(NUM_ACCOUNTS)
			val toAcc = rnd.nextInt(NUM_ACCOUNTS)
			val amount = rnd.nextInt(5)+1
			driver.execTransaction(tInfo, sequence, 0, isCountingOn) { tInfo:ThreadInfo =>
				execXact1(fromAcc,toAcc,amount)(tInfo)
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
		1
	}

	def execXact2(implicit tInfo: ThreadInfo) = {
		1
	}

	def runXactSeq(driver: Driver, commandSeq:Seq[ddbt.lib.util.XactCommand])(implicit tInfo: ThreadInfo): Unit = {
		// commandSeq.foreach { x =>
		// }
		throw new UnsupportedOperationException
	}

	def init = {
		implicit val xact = tm.begin("InitXact")(new ThreadInfo(0))
		for(i <- 0 until NUM_ACCOUNTS) accountsTbl += (i, Tuple1(i*3L))
		xact.commit
		this
	}
}
