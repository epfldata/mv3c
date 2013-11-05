package ddbt.tpcc.tx
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.loadtest.TpccUnitTest._

/**
 * Delivery Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
object Delivery {

  //Tables
  //val newOrderTbl = new HashSet[(Int,Int,Int)]

  //Partial Tables (containing all rows, but not all columns)
  //removed columns are commented out
  //val orderPartialTbl = new HashMap[(Int,Int,Int),(Int/*,Date*/,Option[Int]/*,Int,Boolean*/)]
  //only the slice over first three key parts is used
  //val orderLinePartialTbl = new HashMap[(Int,Int,Int,Int),(/*Int,Int,*/Option[Date]/*,Int*/,Double/*,String*/)]
  //val customerPartialTbl = new HashMap[(Int,Int,Int),(/*String,String,String,String,String,String,String,String,String,Date,String,Double,Double,*/Double/*,Double,Int*/,Int/*,String*/)]
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
   *      + findOrderCID
   *     and W in
   *      + updateOrderCarrier
   *   - [OrderLine: RW] where R in
   *      + findOrderLineTotalAmount
   *     and W in
   *      + updateOrderLineDeliveryDate
   *   - [Customer: W] in
   *      + updateCustomerBalance
   */
  def deliveryTx(datetime:Date, w_id: Int, o_carrier_id: Int): Int = {
    try {
      val DIST_PER_WAREHOUSE = 10
      val orderIDs = new Array[Int](10)
      var d_id = 1
      while (d_id <= DIST_PER_WAREHOUSE) {
        DeliveryTxOps.findFirstNewOrder(w_id,d_id) match {
          case Some(no_o_id) => {
            orderIDs(d_id - 1) = no_o_id
            DeliveryTxOps.deleteNewOrder(w_id,d_id,no_o_id)
            val c_id = DeliveryTxOps.findOrderCID(w_id,d_id,no_o_id)
            DeliveryTxOps.updateOrderCarrier(w_id,d_id,no_o_id,c_id,o_carrier_id)
            DeliveryTxOps.updateOrderLineDeliveryDate(w_id,d_id,no_o_id,datetime)
            val ol_total = DeliveryTxOps.findOrderLineTotalAmount(w_id,d_id,no_o_id)
            DeliveryTxOps.updateCustomerBalance(w_id,d_id,c_id,ol_total)
          }
          case None => orderIDs(d_id - 1) = 0
        }
        d_id += 1
      }
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
      println(output.toString)
      skippedDeliveries
    } catch {
      case e: Throwable => {
        println("An error occurred in handling Delivery transaction for warehouse=%d, carrier=%d".format(w_id,o_carrier_id))
        1
      }
    }
  }

  object DeliveryTxOps {
    def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int):Option[Int] = {
      var first_no_o_id:Option[Int] = None
      SharedData.newOrderTbl.foreach { case (no_o_id, no_d_id, no_w_id) =>
        if(no_d_id == no_d_id_input && no_w_id == no_w_id_input) {
          first_no_o_id match {
            case None => first_no_o_id = Some(no_o_id)
            case Some(x) => if(no_o_id < x) first_no_o_id = Some(no_o_id)
          }
        }
      }
      first_no_o_id
    }

    def deleteNewOrder(no_w_id:Int, no_d_id:Int, no_o_id:Int) = {
      SharedData.onDelete_NewOrder(no_o_id,no_d_id,no_w_id)
    }

    def findOrderCID(o_w_id:Int, o_d_id:Int, o_id:Int) = {
      SharedData.orderTbl((o_id,o_d_id,o_w_id))._1
    }

    def updateOrderCarrier(o_w_id:Int, o_d_id:Int, o_id:Int, o_c_id:Int, o_carrier_id:Int) {
      SharedData.onUpdate_Order_forDelivery(o_id,o_d_id,o_w_id, o_c_id,Some(o_carrier_id))
    }

    def updateOrderLineDeliveryDate(ol_w_id:Int, ol_d_id:Int, ol_o_id:Int, ol_delivery_d:Date) {
      val ol_numbers = new ArrayBuffer[(Int,Date,Double)]
      //should be replaced by a slice over first three key parts
      SharedData.orderLineTbl.foreach{ ol =>
        if(ol._1._1 == ol_o_id && ol._1._2 == ol_d_id && ol._1._3 == ol_w_id) {
          SharedData.onUpdateOrderLine(ol_o_id,ol_d_id,ol_w_id,ol._1._4, ol._2._1, ol._2._2, Some(ol_delivery_d), ol._2._4,ol._2._5,ol._2._6)
        }
      }
    }

    def findOrderLineTotalAmount(ol_w_id:Int, ol_d_id:Int, ol_o_id:Int):Double = {
      var ol_total = 0.0
      //should be replaced by a slice over first three key parts
      SharedData.orderLineTbl.foreach{ ol =>
        if(ol._1._1 == ol_o_id && ol._1._2 == ol_d_id && ol._1._3 == ol_w_id) {
          ol_total += ol._2._5
        }
      }
      ol_total
    }

    def updateCustomerBalance(c_w_id:Int, c_d_id:Int, c_id:Int, ol_total:Double) = {
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = SharedData.customerTbl((c_id,c_d_id,c_w_id))
      SharedData.onUpdateCustomer(c_id,c_d_id,c_w_id,c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance+ol_total,c_ytd_payment,c_payment_cnt,c_delivery_cnt+1,c_data)
    }
  }
}

