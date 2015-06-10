package ddbt.tpcc.tx11
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import ddbt.tpcc.tx.MVCCTpccTableV4._
import NewOrder._

object NewOrder {

  private val logger = LoggerFactory.getLogger(classOf[NewOrder])

  private val DEBUG = logger.isDebugEnabled

  private val TRACE = logger.isTraceEnabled

  private val SHOW_OUTPUT = ddbt.tpcc.loadtest.TpccConstants.SHOW_OUTPUT
}

/**
 * NewOrder Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
class NewOrder extends InMemoryTxImplViaMVCCTpccTableV4 with INewOrderInMem {

  /**
   * @param w_id is warehouse id
   * @param d_id is district id
   * @param c_id is customer id
   *
   * Table interactions:
   *   - [Warehouse: R] in
   *      + findCustomerWarehouseFinancialInfo
   *   - [District: RW] in
   *      + updateDistrictNextOrderId
   *   - [Customer: R] in
   *      + findCustomerWarehouseFinancialInfo
   *   - [Order: W] in
   *      + insertOrder
   *   - [NewOrder: W] in
   *      + insertNewOrder
   *   - [Item: R] in
   *      + findItem
   *   - [Stock: RW] in
   *      + updateStock
   *   - [OrderLine: W] in
   *      + insertOrderLine
   *
   */
  override def newOrderTx(datetime:Date, t_num: Int, w_id:Int, d_id:Int, c_id:Int, o_ol_count:Int, o_all_local:Int, itemid:Array[Int], supware:Array[Int], quantity:Array[Int], price:Array[Float], iname:Array[String], stocks:Array[Int], bg:Array[Char], amt:Array[Float]): Int = {
    implicit val xact = ISharedData.begin("neworder")
    try {
      if(SHOW_OUTPUT) logger.info("- Started NewOrder transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))

      val idata = new Array[String](o_ol_count)

      val (customer, warehouse) = NewOrderTxOps.findCustomerWarehouseFinancialInfo(w_id,d_id,c_id)
      val (c_discount, c_last, c_credit, w_tax) = (customer.getImage._13, customer.row._3, customer.row._11, warehouse.row._7)

      var ditsrict: DeltaVersion[DistrictTblKey,DistrictTblValue] = null
      NewOrderTxOps.updateDistrictNextOrderId(w_id,d_id,dv => {
        ditsrict = dv
        val cv = dv.row
        (cv._1,cv._2,cv._3,cv._4,cv._5,cv._6,cv._7,cv._8,cv._9+1)
      })
      val d_tax = ditsrict.row._7
      val o_id = ditsrict.row._9

      //var o_all_local:Boolean = true
      //supware.foreach { s_w_id => if(s_w_id != w_id) o_all_local = false }
      //val o_ol_count = supware.length

      NewOrderTxOps.insertOrder(o_id, w_id, d_id, c_id, datetime, o_ol_count, o_all_local > 0)

      NewOrderTxOps.insertNewOrder(o_id, w_id, d_id)

      var total = 0f
      var failed = false
      var ol_number = 0

      while(ol_number < o_ol_count) {
        try {
          val item = NewOrderTxOps.findItem(itemid(ol_number))
          val (/*_, */i_name, i_price, i_data) = item.row
          price(ol_number) = i_price
          iname(ol_number) = i_name
          idata(ol_number) = i_data

          val ol_supply_w_id = supware(ol_number)
          val ol_i_id = itemid(ol_number)
          val ol_quantity = quantity(ol_number)
          var ol_dist_info = ""
          NewOrderTxOps.updateStock(ol_supply_w_id, ol_i_id, stock => stock.row match {
            case (s_quantity,s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data) =>
              if(idata(ol_number).contains("original") && s_data.contains("original")) {
                bg(ol_number) = 'B'
              } else {
                bg(ol_number) = 'G'
              }

              ol_dist_info = d_id match {
                case 1  => s_dist_01
                case 2  => s_dist_02
                case 3  => s_dist_03
                case 4  => s_dist_04
                case 5  => s_dist_05
                case 6  => s_dist_06
                case 7  => s_dist_07
                case 8  => s_dist_08
                case 9  => s_dist_09
                case 10 => s_dist_10
              }

              stocks(ol_number) = s_quantity

              var new_s_quantity = s_quantity - ol_quantity
              if(s_quantity <= ol_quantity) new_s_quantity += 91

              //TODO this is the correct version but is not implemented in the correctness test
              // var s_remote_cnt_increment = 0
              // if(ol_supply_w_id != w_id) s_remote_cnt_increment = 1
              ((new_s_quantity,s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,
                s_ytd/*+ol_quantity*/,s_order_cnt/*+1*/,s_remote_cnt/*+s_remote_cnt_increment*/, s_data))
          })

          val ol_amount = (ol_quantity * price(ol_number) * (1+w_tax+d_tax) * (1 - c_discount)).asInstanceOf[Float]
          amt(ol_number) =  ol_amount
          total += ol_amount

          NewOrderTxOps.insertOrderLine(w_id, d_id, o_id, ol_number+1/*to start from 1*/, ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_dist_info)

        } catch {
          case nsee: Exception => {
            if(SHOW_OUTPUT) logger.info("An item was not found in handling NewOrder transaction for warehouse=%d, district=%d, customer=%d, items=%s".format(w_id,d_id,c_id, java.util.Arrays.toString(itemid)))
            failed = true
          }
        }
        if(failed) {
          ISharedData.rollback
          return 1
        }
        ol_number += 1
      }

      if(SHOW_OUTPUT) logger.info("- Finished NewOrder transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))

      ISharedData.commit
      1
    } catch {
      case me: MVCCException => {
        error("An error occurred in handling NewOrder transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))
        error(me)
        ISharedData.rollback
        0
      }
      case e: Throwable => {
        logger.error("Thread"+Thread.currentThread().getId()+" :> "+xact+": An error occurred in handling NewOrder transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))
        logger.error(e.toString)
        if(e.getStackTrace.isEmpty) logger.error("Stack Trace is empty!") else e.getStackTrace.foreach(st => logger.error(st.toString))
        ISharedData.rollback
        0
      }
    }
  }

  object NewOrderTxOps {
    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     * @param c_id is customer id
     * @return (c_discount, c_last, c_credit, w_tax)
     */
    def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int)(implicit xact:Transaction) = {
      ISharedData.findCustomerWarehouseFinancialInfo(w_id,d_id,c_id)
    }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     *
     * @param new_d_next_o_id is the next order id
     * @param d_tax is the district tax value for this dirstrict
     */
    def updateDistrictNextOrderId(w_id:Int, d_id:Int, updateFunc:DeltaVersion[DistrictTblKey,DistrictTblValue] => DistrictTblValue)(implicit xact:Transaction): Unit = {
      ISharedData.onUpdate_District_byFunc(d_id, w_id, updateFunc)
    }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    def insertOrder(o_id:Int, w_id:Int, d_id:Int, c_id:Int, o_entry_d:Date, o_ol_cnt:Int, o_all_local:Boolean)(implicit xact:Transaction): Unit = {
      ISharedData.onInsert_Order(o_id,d_id,w_id , c_id, o_entry_d, None, o_ol_cnt, o_all_local)
    }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    def insertNewOrder(o_id:Int, w_id:Int, d_id:Int)(implicit xact:Transaction): Unit = {
      ISharedData.onInsert_NewOrder(o_id,d_id,w_id)
    }

    /**
     * @param item_id is the item id
     */
    def findItem(item_id:Int)(implicit xact:Transaction) = {
      ISharedData.findItem(item_id)
    }

    /**
     * @param w_id is warehouse id
     */
    def updateStock(w_id:Int, item_id:Int, updateFunc:DeltaVersion[StockTblKey,StockTblValue] => StockTblValue)(implicit xact:Transaction) = {
      ISharedData.onUpdateStock_byFunc(item_id,w_id, updateFunc)
    }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    def insertOrderLine(w_id:Int, d_id:Int, o_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
      ISharedData.onInsertOrderLine(o_id, d_id, w_id, ol_number, ol_i_id, ol_supply_w_id, None, ol_quantity, ol_amount, ol_dist_info)
    }
  }
}
