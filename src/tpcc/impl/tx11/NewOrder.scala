package ddbt.tpcc.tx11
import java.io._
import scala.collection.mutable._
import java.util.Date
import ddbt.tpcc.itx._
import ddbt.tpcc.tx._
import org.slf4j.LoggerFactory
import ddbt.tpcc.tx.MVCCTpccTableV4._
import ddbt.tpcc.tx.TpccTable._
import ddbt.lib.mvc3t._
import TransactionManager._
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
    if(ISharedData.isUnitTestEnabled) xact.setCommand(NewOrderCommand(datetime, t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stocks, bg, amt))
    try {
      if(SHOW_OUTPUT) logger.info("- Started NewOrder transaction for warehouse=%d, district=%d, customer=%d".format(w_id,d_id,c_id))

      val idata = new Array[String](o_ol_count)

      var districtPred = ISharedData.tm.districtTbl.getPred((d_id,w_id))
      val district = districtPred.getEntry
      val d_tax = district.row._7
      val o_id = district.row._9


      // district.setEntryValue(district.row.copy(_9 = district.row._9+1)/*, new ClosureTransition(List(districtPred), {
      //   val newDistrict = districtPred.getEntry
      //   newDistrict.setEntryValue(newDistrict.row.copy(_9 = newDistrict.row._9+1))
      // })*/)
      var failed = false

      //TODO: caches should be invalidated, before the returning function of onChange is called
      new ClosureTransition(List(districtPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
        val newDistrict = district.getCurrentValue
        val o_id = newDistrict.row._9
        List(newDistrict.setEntryValue(newDistrict.row.copy(_9 = o_id+1)))
      }), () => {
        var ctDeps = List[ClosureTransition]()

        val updatedDistrict = district.setEntryValue(district.row.copy(_9 = o_id+1))

        //var o_all_local:Boolean = true
        //supware.foreach { s_w_id => if(s_w_id != w_id) o_all_local = false }
        //val o_ol_count = supware.length

        ctDeps = new ClosureTransition(List(districtPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
          val newDistrict = district.getCurrentValue
          val o_id = newDistrict.row._9 - 1
          outputVersions(0).remove
          List(ISharedData.tm.orderTbl += ((o_id,d_id,w_id), outputVersions(0).row.asInstanceOf[OrderTblValue]))
        }), () => {
          (List(ISharedData.tm.orderTbl += ((o_id,d_id,w_id), (c_id,datetime,None,o_ol_count,o_all_local > 0))), Nil)
        }) :: ctDeps

        ctDeps = new ClosureTransition(List(districtPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
          val newDistrict = district.getCurrentValue
          val o_id = newDistrict.row._9 - 1
          outputVersions(0).remove
          List(ISharedData.tm.newOrderTbl += ((o_id, d_id, w_id), TRUE_TUPLE))
        }), () => {
          (List(ISharedData.tm.newOrderTbl += ((o_id, d_id, w_id), TRUE_TUPLE)), Nil)
        }) :: ctDeps


        var total = 0f
        var ol_number_var = 0

        while((ol_number_var < o_ol_count) && !failed) {
          {
            val ol_number = ol_number_var
            val ol_supply_w_id = supware(ol_number)
            val ol_i_id = itemid(ol_number)
            val ol_quantity = quantity(ol_number)

            try {
              val itemPred = ISharedData.tm.itemPartialTbl.getPred(itemid(ol_number))
              //TODO we should handle output variables beside handling internal objects
              // new ClosureTransition(List(itemPred), () => {
                val item = itemPred.getEntry

                val (/*_, */i_name, i_price, _) = item.row
                price(ol_number) = i_price
                iname(ol_number) = i_name
              //   List()
              // })


              var stockPred = ISharedData.tm.stockTbl.getPred((ol_i_id,ol_supply_w_id))
              val stock = stockPred.getEntry
              new ClosureTransition(List(itemPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                Nil
              }, stockPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                val oldStock = outputVersions(0).asInstanceOf[DeltaVersion[StockTblKey,StockTblValue]]
                val currentStock = oldStock.entry.getTheValue

                val s_quantity = currentStock.row._1

                var new_s_quantity = s_quantity - ol_quantity
                if(s_quantity <= ol_quantity) new_s_quantity += 91

                List(currentStock.setEntryValue(stock.row.copy(_1 = new_s_quantity)))
              }), () => {
                val item = itemPred.getEntry
                val (/*_, */_, _, i_data) = item.row
                idata(ol_number) = i_data

                val updatedStock = stock.setEntryValue(stock.row match {
                  case (s_quantity,s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data) =>
                    if(i_data.contains("original") && s_data.contains("original")) {
                      bg(ol_number) = 'B'
                    } else {
                      bg(ol_number) = 'G'
                    }

                    stocks(ol_number) = s_quantity

                    var new_s_quantity = s_quantity - ol_quantity
                    if(s_quantity <= ol_quantity) new_s_quantity += 91

                    //TODO this is the correct version but is not implemented in the correctness test
                    // var s_remote_cnt_increment = 0
                    // if(ol_supply_w_id != w_id) s_remote_cnt_increment = 1
                    stock.row.copy(_1 = new_s_quantity/*, _12 = stock.row._12/*+ol_quantity*/,  _13 = stock.row._13/*+1*/,  _14 = stock.row._14/*+s_remote_cnt_increment*/*/)
                })
                (List(updatedStock),Nil)
              })


              val warehousePred = ISharedData.tm.warehouseTbl.getPred(w_id)
              val warehouse = warehousePred.getEntry
              val w_tax = warehouse.row._7

              val customerPred = ISharedData.tm.customerTbl.getPred((c_id,d_id,w_id))
              val customer = customerPred.getEntry
              val c_discount = customer.row._13

              val (_,s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,_,_,_,_) = stock.row

              val ol_dist_info = d_id match {
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

              val ol_amount = (ol_quantity * price(ol_number) * (1+w_tax+d_tax) * (1 - c_discount)).asInstanceOf[Float]


              ctDeps = new ClosureTransition(List(districtPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                val newDistrict = district.getCurrentValue
                val o_id = newDistrict.row._9 - 1
                val insertedOrderLine = outputVersions(0).asInstanceOf[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]
                if(insertedOrderLine.getKey._1 != o_id) { //o_id changed?
                  insertedOrderLine.remove
                  List(ISharedData.tm.orderLineTbl += ((o_id, d_id, w_id, ol_number+1/*to start from 1*/), (ol_i_id, ol_supply_w_id, None, ol_quantity, ol_amount, ol_dist_info)))
                } else { //d_tax changed?
                  val d_tax = newDistrict.row._7
                  val ol_amount = (ol_quantity * price(ol_number) * (1+w_tax+d_tax) * (1 - c_discount)).asInstanceOf[Float]
                  if(ol_amount != insertedOrderLine.row._5) {
                    // System.err.println("districtPred.onChange called for <" + insertedOrderLine.getKey + ", " + insertedOrderLine.row + "> and newAmount = " + ol_amount)
                    List(insertedOrderLine.setEntryValue(insertedOrderLine.row.copy(_5 = ol_amount)))
                  }
                  Nil
                }
              }, warehousePred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                val insertedOrderLine = outputVersions(0).asInstanceOf[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]
                val w_tax = warehouse.getCurrentValue.row._7
                val ol_amount = (ol_quantity * price(ol_number) * (1+w_tax+d_tax) * (1 - c_discount)).asInstanceOf[Float]
                if(ol_amount != insertedOrderLine.row._5) {
                  // System.err.println("warehousePred.onChange called for <" + insertedOrderLine.getKey + ", " + insertedOrderLine.row + "> and newAmount = " + ol_amount + ", ol_quantity = %s, ol_number = %d, price(ol_number) = %s, w_tax = %s, d_tax = %s, c_discount = %s, all prices = %s".format(ol_quantity, ol_number, price(ol_number), w_tax, d_tax, c_discount, price.mkString("{",",","}")))
                  List(insertedOrderLine.setEntryValue(insertedOrderLine.row.copy(_5 = ol_amount)))
                }
                Nil
              }, stockPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                val insertedOrderLine = outputVersions(0).asInstanceOf[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]
                val (_,new_s_dist_01,new_s_dist_02,new_s_dist_03,new_s_dist_04,new_s_dist_05,new_s_dist_06,new_s_dist_07,new_s_dist_08,new_s_dist_09,new_s_dist_10,_,_,_,_) = stock.getCurrentValue.row

                val new_ol_dist_info = d_id match {
                  case 1  => new_s_dist_01
                  case 2  => new_s_dist_02
                  case 3  => new_s_dist_03
                  case 4  => new_s_dist_04
                  case 5  => new_s_dist_05
                  case 6  => new_s_dist_06
                  case 7  => new_s_dist_07
                  case 8  => new_s_dist_08
                  case 9  => new_s_dist_09
                  case 10 => new_s_dist_10
                }
                if(new_ol_dist_info != insertedOrderLine.row._6) {
                  List(insertedOrderLine.setEntryValue(insertedOrderLine.row.copy(_6 = new_ol_dist_info)))
                }
                Nil
              }, customerPred.onChange{ outputVersions:List[DeltaVersion[_,_]] =>
                val insertedOrderLine = outputVersions(0).asInstanceOf[DeltaVersion[OrderLineTblKey,OrderLineTblValue]]
                val c_discount = customer.getCurrentValue.row._13
                val ol_amount = (ol_quantity * price(ol_number) * (1+w_tax+d_tax) * (1 - c_discount)).asInstanceOf[Float]
                if(ol_amount != insertedOrderLine.row._5) {
                  // System.err.println("customerPred.onChange called for <" + insertedOrderLine.getKey + ", " + insertedOrderLine.row + "> and newAmount = " + ol_amount)
                  List(insertedOrderLine.setEntryValue(insertedOrderLine.row.copy(_5 = ol_amount)))
                }
                Nil
              }), () => {

                amt(ol_number) =  ol_amount
                total += ol_amount
                // System.err.println("Main code called for <" + ((o_id, d_id, w_id, ol_number+1/*to start from 1*/), (ol_i_id, ol_supply_w_id, None, ol_quantity, ol_amount, ol_dist_info)) + "> ")
                (List(ISharedData.tm.orderLineTbl += ((o_id, d_id, w_id, ol_number+1/*to start from 1*/), (ol_i_id, ol_supply_w_id, None, ol_quantity, ol_amount, ol_dist_info))), Nil)
              }) :: ctDeps

            } catch {
              case nsee: Exception => {
                if(SHOW_OUTPUT) logger.info("An item was not found in handling NewOrder transaction for warehouse=%d, district=%d, customer=%d, items=%s".format(w_id,d_id,c_id, java.util.Arrays.toString(itemid)))
                failed = true
              }
            }
          }
          ol_number_var += 1
        }
        (List(updatedDistrict),ctDeps)
      })

      if(failed) {
        xact.tm.forcedRollback += 1
        ISharedData.rollback
        return 1
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
    // def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int)(implicit xact:Transaction) = {
    //   ISharedData.findCustomerWarehouseFinancialInfo(w_id,d_id,c_id)
    // }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     *
     * @param new_d_next_o_id is the next order id
     * @param d_tax is the district tax value for this dirstrict
     */
    // def updateDistrictNextOrderId(w_id:Int, d_id:Int, updateFunc:DeltaVersion[DistrictTblKey,DistrictTblValue] => DistrictTblValue)(implicit xact:Transaction): Unit = {
    //   ISharedData.onUpdate_District_byFunc(d_id, w_id, updateFunc)
    // }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    // def insertOrder(o_id:Int, w_id:Int, d_id:Int, c_id:Int, o_entry_d:Date, o_ol_cnt:Int, o_all_local:Boolean)(implicit xact:Transaction): Unit = {
    //   ISharedData.onInsert_Order(o_id,d_id,w_id , c_id, o_entry_d, None, o_ol_cnt, o_all_local)
    // }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    // def insertNewOrder(o_id:Int, w_id:Int, d_id:Int)(implicit xact:Transaction): Unit = {
    //   ISharedData.onInsert_NewOrder(o_id,d_id,w_id)
    // }

    /**
     * @param item_id is the item id
     */
    // def findItem(item_id:Int)(implicit xact:Transaction) = {
    //   ISharedData.findItem(item_id)
    // }

    /**
     * @param w_id is warehouse id
     */
    // def updateStock(w_id:Int, item_id:Int, updateFunc:DeltaVersion[StockTblKey,StockTblValue] => StockTblValue)(implicit xact:Transaction) = {
    //   ISharedData.onUpdateStock_byFunc(item_id,w_id, updateFunc)
    // }

    /**
     * @param w_id is warehouse id
     * @param d_id is district id
     */
    // def insertOrderLine(w_id:Int, d_id:Int, o_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
    //   ISharedData.onInsertOrderLine(o_id, d_id, w_id, ol_number, ol_i_id, ol_supply_w_id, None, ol_quantity, ol_amount, ol_dist_info)
    // }
  }
}
