package ddbt.tpcc.tx9

import ddbt.lib.util.ThreadInfo
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import ddbt.tpcc.tx.MVCCTpccTableV2._
import Delivery._

object Delivery {

  private val logger = LoggerFactory.getLogger(classOf[Delivery])

  private val DEBUG = logger.isDebugEnabled

  private val TRACE = logger.isTraceEnabled

  private val SHOW_OUTPUT = ddbt.tpcc.loadtest.TpccConstants.SHOW_OUTPUT
}

/**
 * Delivery Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
class Delivery extends InMemoryTxImplViaMVCCTpccTableV2 with IDeliveryInMem {

  /**
   * @param w_id is warehouse id
   * @param o_carrier_id is the carrier id for this warehouse
   *
   * Table interactions:
   *   - [NewOrder: RW] where R in
   *      + findFirstNewOrder
   *     and W in
   *      + deleteNewOrder
   *   - [Order: RW] where R in
   *      + updateOrderCarrierAndFindCID
   *   - [OrderLine: RW] where R in
   *      + findOrderLineTotalAmount
   *     and W in
   *      + updateOrderLineDeliveryDate
   *   - [Customer: W] in
   *      + updateCustomerBalance
   */
  override def deliveryTx(datetime:Date, w_id: Int, o_carrier_id: Int)(implicit tInfo: ThreadInfo): Int = {
    try {
      implicit val xact = ISharedData.begin

      val DIST_PER_WAREHOUSE = 10
      val orderIDs = new Array[Int](10)
      var d_id = 1
      while (d_id <= DIST_PER_WAREHOUSE) {
        DeliveryTxOps.findFirstNewOrder(w_id,d_id) match {
          case Some(no_o_id) => {
            orderIDs(d_id - 1) = no_o_id
            DeliveryTxOps.deleteNewOrder(w_id,d_id,no_o_id)
            var c_id = 0
            DeliveryTxOps.updateOrderCarrierAndFindCID(w_id,d_id,no_o_id,(cv => { c_id = cv._1; ((cv._1,cv._2,Some(o_carrier_id),cv._4,cv._5)) }))
            val ol_total = DeliveryTxOps.updateOrderLineDeliveryDateAndFindOrderLineTotalAmount(w_id,d_id,no_o_id,datetime)
            DeliveryTxOps.updateCustomerBalance(w_id,d_id,c_id,ol_total)
          }
          case None => orderIDs(d_id - 1) = 0
        }
        d_id += 1
      }

      if(SHOW_OUTPUT) {
        val output: StringBuilder = new StringBuilder
        output.append("\n+---------------------------- DELIVERY ---------------------------+\n")
        output.append(" Date: ").append(datetime)
        output.append("\n\n Warehouse: ").append(w_id)
        output.append("\n Carrier:   ").append(o_carrier_id)
        output.append("\n\n Delivered Orders\n")
        var skippedDeliveries: Int = 0
        var i: Int = 1
        while (i <= 10) {
          if (orderIDs(i - 1) >= 0) {
            output.append("  District ")
            output.append(if (i < 10) " " else "")
            output.append(i)
            output.append(": Order number ")
            output.append(orderIDs(i - 1))
            output.append(" was delivered.\n")
          }
          else {
            output.append("  District ")
            output.append(if (i < 10) " " else "")
            output.append(i)
            output.append(": No orders to be delivered.\n")
            skippedDeliveries += 1
          }
          i += 1
        }
        output.append("+-----------------------------------------------------------------+\n\n")
        logger.info(output.toString)
      }
      // skippedDeliveries

      ISharedData.commit
      1
    } catch {
      case e: Throwable => {
        logger.error("An error occurred in handling Delivery transaction for warehouse=%d, carrier=%d".format(w_id,o_carrier_id))
        e.printStackTrace
        0
      }
    }
  }

  object DeliveryTxOps {
    def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int)(implicit xact:Transaction):Option[Int] = {
      ISharedData.findFirstNewOrder(no_w_id_input, no_d_id_input)
    }

    def deleteNewOrder(no_w_id:Int, no_d_id:Int, no_o_id:Int)(implicit xact:Transaction) = {
      ISharedData.onDelete_NewOrder(no_o_id,no_d_id,no_w_id)
    }

    def updateOrderCarrierAndFindCID(o_w_id:Int, o_d_id:Int, o_id:Int, updateFunc:((Int, Date, Option[Int], Int, Boolean)) => (Int, Date, Option[Int], Int, Boolean))(implicit xact:Transaction) = {
      ISharedData.onUpdate_Order_byFunc(o_id,o_d_id,o_w_id, updateFunc)
    }

    def updateOrderLineDeliveryDateAndFindOrderLineTotalAmount(ol_w_id_input:Int, ol_d_id_input:Int, ol_o_id_input:Int, ol_delivery_d_input:Date)(implicit xact:Transaction):Float = {
      var ol_total = 0f
      ISharedData.orderLineTblSliceEntry(0, (ol_o_id_input, ol_d_id_input, ol_w_id_input), { cv => 
        val k = cv.getKey
        val v = k.getValue
        ol_total += v._5
        //TODO why it does not work?
        // k.setValue(v.copy(_3 = Some(ol_delivery_d_input)))
        ISharedData.tm.orderLineTbl(k.getKey) = v.copy(_3 = Some(ol_delivery_d_input))
      })
      ol_total
    }

    def updateCustomerBalance(c_w_id:Int, c_d_id:Int, c_id:Int, ol_total:Float)(implicit xact:Transaction) = {
      ISharedData.onUpdateCustomer_byFunc(c_id,c_d_id,c_w_id, {
        c:((String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String)) => c match {
          case (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) => 
            (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance+ol_total,c_ytd_payment,c_payment_cnt,c_delivery_cnt+1,c_data)
        }
      })
    }
  }
}

