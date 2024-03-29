package ddbt.tpcc.tx10

import ddbt.lib.util.ThreadInfo
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import ddbt.tpcc.tx.TpccTable._
import ddbt.tpcc.tx.MVCCTpccTableV3._
import ddbt.lib.mvconcurrent._
import TransactionManager._
import Payment._

object Payment {

  private val logger = LoggerFactory.getLogger(classOf[Payment])

  private val DEBUG = logger.isDebugEnabled

  private val TRACE = logger.isTraceEnabled

  private val SHOW_OUTPUT = ddbt.tpcc.loadtest.TpccConstants.SHOW_OUTPUT
}

/**
 * Payment Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
class Payment extends InMemoryTxImplViaMVCCTpccTableV3 with IPaymentInMem {

  /**
   * @param w_id is warehouse id
   * @param d_id is district id
   * @param c_id is customer id
   *
   * Table interactions:
   *   - [Warehouse: RW] in
   *      + updateWarehouse
   *   - [District: RW] in
   *      + updateDistrict
   *   - [Customer: RW] in
   *      + findCustomerById
   *      + findCustomerData
   *   - [History: W] in
   *      + insertHistory
   *
   */
  override def paymentTx(datetime:Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_w_id: Int, c_d_id: Int, c_id: Int, c_last_input: String, h_amount: Float)(implicit tInfo: ThreadInfo): Int = {
    implicit val xact = ISharedData.begin("payment")
    if(ISharedData.isUnitTestEnabled) xact.setCommand(PaymentCommand(datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last_input, h_amount))
    try {
      PaymentTxOps.updateWarehouse(w_id, { case (w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd) => {
        PaymentTxOps.updateDistrict(w_id,d_id, { case (d_name,d_street_1,d_street_2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id) => { 
          var c: DeltaVersion[(Int,Int,Int),(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)] = null
          if (c_by_name > 0) {
            c = ISharedData.findCustomerEntryByName(c_w_id, c_d_id, c_last_input)
          } else {
            c = ISharedData.findCustomerEntryById(c_w_id, c_d_id, c_id)
          }
          val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,found_c_data) = c.getImage
          val found_c_id = c.entry.key._1
          var c_data:String = found_c_data

          if (c_credit.contains("BC")) {
            c_data = "%d %d %d %d %d $%f %s | %s".format(found_c_id, c_d_id, c_w_id, d_id, w_id, 
                h_amount, datetime.toString, c_data)
            if (c_data.length > 500) c_data = c_data.substring(0, 500)
            //TODO this is the correct version but is not implemented in the correctness test
            ISharedData.onUpdateCustomer_byEntry(c, c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance+h_amount,c_ytd_payment/*+h_amount*/,c_payment_cnt/*+1*/,c_delivery_cnt,c_data)
          } else {
            //TODO this is the correct version but is not implemented in the correctness test
            ISharedData.onUpdateCustomer_byEntry(c, c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance+h_amount,c_ytd_payment/*+h_amount*/,c_payment_cnt/*+1*/,c_delivery_cnt,found_c_data)
          }
          //TODO this is the correct version but is not implemented in the correctness test
          val h_data: String = {if (w_name.length > 10) w_name.substring(0, 10) else w_name} + "    " + {if (d_name.length > 10) d_name.substring(0, 10) else d_name}
          PaymentTxOps.insertHistory(found_c_id,c_d_id,c_w_id,d_id,w_id,datetime,h_amount,h_data)

          if(SHOW_OUTPUT) {
            val output: StringBuilder = new StringBuilder
            output.append("\n+---------------------------- PAYMENT ----------------------------+")
            .append("\n Date: " + datetime)
            .append("\n\n Warehouse: ").append(w_id)
            .append("\n   Street:  ").append(w_street_1)
            .append("\n   Street:  ").append(w_street_2)
            .append("\n   City:    ").append(w_city)
            .append("   State: ").append(w_state)
            .append("  Zip: ").append(w_zip)
            .append("\n\n District:  ").append(d_id)
            .append("\n   Street:  ").append(d_street_1)
            .append("\n   Street:  ").append(d_street_2)
            .append("\n   City:    ").append(d_city)
            .append("   State: ").append(d_state)
            .append("  Zip: ").append(d_zip)
            .append("\n\n Customer:  ").append(c_id)
            .append("\n   Name:    ").append(c_first).append(" ").append(c_middle).append(" ").append(c_last)
            .append("\n   Street:  ").append(c_street_1)
            .append("\n   Street:  ").append(c_street_2)
            .append("\n   City:    ").append(c_city)
            .append("   State: ").append(c_state)
            .append("  Zip: ").append(c_zip)
            .append("\n   Since:   ")
            if (c_since != null) {
              output.append(c_since)
            }
            else {
              output.append("")
            }
            output.append("\n   Credit:  ").append(c_credit)
            .append("\n   Disc:    ").append(c_discount * 100).append("%")
            .append("\n   Phone:   ").append(c_phone)
            .append("\n\n Amount Paid:      ").append(h_amount)
            .append("\n Credit Limit:     ").append(c_credit_lim)
            .append("\n New Cust-Balance: ").append(c_balance)
            if (c_credit == "BC") {
              if (c_data.length > 50) {
                output.append("\n\n Cust-Data: ").append(c_data.substring(0, 50))
                val data_chunks: Int = (if (c_data.length > 200) 4 else c_data.length / 50)
                var n: Int = 1
                while (n < data_chunks) {
                  output.append("\n            ").append(c_data.substring(n * 50, (n + 1) * 50))
                  n += 1
                }
              } else {
                output.append("\n\n Cust-Data: " + c_data)
              }
            }
            output.append("\n+-----------------------------------------------------------------+\n\n")
            logger.info(output.toString)
          }
          (d_name,d_street_1,d_street_2,d_city,d_state,d_zip,d_tax,d_ytd+h_amount,d_next_o_id)
        }})
        (w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd+h_amount)
      }})

      ISharedData.commit
      1
    } catch {
      case me: MVCCException => {
        error("An error occurred in handling Payment transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))
        error(me)
        ISharedData.rollback
        0
      }
      case e: Throwable => {
        logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred in handling Payment transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))
        logger.error(e.toString)
        if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
        ISharedData.rollback
        0
      }
    }
  }
  object PaymentTxOps {
    def updateWarehouse(w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double)) => (String, String, String, String, String, String, Float, Double))(implicit xact:Transaction) = {
      ISharedData.onUpdate_Warehouse_byFunc(w_id,updateFunc)
    }
    def updateDistrict(w_id:Int, d_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double, Int)) => (String, String, String, String, String, String, Float, Double, Int))(implicit xact:Transaction) = {
      ISharedData.onUpdate_District_byFunc(d_id,w_id, updateFunc)
    }
    def insertHistory(h_c_id:Int,h_c_d_id:Int,h_c_w_id:Int,h_d_id:Int,h_w_id:Int,h_date:Date,h_amount:Float,h_data:String)(implicit xact:Transaction) = {
      ISharedData.onInsert_HistoryTbl(h_c_id,h_c_d_id,h_c_w_id,h_d_id,h_w_id,h_date,h_amount,h_data)
    }
  }
}

