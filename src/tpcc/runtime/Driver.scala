package ddbt.tpcc.loadtest

import java.io.PrintWriter
import java.sql.Connection
import java.sql.SQLException
import java.util.Arrays
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import java.util.concurrent._
import Driver._

object Driver {

  private val logger = LoggerFactory.getLogger(classOf[TpccDriver])

  private val DEBUG = logger.isDebugEnabled

  /**
   * For debug use only.
   */
  private val DETECT_LOCK_WAIT_TIMEOUTS = false

}

class XactCounter {
  var value = 0L
}
abstract class Driver(var conn: java.sql.Connection, 
    fetchSize: Int, 
    TRANSACTION_COUNT: Int) {

  val success = new Array[Long](TRANSACTION_COUNT)

  val late = new Array[Long](TRANSACTION_COUNT)

  val retry = new Array[Long](TRANSACTION_COUNT)

  val failure = new Array[Long](TRANSACTION_COUNT)

  val counter = new XactCounter

  java.util.Arrays.fill(success, 0L)
  java.util.Arrays.fill(late, 0L)
  java.util.Arrays.fill(retry, 0L)
  java.util.Arrays.fill(failure, 0L)

  var num_ware: Int = _

  var num_conn: Int = _

  var num_node: Int = _

  var max_rt: Array[Float] = new Array[Float](TRANSACTION_COUNT)
  for (i <- 0 until TRANSACTION_COUNT) {
    max_rt(i) = 0f
  }

  val MAX_RETRY = 2000

  val RTIME_NEWORD = 5 * 1000

  val RTIME_PAYMENT = 5 * 1000

  val RTIME_ORDSTAT = 5 * 1000

  val RTIME_DELIVERY = 5 * 1000

  val RTIME_SLEV = 20 * 1000

  // val exec = Executors.newSingleThreadExecutor()

  var startTime = 0L

  def runAllTransactions(t_num: Int, numWare: Int, numConn: Int, loopConditionChecker: => Boolean, maximumNumberOfTransactionsToExecute:Int = 0, commandSeq:Seq[ddbt.lib.util.XactCommand]=null): Int = {

    startTime = (System.currentTimeMillis() % 1000) * 1000

    num_ware = numWare
    num_conn = numConn
    var count = 0
    var seqCounter = 0
    if(commandSeq == null) {
      val seq = Util.seq
      val seqLen = seq.length
      val seqSectionLen = seq.length / numConn
      val seqSectionStart = t_num * seqSectionLen
      val seqSectionEnd = (t_num + 1) * seqSectionLen
      seqCounter = seqSectionStart
      var sequence = seq(seqCounter)
      // var sequence = Util.seqGet()
        // println("seqSectionStart = %d".format(seqSectionStart))
        // println("seqSectionEnd = %d".format(seqSectionEnd))
      while (loopConditionChecker || (count < maximumNumberOfTransactionsToExecute)) {
        try {
        // println("sequence = %d".format(sequence))

          // if (DEBUG) logger.debug("BEFORE runAllTransactions: sequence: " + sequence)
          // if (DETECT_LOCK_WAIT_TIMEOUTS) {
          //   val _sequence = sequence
          //   val t = new FutureTask[Any](new Callable[Any]() {

          //     def call(): AnyRef = {
          //       doNextTransaction(t_num, _sequence)
          //       null
          //     }
          //   })
          //   exec.execute(t)
          //   try {
          //     t.get(15, TimeUnit.SECONDS)
          //   } catch {
          //     case e: InterruptedException => {
          //       logger.error("InterruptedException", e)
          //       Tpcc.activate_transaction = false
          //     }
          //     case e: ExecutionException => {
          //       logger.error("Unhandled exception", e)
          //       Tpcc.activate_transaction = false
          //     }
          //     case e: TimeoutException => {
          //       logger.error("Detected Lock Wait", e)
          //       Tpcc.activate_transaction = false
          //     }
          //   }
          // } else {
            doNextTransaction(t_num, sequence)
          // }
          count += 1
        } catch {
          case th: Throwable => {
            logger.error("FAILED", th)
            Tpcc.activate_transaction = false
            try {
              if(conn != null) conn.rollback()
            } catch {
              case e: SQLException => logger.error("", e)
            }
            return -1
          }
        }
        // finally {
        //   if (DEBUG) logger.debug("AFTER runAllTransactions: sequence: " + sequence)
        // }
        seqCounter += 1
        if(seqCounter >= seqSectionEnd) {
          // println("sequence restarted")
          seqCounter = seqSectionStart
        }
        sequence = seq(seqCounter)
        // sequence = Util.seqGet()
      }
    } else {
      runCommandSeq(commandSeq)
      count += 1
    }
    logger.debug("Driver terminated after {} transactions", count)
    (0)
  }

  def doNextTransaction(t_num: Int, sequence: Int): Unit

  def runCommandSeq(commandSeq:Seq[ddbt.lib.util.XactCommand]): Unit

  def execTransaction(t_num:Int, xactId:Int, timeout: Int, counting_on: Boolean)(xact: =>Int): Int = {
    var i = 0
    // var rt = 0f
    var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    while (i < MAX_RETRY) {
      ret = xact
      // endTime = System.currentTimeMillis()
      if (ret >= 1) {
        // rt = (endTime - beginTime).toFloat
        // if (rt > max_rt(xactId)) max_rt(xactId) = rt
        // RtHist.histInc(xactId, rt)
        if (counting_on) {
          // if (rt < timeout) {
            success(xactId) += 1L
            // success2(xactId)(t_num) += 1L
          // } else {
            // late(xactId) += 1L
            // late2(xactId)(t_num) += 1L
          // }
        }
        return (1)
      } else {
        if (counting_on) {
          retry(xactId) += 1L
          // retry2(xactId)(t_num) += 1L
        }
      }
      i += 1
    }
    if (counting_on) {
      retry(xactId) -= 1L
      // retry2(xactId)(t_num) -= 1L
      failure(xactId) += 1L
      // failure2(xactId)(t_num) += 1L
    }
    (0)
  }

  def execTransactionWithProfiling(t_num:Int, xactId:Int, timeout: Int, counting_on: =>Boolean)(xact: =>Int): Int = {
    var i = 0
    var rt = 0f
    var ret = 0
    var endTime = 0L
    var beginTime = System.currentTimeMillis()
    while (i < MAX_RETRY) {
      ret = xact
      endTime = System.currentTimeMillis()
      if (ret >= 1) {
        rt = (endTime - beginTime).toFloat
        if (rt > max_rt(xactId)) max_rt(xactId) = rt
        RtHist.histInc(xactId, rt)
        if (counting_on) {
          if (rt < timeout) {
            success(xactId) += 1L
            // success2(xactId)(t_num) += 1L
          } else {
            late(xactId) += 1L
            // late2(xactId)(t_num) += 1L
          }
        }
        return (1)
      } else {
        if (counting_on) {
          retry(xactId) += 1L
          // retry2(xactId)(t_num) += 1L
        }
      }
      i += 1
    }
    if (counting_on) {
      retry(xactId) -= 1L
      // retry2(xactId)(t_num) -= 1L
      failure(xactId) += 1L
      // failure2(xactId)(t_num) += 1L
    }
    (0)
  }
}
