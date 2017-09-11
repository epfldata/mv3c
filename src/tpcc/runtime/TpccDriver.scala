package ddbt.tpcc.loadtest

import ddbt.lib.util.ThreadInfo
import java.io.PrintWriter
import java.sql.Connection
import java.sql.SQLException
import java.util.Arrays
import java.util.Calendar
import java.util.Date
import java.sql.Timestamp
import java.util.concurrent._
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import ddbt.tpcc.tx.TpccTable
import ddbt.tpcc.itx._
import TpccDriver._
import TpccConstants._

object TpccDriver {

  val logger = LoggerFactory.getLogger(classOf[TpccDriver])

  val DEBUG = logger.isDebugEnabled

  /**
   * Can be disabled for debug use only.
   */
  val ALLOW_MULTI_WAREHOUSE_TX = true
}

class TpccDriver(conn: java.sql.Connection, 
    fetchSize: Int, 
    TRANSACTION_COUNT: Int,
    newOrder: INewOrder,
    payment: IPayment,
    orderStat: IOrderStatus,
    slev: IStockLevel,
    delivery: IDelivery) extends Driver(conn, fetchSize, TRANSACTION_COUNT) {

  override def doNextTransaction(tInfo: ThreadInfo, sequence: Int) {
    if (sequence == 0) {
      doNeword(tInfo)
    } else if (sequence == 1) {
      doPayment(tInfo)
    } else if (sequence == 2) {
      doOrdstat(tInfo)
    } else if (sequence == 3) {
      doDelivery(tInfo)
    } else if (sequence == 4) {
      doSlev(tInfo)
    } else {
      throw new IllegalStateException("Error - Unknown sequence")
    }
  }

  private def doNeword(tInfo: ThreadInfo): Int = {
    val t_num = tInfo.tid
    var c_num = 0
    var w_id = 0
    var d_id = 0
    var c_id = 0
    var ol_cnt = 0
    var all_local = 1
    val notfound = MAXITEMS + 1
    var rbk = 0
    val itemid = Array.ofDim[Int](MAX_NUM_ITEMS)
    val supware = Array.ofDim[Int](MAX_NUM_ITEMS)
    val qty = Array.ofDim[Int](MAX_NUM_ITEMS)
    var price = Array.ofDim[Float](MAX_NUM_ITEMS)
    var amt = Array.ofDim[Float](MAX_NUM_ITEMS)
    var iname = Array.ofDim[String](MAX_NUM_ITEMS)
    var bg = Array.ofDim[Char](MAX_NUM_ITEMS)
    var stock = Array.ofDim[Int](MAX_NUM_ITEMS)

    if (num_node == 0) {
      w_id = Util.randomNumber(1, num_ware)
    } else {
      c_num = ((num_node * t_num) / num_conn)
      w_id = Util.randomNumber(1 + (num_ware * c_num) / num_node, (num_ware * (c_num + 1)) / num_node)
    }
    if (w_id < 1) {
      throw new IllegalStateException("Invalid warehouse ID " + w_id)
    }
    d_id = Util.randomNumber(1, DIST_PER_WARE)
    c_id = Util.nuRand(1023, 1, CUST_PER_DIST)
    ol_cnt = Util.randomNumber(5, 15)
    rbk = Util.randomNumber(1, 100)

    var j = 0
    while (j < ol_cnt) {
      itemid(j) = Util.nuRand(8191, 1, MAXITEMS)
      if ((j == ol_cnt - 1) && (rbk == 1)) {
         itemid(j) = notfound
      }
      if (ALLOW_MULTI_WAREHOUSE_TX) {
        if (Util.randomNumber(1, 100) != 1) {
          supware(j) = w_id
        } else {
          supware(j) = otherWare(w_id)
          if(supware(j) != w_id) all_local = 0
        }
      } else {
        supware(j) = w_id
      }
      qty(j) = Util.randomNumber(1, 10)
      j += 1
    }

    
    val timeout = RTIME_NEWORD
    val xactId = 0
    execTransaction(tInfo, xactId, timeout, Tpcc.counting_on) { implicit tInfo: ThreadInfo =>
      val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
      newOrder.newOrderTx(currentTimeStamp, t_num, w_id, d_id, c_id, ol_cnt, all_local, itemid, supware, qty, price, iname, stock, bg, amt)
    }
    // var i = 0
    // var rt = 0f
    // var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    // while (i < MAX_RETRY) {
    //   if (DEBUG) logger.debug("t_num: " + t_num + " w_id: " + w_id + " c_id: " + c_id + 
    //     " ol_cnt: " + ol_cnt + " all_local: " + all_local + " qty: " + Arrays.toString(qty))

    //   ret = {
    //     val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
    //     newOrder.newOrderTx(currentTimeStamp, t_num, w_id, d_id, c_id, ol_cnt, all_local, itemid, supware, qty, price, iname, stock, bg, amt)
    //   }
    //   endTime = System.currentTimeMillis()
    //   if (ret >= 1) {
    //     rt = (endTime - beginTime).toFloat
    //     if (rt > max_rt(xactId)) max_rt(xactId) = rt
    //     RtHist.histInc(xactId, rt)
    //     if (Tpcc.counting_on) {
    //       if (rt < timeout) {
    //         success(xactId) += 1
    //         success2(xactId)(t_num) += 1
    //       } else {
    //         late(xactId) += 1
    //         late2(xactId)(t_num) += 1
    //       }
    //     }
    //     return (1)
    //   } else {
    //     if (Tpcc.counting_on) {
    //       retry(xactId) += 1
    //       retry2(xactId)(t_num) += 1
    //     }
    //   }
    //   i += 1
    // }
    // if (Tpcc.counting_on) {
    //   retry(xactId) -= 1
    //   retry2(xactId)(t_num) -= 1
    //   failure(xactId) += 1
    //   failure2(xactId)(t_num) += 1
    // }
    // (0)
  }

  private def otherWare(home_ware: Int): Int = {
    var tmp: Int = 0
    if (num_ware == 1) return home_ware
    while ({(tmp = Util.randomNumber(1, num_ware)); tmp} == home_ware) {
      // NOTHING
    }
    tmp
  }

  private def doPayment(tInfo: ThreadInfo): Int = {
    val t_num = tInfo.tid
    var c_num = 0
    var byname = 0
    var w_id = 0
    var d_id = 0
    var c_w_id = 0
    var c_d_id = 0
    var c_id = 0
    var h_amount = 0
    var c_last: String = null
    if (num_node == 0) {
      w_id = Util.randomNumber(1, num_ware)
    } else {
      c_num = ((num_node * t_num) / num_conn)
      w_id = Util.randomNumber(1 + (num_ware * c_num) / num_node, (num_ware * (c_num + 1)) / num_node)
    }
    d_id = Util.randomNumber(1, DIST_PER_WARE)
    c_id = Util.nuRand(1023, 1, CUST_PER_DIST)
    c_last = Util.lastName(Util.nuRand(255, 0, 999))
    h_amount = Util.randomNumber(1, 5000)
    byname = if (Util.randomNumber(1, 100) <= 60) 1 else 0
    if (ALLOW_MULTI_WAREHOUSE_TX) {
      if (Util.randomNumber(1, 100) <= 85) {
        c_w_id = w_id
        c_d_id = d_id
      } else {
        c_w_id = otherWare(w_id)
        c_d_id = Util.randomNumber(1, DIST_PER_WARE)
      }
    } else {
      c_w_id = w_id
      c_d_id = d_id
    }

    

    val timeout = RTIME_PAYMENT
    val xactId = 1
    execTransaction(tInfo, xactId, timeout, Tpcc.counting_on) { implicit tInfo: ThreadInfo =>
      val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
      payment.paymentTx(currentTimeStamp, t_num, w_id, d_id, byname, c_w_id, c_d_id, c_id, c_last, h_amount)
    }
    // var i = 0
    // var rt = 0f
    // var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    // while (i < MAX_RETRY) {
    //   ret = {
    //     val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
    //     payment.paymentTx(currentTimeStamp, t_num, w_id, d_id, byname, c_w_id, c_d_id, c_id, c_last, h_amount)
    //   }
    //   endTime = System.currentTimeMillis()
    //   if (ret >= 1) {
    //     rt = (endTime - beginTime).toFloat
    //     if (rt > max_rt(xactId)) max_rt(xactId) = rt
    //     RtHist.histInc(xactId, rt)
    //     if (Tpcc.counting_on) {
    //       if (rt < timeout) {
    //         success(xactId) += 1
    //         success2(xactId)(t_num) += 1
    //       } else {
    //         late(xactId) += 1
    //         late2(xactId)(t_num) += 1
    //       }
    //     }
    //     return (1)
    //   } else {
    //     if (Tpcc.counting_on) {
    //       retry(xactId) += 1
    //       retry2(xactId)(t_num) += 1
    //     }
    //   }
    //   i += 1
    // }
    // if (Tpcc.counting_on) {
    //   retry(xactId) -= 1
    //   retry2(xactId)(t_num) -= 1
    //   failure(xactId) += 1
    //   failure2(xactId)(t_num) += 1
    // }
    // (0)
  }

  private def doOrdstat(tInfo: ThreadInfo): Int = {
    val t_num = tInfo.tid
    var c_num = 0
    var byname = 0
    var w_id = 0
    var d_id = 0
    var c_id = 0
    var c_last: String = null
    if (num_node == 0) {
      w_id = Util.randomNumber(1, num_ware)
    } else {
      c_num = ((num_node * t_num) / num_conn)
      w_id = Util.randomNumber(1 + (num_ware * c_num) / num_node, (num_ware * (c_num + 1)) / num_node)
    }
    d_id = Util.randomNumber(1, DIST_PER_WARE)
    c_id = Util.nuRand(1023, 1, CUST_PER_DIST)
    c_last = Util.lastName(Util.nuRand(255, 0, 999))
    byname = if (Util.randomNumber(1, 100) <= 60) 1 else 0

    
    val timeout = RTIME_ORDSTAT
    val xactId = 2
    execTransaction(tInfo, xactId, timeout, Tpcc.counting_on) { implicit tInfo: ThreadInfo =>
      val datetime = new java.util.Date({startTime += 1000; startTime})
      orderStat.orderStatusTx(datetime, t_num, w_id, d_id, byname, c_id, c_last)
    }
    // var i = 0
    // var rt = 0f
    // var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    // while (i < MAX_RETRY) {
    //   ret = {
    //     val datetime = new java.util.Date({startTime += 1000; startTime})
    //     orderStat.orderStatusTx(datetime, t_num, w_id, d_id, byname, c_id, c_last)
    //   }
    //   endTime = System.currentTimeMillis()
    //   if (ret >= 1) {
    //     rt = (endTime - beginTime).toFloat
    //     if (rt > max_rt(xactId)) max_rt(xactId) = rt
    //     RtHist.histInc(xactId, rt)
    //     if (Tpcc.counting_on) {
    //       if (rt < timeout) {
    //         success(xactId) += 1
    //         success2(xactId)(t_num) += 1
    //       } else {
    //         late(xactId) += 1
    //         late2(xactId)(t_num) += 1
    //       }
    //     }
    //     return (1)
    //   } else {
    //     if (Tpcc.counting_on) {
    //       retry(xactId) += 1
    //       retry2(xactId)(t_num) += 1
    //     }
    //   }
    //   i += 1
    // }
    // if (Tpcc.counting_on) {
    //   retry(xactId) -= 1
    //   retry2(xactId)(t_num) -= 1
    //   failure(xactId) += 1
    //   failure2(xactId)(t_num) += 1
    // }
    // (0)
  }

  private def doDelivery(tInfo: ThreadInfo): Int = {
    val t_num = tInfo.tid
    var c_num = 0
    var w_id = 0
    var o_carrier_id = 0
    if (num_node == 0) {
      w_id = Util.randomNumber(1, num_ware)
    } else {
      c_num = ((num_node * t_num) / num_conn)
      w_id = Util.randomNumber(1 + (num_ware * c_num) / num_node, (num_ware * (c_num + 1)) / num_node)
    }
    o_carrier_id = Util.randomNumber(1, 10)

    
    val timeout = RTIME_DELIVERY
    val xactId = 3
    execTransaction(tInfo, xactId, timeout, Tpcc.counting_on) { implicit tInfo: ThreadInfo =>
      val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
      delivery.deliveryTx(currentTimeStamp, w_id, o_carrier_id)
    }
    // var i = 0
    // var rt = 0f
    // var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    // while (i < MAX_RETRY) {
    //   ret = {
    //     val currentTimeStamp = new Timestamp({startTime += 1000; startTime})
    //     delivery.deliveryTx(currentTimeStamp, w_id, o_carrier_id)
    //   }
    //   endTime = System.currentTimeMillis()
    //   if (ret >= 1) {
    //     rt = (endTime - beginTime).toFloat
    //     if (rt > max_rt(xactId)) max_rt(xactId) = rt
    //     RtHist.histInc(xactId, rt)
    //     if (Tpcc.counting_on) {
    //       if (rt < timeout) {
    //         success(xactId) += 1
    //         success2(xactId)(t_num) += 1
    //       } else {
    //         late(xactId) += 1
    //         late2(xactId)(t_num) += 1
    //       }
    //     }
    //     return (1)
    //   } else {
    //     if (Tpcc.counting_on) {
    //       retry(xactId) += 1
    //       retry2(xactId)(t_num) += 1
    //     }
    //   }
    //   i += 1
    // }
    // if (Tpcc.counting_on) {
    //   retry(xactId) -= 1
    //   retry2(xactId)(t_num) -= 1
    //   failure(xactId) += 1
    //   failure2(xactId)(t_num) += 1
    // }
    // (0)
  }

  private def doSlev(tInfo: ThreadInfo): Int = {
    val t_num = tInfo.tid
    var c_num = 0
    var w_id = 0
    var d_id = 0
    var level = 0
    if (num_node == 0) {
      w_id = Util.randomNumber(1, num_ware)
    } else {
      c_num = ((num_node * t_num) / num_conn)
      w_id = Util.randomNumber(1 + (num_ware * c_num) / num_node, (num_ware * (c_num + 1)) / num_node)
    }
    d_id = Util.randomNumber(1, DIST_PER_WARE)
    level = Util.randomNumber(10, 20)

    
    val timeout = RTIME_SLEV
    val xactId = 4
    execTransaction(tInfo, xactId, timeout, Tpcc.counting_on) { implicit tInfo: ThreadInfo =>
      slev.stockLevelTx(t_num, w_id, d_id, level)
    }
    // var i = 0
    // var rt = 0f
    // var ret = 0
    // var endTime = 0L
    // var beginTime = System.currentTimeMillis()
    // while (i < MAX_RETRY) {
    //   ret = {
    //     slev.stockLevelTx(t_num, w_id, d_id, level)
    //   }
    //   endTime = System.currentTimeMillis()
    //   if (ret >= 1) {
    //     rt = (endTime - beginTime).toFloat
    //     if (rt > max_rt(xactId)) max_rt(xactId) = rt
    //     RtHist.histInc(xactId, rt)
    //     if (Tpcc.counting_on) {
    //       if (rt < timeout) {
    //         success(xactId) += 1
    //         success2(xactId)(t_num) += 1
    //       } else {
    //         late(xactId) += 1
    //         late2(xactId)(t_num) += 1
    //       }
    //     }
    //     return (1)
    //   } else {
    //     if (Tpcc.counting_on) {
    //       retry(xactId) += 1
    //       retry2(xactId)(t_num) += 1
    //     }
    //   }
    //   i += 1
    // }
    // if (Tpcc.counting_on) {
    //   retry(xactId) -= 1
    //   retry2(xactId)(t_num) -= 1
    //   failure(xactId) += 1
    //   failure2(xactId)(t_num) += 1
    // }
    // (0)
  }

  override def runCommandSeq(commandSeq:Seq[ddbt.lib.util.XactCommand])(implicit tInfo: ThreadInfo) = commandSeq.foreach {
    case TpccTable.DeliveryCommand(datetime, w_id, o_carrier_id) => delivery.deliveryTx(datetime, w_id, o_carrier_id)
    case TpccTable.NewOrderCommand(datetime, t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stocks, bg, amt) => newOrder.newOrderTx(datetime, t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stocks, bg, amt)
    case TpccTable.OrderStatusCommand(datetime, t_num, w_id, d_id, c_by_name, c_id, c_last) => orderStat.orderStatusTx(datetime, t_num, w_id, d_id, c_by_name, c_id, c_last)
    case TpccTable.PaymentCommand(datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last_input, h_amount) => payment.paymentTx(datetime, t_num, w_id, d_id, c_by_name, c_w_id, c_d_id, c_id, c_last_input, h_amount)
    case TpccTable.StockLevelCommand(t_num, w_id, d_id, threshold) => slev.stockLevelTx(t_num, w_id, d_id, threshold)
  }
}
