package ddbt.tpcc.mtx

import ddbt.lib.util.ThreadInfo
import ddbt.tpcc.itx._
import java.util.Date

class NewOrderMixedImpl(val newOrderDB: ddbt.tpcc.loadtest.NewOrder, val newOrderMem: ddbt.tpcc.itx.INewOrderInMem) extends INewOrder {
  override def newOrderTx(datetime:Date, t_num: Int, w_id:Int, d_id:Int, c_id:Int, o_ol_count:Int, o_all_local:Int, itemid:Array[Int], supware:Array[Int], quantity:Array[Int], price:Array[Float], iname:Array[String], stock:Array[Int], bg:Array[Char], amt:Array[Float])(implicit tInfo: ThreadInfo): Int = {
    newOrderMem.newOrderTx(datetime,t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stock, bg, amt)
    newOrderDB.newOrderTx(datetime,t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stock, bg, amt)
  }
}

class DeliveryMixedImpl(val deliveryDB: ddbt.tpcc.loadtest.Delivery, val deliveryMem: ddbt.tpcc.itx.IDeliveryInMem) extends IDelivery {
  override def deliveryTx(datetime:Date, w_id: Int, o_carrier_id: Int)(implicit tInfo: ThreadInfo): Int = {
    deliveryMem.deliveryTx(datetime, w_id, o_carrier_id)
    deliveryDB.deliveryTx(datetime, w_id, o_carrier_id)
  }
}

class OrderStatusMixedImpl(val orderStatusDB: ddbt.tpcc.loadtest.OrderStat, val orderStatusMem: ddbt.tpcc.itx.IOrderStatusInMem) extends IOrderStatus {
  override def orderStatusTx(datetime:Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_id: Int, c_last: String)(implicit tInfo: ThreadInfo): Int = {
    orderStatusMem.orderStatusTx(datetime, t_num, w_id, d_id, c_by_name, c_id, c_last)
    orderStatusDB.orderStatusTx(datetime, t_num, w_id, d_id, c_by_name, c_id, c_last)
  }
}

class PaymentMixedImpl(val paymentDB: ddbt.tpcc.loadtest.Payment, val paymentMem: ddbt.tpcc.itx.IPaymentInMem) extends IPayment {
  override def paymentTx(datetime:Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_w_id: Int, c_d_id: Int, c_id: Int, c_last_input: String, h_amount: Float)(implicit tInfo: ThreadInfo): Int = {
    paymentMem.paymentTx(datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last_input, h_amount)
    paymentDB.paymentTx(datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last_input, h_amount)
  }
}

class StockLevelMixedImpl(val stockLevelDB: ddbt.tpcc.loadtest.Slev, val stockLevelMem: ddbt.tpcc.itx.IStockLevelInMem) extends IStockLevel {
  override def stockLevelTx(t_num: Int, w_id: Int, d_id: Int, threshold: Int)(implicit tInfo: ThreadInfo): Int = {
    stockLevelMem.stockLevelTx(t_num, w_id, d_id, threshold)
    stockLevelDB.stockLevelTx(t_num, w_id, d_id, threshold)
  }
}