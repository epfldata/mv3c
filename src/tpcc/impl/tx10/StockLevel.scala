package ddbt.tpcc.tx10

import ddbt.lib.util.ThreadInfo
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import StockLevel._
import ddbt.tpcc.tx.TpccTable._
import ddbt.tpcc.tx.MVCCTpccTableV3._
import ddbt.lib.mvconcurrent._
import TransactionManager._

object StockLevel {

  private val logger = LoggerFactory.getLogger(classOf[StockLevel])

  private val DEBUG = logger.isDebugEnabled

  private val TRACE = logger.isTraceEnabled

  private val SHOW_OUTPUT = ddbt.tpcc.loadtest.TpccConstants.SHOW_OUTPUT
}

/**
 * StockLevel Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
class StockLevel extends InMemoryTxImplViaMVCCTpccTableV3 with IStockLevelInMem {

  /**
   * @param w_id is warehouse id
   * @param d_id is district id
   * @param threshold is the threshold for the items in stock
   *
   * Table interactions:
   *   - [District: R] n
   *      + findDistrictnextOrderId
   *   - [Stock: R] in
   *      + findOrderLineStockRecentItemsUnderThresholds
   *   - [OrderLine: R] in
   *      + findOrderLineStockRecentItemsUnderThresholds
   *
   */
  override def stockLevelTx(t_num: Int, w_id: Int, d_id: Int, threshold: Int)(implicit tInfo: ThreadInfo): Int= {
    implicit val xact = ISharedData.begin("slev")
    if(ISharedData.isUnitTestEnabled) xact.setCommand(StockLevelCommand(t_num, w_id, d_id, threshold))
    try {
      val o_id = StockLevelTxOps.findDistrictnextOrderId(w_id,d_id)
      val stock_count = StockLevelTxOps.findOrderLineStockRecentItemsUnderThresholds(w_id, d_id, o_id, threshold)

      val output: StringBuilder = new StringBuilder
      output.append("\n+-------------------------- STOCK-LEVEL --------------------------+")
      output.append("\n Warehouse: ").append(w_id)
      output.append("\n District:  ").append(d_id)
      output.append("\n\n Stock Level Threshold: ").append(threshold)
      output.append("\n Low Stock Count:       ").append(stock_count)
      output.append("\n+-----------------------------------------------------------------+\n\n")
      if(SHOW_OUTPUT) logger.info(output.toString)

      ISharedData.commit
      1
    } catch {
      case me: MVCCException => {
        error("An error occurred in handling StockLevel transaction for warehouse=%d, district=%d, threshold=%d".format(w_id,d_id,threshold))
        error(me)
        ISharedData.rollback
        0
      }
      case e: Throwable => {
        logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred in handling StockLevel transaction for warehouse=%d, district=%d, threshold=%d".format(w_id,d_id,threshold))
        logger.error(e.toString)
        if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
        ISharedData.rollback
        0
      }
    }
  }

  object StockLevelTxOps {
      def findDistrictnextOrderId(w_id:Int, d_id:Int)(implicit xact:Transaction) = {
        ISharedData.findDistrict(w_id,d_id)._9
      }

      def findOrderLineStockRecentItemsUnderThresholds(w_id:Int, d_id:Int, o_id:Int, threshold:Int)(implicit xact:Transaction) = {
        val unique_ol_i_id = new ddbt.lib.shm.SHSet[Int]
        var i = o_id-20
        while(i < o_id) {
          ISharedData.orderLineTblSlice(0, (i, d_id, w_id), { case (_,(ol_i_id,_,_,_,_,_)) =>
            val (s_quantity,_,_,_,_,_,_,_,_,_,_,_,_,_,_) = ISharedData.findStock(ol_i_id,w_id)
            if(s_quantity < threshold) {
              unique_ol_i_id += ol_i_id
            }
          })
          i+=1
        }

        unique_ol_i_id.size
      }
  }
}

