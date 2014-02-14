package tpcc.lms

import ddbt.tpcc.itx._
import ddbt.tpcc.loadtest.TpccConstants._
import java.util.Date
import tpcc.lmsgen.EfficientExecutor
import java.sql.Connection
import java.sql.Statement
import java.sql.ResultSet
import ddbt.tpcc.tx._
import tpcc.lmsgen._

class InMemoryLMSTxImpl extends IInMemoryTx {
  var exec:EfficientExecutor = null

  override def setSharedData(db:AnyRef) = {
    exec = db.asInstanceOf[EfficientExecutor]
    this
  }
} 

class NewOrderLMSImpl extends InMemoryLMSTxImpl with INewOrderInMem {
  override def newOrderTx(datetime:Date, t_num: Int, w_id:Int, d_id:Int, c_id:Int, o_ol_count:Int, o_all_local:Int, itemid:Array[Int], supware:Array[Int], quantity:Array[Int], price:Array[Float], iname:Array[String], stock:Array[Int], bg:Array[Char], amt:Array[Float]): Int = {
    exec.newOrderTxInst(SHOW_OUTPUT, datetime,t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stock, bg, amt)
  }
}

class DeliveryLMSImpl extends InMemoryLMSTxImpl with IDeliveryInMem {
  override def deliveryTx(datetime:Date, w_id: Int, o_carrier_id: Int): Int = {
    exec.deliveryTxInst(SHOW_OUTPUT, datetime, w_id, o_carrier_id)
  }
}

class OrderStatusLMSImpl extends InMemoryLMSTxImpl with IOrderStatusInMem {
  override def orderStatusTx(datetime:Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_id: Int, c_last: String):Int = {
    exec.orderStatusTxInst(SHOW_OUTPUT, datetime, t_num, w_id, d_id, c_by_name, c_id, c_last)
  }
}

class PaymentLMSImpl extends InMemoryLMSTxImpl with IPaymentInMem {
  override def paymentTx(datetime:Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_w_id: Int, c_d_id: Int, c_id: Int, c_last: String, h_amount: Float):Int = {
    exec.paymentTxInst(SHOW_OUTPUT, datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last, h_amount)
  }
}

class StockLevelLMSImpl extends InMemoryLMSTxImpl with IStockLevelInMem {
  override def stockLevelTx(t_num: Int, w_id: Int, d_id: Int, threshold: Int):Int = {
    exec.stockLevelTxInst(SHOW_OUTPUT, null, t_num, w_id, d_id, threshold)
  }
}

object LMSDataLoader {
  def loadDataIntoMaps(exec:EfficientExecutor,driverClassName:String, jdbcUrl:String, db_user:String, db_password:String) = {
    val conn = ddbt.tpcc.loadtest.DatabaseConnector.connectToDB(driverClassName,jdbcUrl,db_user,db_password)
    var stmt:Statement = null
    var rs:ResultSet = null
      try {
          stmt = conn.createStatement();

          rs = stmt.executeQuery(TpccSelectQueries.ALL_WAREHOUSES);
          while (rs.next()) {
            exec.x2.insert(SEntry9_ISSSSSSFD(
              rs.getInt("w_id"),
              rs.getString("w_name"),
              rs.getString("w_street_1"),
              rs.getString("w_street_2"),
              rs.getString("w_city"),
              rs.getString("w_state"),
              rs.getString("w_zip"),
              rs.getFloat("w_tax"),
              rs.getDouble("w_ytd")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_CUSTOMERS);
          while (rs.next()) {
            exec.x7.insert(SEntry21_IIISSSSSSSSSASFFFFIIS(
              rs.getInt("c_id"),
              rs.getInt("c_d_id"),
              rs.getInt("c_w_id"),
              rs.getString("c_first"),
              rs.getString("c_middle"),
              rs.getString("c_last"),
              rs.getString("c_street_1"),
              rs.getString("c_street_2"),
              rs.getString("c_city"),
              rs.getString("c_state"),
              rs.getString("c_zip"),
              rs.getString("c_phone"),
              rs.getTimestamp("c_since"),
              rs.getString("c_credit"),
              rs.getFloat("c_credit_lim"),
              rs.getFloat("c_discount"),
              rs.getFloat("c_balance"),
              rs.getFloat("c_ytd_payment"),
              rs.getInt("c_payment_cnt"),
              rs.getInt("c_delivery_cnt"),
              rs.getString("c_data")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_DISTRICTS);
          while (rs.next()) {
            exec.x5.insert(SEntry11_IISSSSSSFDI(
              rs.getInt("d_id"),
              rs.getInt("d_w_id"),
              rs.getString("d_name"),
              rs.getString("d_street_1"),
              rs.getString("d_street_2"),
              rs.getString("d_city"),
              rs.getString("d_state"),
              rs.getString("d_zip"),
              rs.getFloat("d_tax"),
              rs.getDouble("d_ytd"),
              rs.getInt("d_next_o_id")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_HISTORIES);
          while (rs.next()) {
            exec.x1.insert(SEntry8_IIIIIAFS(
              rs.getInt("h_c_id"),
          rs.getInt("h_c_d_id"),
          rs.getInt("h_c_w_id"),
          rs.getInt("h_d_id"),
          rs.getInt("h_w_id"),
          rs.getTimestamp("h_date"),
          rs.getFloat("h_amount"),
          rs.getString("h_data")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_ITEMS);
          while (rs.next()) {
            exec.x3.insert(SEntry5_IISFS(
              rs.getInt("i_id"),
              rs.getInt("i_im_id"),
              rs.getString("i_name"),
              rs.getFloat("i_price"),
              rs.getString("i_data")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_NEW_ORDERS);
          while (rs.next()) {
            exec.x0.insert(SEntry3_III(
              rs.getInt("no_o_id"),
              rs.getInt("no_d_id"),
              rs.getInt("no_w_id")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_ORDER_LINES);
          while (rs.next()) {
            exec.x6.insert(SEntry10_IIIIIIAIFS(
              rs.getInt("ol_o_id"),
              rs.getInt("ol_d_id"),
              rs.getInt("ol_w_id"),
              rs.getInt("ol_number"),
              rs.getInt("ol_i_id"),
              rs.getInt("ol_supply_w_id"),
              rs.getTimestamp("ol_delivery_d"),
              rs.getInt("ol_quantity"),
              rs.getFloat("ol_amount"),
              rs.getString("ol_dist_info")
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_ORDERS);
          while (rs.next()) {
            val o_carrier_id = rs.getInt("o_carrier_id")
            val o_carrier_id_wasNull = rs.wasNull
            val o_all_local = (rs.getInt("o_all_local") > 0)
            exec.x4.insert(SEntry8_IIIIAIIB(
              rs.getInt("o_id"),
              rs.getInt("o_d_id"),
              rs.getInt("o_w_id"),
              rs.getInt("o_c_id"),
              rs.getTimestamp("o_entry_d"),
              if(o_carrier_id_wasNull) (-1) else rs.getInt("o_carrier_id"),
              rs.getInt("o_ol_cnt"),
              o_all_local
            ))
          }
          rs.close

          rs = stmt.executeQuery(TpccSelectQueries.ALL_STOCKS);
          while (rs.next()) {
            exec.x8.insert(SEntry17_IIISSSSSSSSSSIIIS(
              rs.getInt("s_i_id"),
              rs.getInt("s_w_id"),
              rs.getInt("s_quantity"),
              rs.getString("s_dist_01"),
              rs.getString("s_dist_02"),
              rs.getString("s_dist_03"),
              rs.getString("s_dist_04"),
              rs.getString("s_dist_05"),
              rs.getString("s_dist_06"),
              rs.getString("s_dist_07"),
              rs.getString("s_dist_08"),
              rs.getString("s_dist_09"),
              rs.getString("s_dist_10"),
              rs.getInt("s_ytd"),
              rs.getInt("s_order_cnt"),
              rs.getInt("s_remote_cnt"),
              rs.getString("s_data")
            ))
          }
          rs.close

          // println("All Tables loaded...")
          // println("Warehouse => " + warehouseTbl)
      } finally {
          if (stmt != null) { stmt.close(); }
      }
  }

  def getAllMapsInfoStr(exec:EfficientExecutor):String = {
    new StringBuilder("\nTables Info:\nnewOrderTbl => ").append(exec.x0.getInfoStr).append("\n")
    .append("historyTbl => ").append(exec.x1.getInfoStr).append("\n")
    .append("warehouseTbl => ").append(exec.x2.getInfoStr).append("\n")
    .append("itemPartialTbl => ").append(exec.x3.getInfoStr).append("\n")
    .append("orderTbl => ").append(exec.x4.getInfoStr).append("\n")
    .append("districtTbl => ").append(exec.x5.getInfoStr).append("\n")
    .append("orderLineTbl => ").append(exec.x6.getInfoStr).append("\n")
    .append("customerTbl => ").append(exec.x7.getInfoStr).append("\n")
    .append("stockTbl => ").append(exec.x8.getInfoStr).toString
  }

  def moveDataToTpccTable(exec:EfficientExecutor):TpccTable = {
    val res = new TpccTable
    exec.x0.foreach { e=> res.onInsert_NewOrder(e._1,e._2,e._3) }
    exec.x1.foreach { e=> res.onInsert_HistoryTbl(e._1,e._2,e._3,e._4,e._5,e._6,e._7,e._8) }
    exec.x2.foreach { e=> res.onInsert_Warehouse(e._1,e._2,e._3,e._4,e._5,e._6,e._7,e._8,e._9) }
    exec.x3.foreach { e=> res.onInsert_Item(e._1,e._2,e._3,e._4,e._5) }
    exec.x4.foreach { e=> res.onInsert_Order(e._1,e._2,e._3,e._4,e._5,if(e._6 == -1) None else Some(e._6),e._7,e._8) }
    exec.x5.foreach { e => res.onInsert_District(e._1,e._2,e._3,e._4,e._5,e._6,e._7,e._8,e._9,e._10,e._11) }
    exec.x6.foreach { e => res.onInsertOrderLine(e._1,e._2,e._3,e._4,e._5,e._6,if(e._7 == null) None else Some(e._7),e._8,e._9,e._10) }
    exec.x7.foreach { e => res.onInsertCustomer(e._1,e._2,e._3,e._4,e._5,e._6,e._7,e._8,e._9,e._10,e._11,e._12,e._13,e._14,e._15,e._16,e._17,e._18,e._19,e._20,e._21) }
    exec.x8.foreach { e => res.onInsertStock(e._1,e._2,e._3,e._4,e._5,e._6,e._7,e._8,e._9,e._10,e._11,e._12,e._13,e._14,e._15,e._16,e._17) }
    res
  }
}

// class LMSTables extends TpccTable {
//   def loadDataIntoMaps(driverClassName:String, jdbcUrl:String, db_user:String, db_password:String) = {
//   }
//   def getAllMapsInfoStr:String = {

//   }
// }