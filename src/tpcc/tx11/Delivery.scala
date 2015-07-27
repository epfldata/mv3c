package ddbt.tpcc.tx11
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import ddbt.tpcc.tx.MVCCTpccTableV4._
import ddbt.tpcc.tx.TpccTable._
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
class Delivery extends InMemoryTxImplViaMVCCTpccTableV4 with IDeliveryInMem {

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
  override def deliveryTx(datetime:Date, w_id: Int, o_carrier_id: Int): Int = {
    implicit val xact = ISharedData.begin("delivery")
    if(ISharedData.isUnitTestEnabled) xact.setCommand(DeliveryCommand(datetime, w_id, o_carrier_id))
    try {
      val DIST_PER_WAREHOUSE = 10
      val orderIDs = new Array[Int](10)
      var d_id = 1
      while (d_id <= DIST_PER_WAREHOUSE) {
        var first_no_o_id = Integer.MAX_VALUE
        var first_newOrder:Option[DeltaVersion[NewOrderTblKey,NewOrderTblValue]] = None
        val firstNewOrderPred = ISharedData.tm.newOrderTbl.slicePred(0, (d_id, w_id))
        val firstNewOrderSlice = firstNewOrderPred.slice
        firstNewOrderSlice.foreachEntry { newOrder => newOrder.getKey.getKey match { case (no_o_id,_,_) =>
            if(no_o_id <= first_no_o_id) {
              first_no_o_id = no_o_id
              first_newOrder = Some(newOrder.getKey)
            }
          }
        }
        new ClosureTransition(List(firstNewOrderPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
          var new_first_no_o_id = Integer.MAX_VALUE
          var new_first_newOrder:Option[DeltaVersion[NewOrderTblKey,NewOrderTblValue]] = None
          firstNewOrderSlice.foreachEntry { newOrder => newOrder.getKey.getKey match { case (no_o_id,_,_) =>
              if(no_o_id <= new_first_no_o_id) {
                new_first_no_o_id = no_o_id
                new_first_newOrder = Some(newOrder.getKey)
              }
            }
          }
          if(first_no_o_id != new_first_no_o_id) {
            throw new RuntimeException("Change handler for firstNewOrderPred (because first_no_o_id<"+first_no_o_id+"> != new_first_no_o_id<"+new_first_no_o_id+">) is not fully implemented!")
          }
          Nil
        }), () => {
          var outputVersions = List[DeltaVersion[_,_]]()
          first_newOrder match {
            case Some(newOrder) => {
              val no_o_id = newOrder.getKey._1
              orderIDs(d_id - 1) = no_o_id
              outputVersions = (ISharedData.tm.newOrderTbl -= newOrder) :: outputVersions

              var orderPred = ISharedData.tm.orderTbl.getPred((no_o_id,d_id,w_id))
              val order = orderPred.getEntry
              outputVersions = order.setEntryValue(order.row.copy(_3 = Some(o_carrier_id))) :: outputVersions

              var orderLines = List[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]()
              var orderLinePred = ISharedData.tm.orderLineTbl.slicePred(0, (no_o_id, d_id, w_id))
              orderLinePred.slice.foreachEntry{ v => 
                orderLines = v.getKey :: orderLines
              }
              orderLines.foreach { orderLine =>
                outputVersions = orderLine.setEntryValue(orderLine.row.copy(_3 = Some(datetime))) :: outputVersions
              }
              val c_id = order.row._1
              var ol_total = 0f
              orderLines.foreach { orderLine =>
                ol_total += orderLine.row._5
              }
              var customerPred = ISharedData.tm.customerTbl.getPred((c_id,d_id,w_id))
              val customer = customerPred.getEntry
              outputVersions = customer.setEntryValue(customer.row.copy(_14 = customer.row._14 + ol_total, _17 = customer.row._17 + 1)) :: outputVersions
            }
            case None => orderIDs(d_id - 1) = 0
          }
          (outputVersions,Nil)
        })
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
      case me: MVCCException => {
        error("An error occurred in handling Delivery transaction for warehouse=%d, carrier=%d".format(w_id,o_carrier_id))
        error(me)
        ISharedData.rollback
        0
      }
      case e: Throwable => {
        logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred in handling Delivery transaction for warehouse=%d, carrier=%d".format(w_id,o_carrier_id))
        logger.error(e.toString)
        if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
        ISharedData.rollback
        0
      }
    }
  }

  object DeliveryTxOps {
    // def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int)(implicit xact:Transaction) = {
    //   ISharedData.findFirstNewOrder(no_w_id_input, no_d_id_input)
    // }

    // def deleteNewOrder(newOrder: DeltaVersion[NewOrderTblKey,NewOrderTblValue])(implicit xact:Transaction) = {
    //   ISharedData.onDelete_NewOrder(newOrder)
    // }

    // def updateOrderCarrierAndFindCID(o_w_id:Int, o_d_id:Int, o_id:Int, updateFunc:DeltaVersion[OrderTblKey,OrderTblValue] => OrderTblValue)(implicit xact:Transaction) = {
    //   ISharedData.onUpdate_Order_byFunc(o_id,o_d_id,o_w_id, updateFunc)
    // }

    // def updateOrderLineDeliveryDateAndFindOrderLineTotalAmount(ol_w_id_input:Int, ol_d_id_input:Int, ol_o_id_input:Int, ol_delivery_d_input:Date)(implicit xact:Transaction) = {
    //   //Modifying the value of elements during traversal might lead to wrong result (e.g. seeing an entry several times)
    //   var orderLines = List[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]()
    //   ISharedData.orderLineTblSliceEntry(0, (ol_o_id_input, ol_d_id_input, ol_w_id_input), { v => 
    //     orderLines = v :: orderLines
    //   })

    //   orderLines.foreach { orderLine =>
    //     val vi = orderLine.row
    //     orderLine.setEntryValue(vi.copy(_3 = Some(ol_delivery_d_input)))
    //   }
    //   orderLines
    // }

    // def updateCustomerBalance(c_w_id:Int, c_d_id:Int, deliveredOrder:DeltaVersion[OrderTblKey,OrderTblValue], orderLines:List[DeltaVersion[OrderLineTblKey,OrderLineTblValue]])(implicit xact:Transaction) = {
    //   val c_id = deliveredOrder.row._1
    //   var ol_total = 0f
    //   orderLines.foreach { orderLine =>
    //     val vi = orderLine.row
    //     ol_total += vi._5
    //   }
    //   ISharedData.onUpdateCustomer_byFunc(c_id,c_d_id,c_w_id, {
    //     customer:DeltaVersion[CustomerTblKey,CustomerTblValue] => customer.row match {
    //       case (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) => 
    //         (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance+ol_total,c_ytd_payment,c_payment_cnt,c_delivery_cnt+1,c_data)
    //     }
    //   })
    // }
  }
}

