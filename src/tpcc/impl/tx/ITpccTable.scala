package ddbt.tpcc.tx
import java.io._
import scala.collection.mutable._
import java.util.Date
import java.sql.Connection
import java.sql.Statement
import java.sql.ResultSet
import ddbt.tpcc.loadtest.Util._
import ddbt.tpcc.loadtest.DatabaseConnector._
import ddbt.lib.shm.SHMap
import ddbt.lib.shm.SEntry
import ddbt.lib.shm.SHMapPooled
import ddbt.lib.BinaryHeap
import ddbt.tpcc.loadtest.TpccConstants._

import TpccTable._

object ITpccTable {
	def testSpecialDsUsed = true
}
/**
 * Tables for TPC-C Benchmark (with all operations reflected through its API
 * (and not directly via internal tables)
 *
 * @author Mohammad Dashti
 */
class ITpccTable extends TpccTable(5) {
	override def testSpecialDsUsed = ITpccTable.testSpecialDsUsed

	override def onInsert_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int) = {
		if(testSpecialDsUsed) {
			newOrderSetImpl((no_d_id, no_w_id)).add(no_o_id)
		} else {
			newOrderTbl((no_o_id, no_d_id, no_w_id)) = (true)
		}
	}

	override def onDelete_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int) = {
		if(testSpecialDsUsed) {
			if(newOrderSetImpl((no_d_id, no_w_id)).remove != no_o_id) {
				throw new RuntimeException("Some operations executed out of order => newOrderSetImpl((%d,%d)).remove != %d".format(no_d_id, no_w_id, no_o_id))
			}
		} else {
			newOrderTbl -= ((no_o_id, no_d_id, no_w_id))
		}
	}

    /*Func*/ def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int):Option[Int] = {
      val noSet = newOrderSetImpl(no_d_id_input, no_w_id_input)
      if(noSet.isEmpty) None
      else Some(noSet.peek)
    }

	override def onInsert_HistoryTbl(h_c_id:Int, h_c_d_id:Int, h_c_w_id:Int, h_d_id:Int, h_w_id:Int, h_date:Date, h_amount:Float, h_data:String) = {
		historyTbl += ((h_c_id,h_c_d_id,h_c_w_id,h_d_id,h_w_id,roundDate(h_date),h_amount,h_data), (true))
	}

	override def onInsert_Item(i_id:Int, i_im_id:Int, i_name:String, i_price:Float, i_data:String) = {
		if(testSpecialDsUsed) {
			itemPartialArr(i_id) = ((/*i_im_id,*/i_name,i_price,i_data))
		} else {
			itemPartialTbl(i_id) = ((/*i_im_id,*/i_name,i_price,i_data))
		}
	}

	/*Func*/ def findItem(item_id:Int) = {
		itemPartialArr(item_id)
	}

	override def onInsert_Order(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int, o_entry_d:Date, o_carrier_id:Option[Int], o_ol_cnt:Int, o_all_local:Boolean) = {
		orderTbl += ((o_id,o_d_id,o_w_id), (o_c_id,o_entry_d,o_carrier_id,o_ol_cnt,o_all_local))
		if(testSpecialDsUsed) {
			//println("orderMaxOrderSetImpl((%s, %s,%s)).add(%s)".format(o_d_id, o_w_id,o_c_id,o_id))
			orderMaxOrderSetImpl((o_d_id, o_w_id,o_c_id)).add(o_id)
		}
	}

	/*Func*/ def findMaxOrder(o_w_id_arg:Int, o_d_id_arg:Int, c_id_arg:Int) = {
		val oSet = orderMaxOrderSetImpl(o_d_id_arg,o_w_id_arg, c_id_arg)
		if(oSet.isEmpty) -1
        else oSet.peek
	}

	/*Func*/ def findOrder(max_o_id:Int, o_w_id_arg:Int, o_d_id_arg:Int) = {
		orderTbl((max_o_id,o_d_id_arg,o_w_id_arg))
	}

	override def onUpdate_Order_forDelivery(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int/*, o_entry_d:Date*/, o_carrier_id:Option[Int]/*, o_ol_cnt:Int, o_all_local:Boolean*/) = {
		orderTbl.update((o_id,o_d_id,o_w_id),(currentVal/*:(Int, java.util.Date, Option[Int], Int, Boolean))*/ => ((o_c_id,currentVal._2,o_carrier_id,currentVal._4,currentVal._5))))
	}

	override def onUpdate_Order_byFunc(o_id:Int, o_d_id:Int, o_w_id:Int, updateFunc:((Int, Date, Option[Int], Int, Boolean)) => (Int, Date, Option[Int], Int, Boolean)) = {
		orderTbl.update((o_id,o_d_id,o_w_id),updateFunc)
	}

	override def onInsert_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double) = {
		if(testSpecialDsUsed) {
			warehouseArr(w_id) = ((w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
		} else {
			warehouseTbl(w_id) = ((w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
		}
	}

	override def onUpdate_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double) = {
		onInsert_Warehouse(w_id,w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd)
	}

	override def onUpdate_Warehouse_byFunc(w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double)) => (String, String, String, String, String, String, Float, Double)) = {
		if(testSpecialDsUsed) {
			warehouseArr(w_id) = updateFunc(warehouseArr(w_id))
		} else {
			warehouseTbl.update(w_id,updateFunc)
		}
	}

	override def onInsert_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int) = {
		if(testSpecialDsUsed) {
			districtArr((d_id+(d_w_id * DISTRICTS_UNDER_A_WAREHOUSE))) = ((d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
		} else {
			districtTbl((d_id,d_w_id)) = ((d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
		}
	}

	override def onUpdate_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int) = {
		onInsert_District(d_id,d_w_id, d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id)
	}

	override def onUpdate_District_forNewOrder(d_id:Int, d_w_id:Int/*, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String*/, d_tax:Float/*, d_ytd:Float*/, d_next_o_id:Int) = {
		val (d_name,d_street1,d_street2,d_city,d_state,d_zip,_,d_ytd,_) = districtTbl(d_id,d_w_id)
		onUpdate_District(d_id,d_w_id, d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id)
	}

	override def onUpdate_District_byFunc(d_id:Int, d_w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double, Int)) => (String, String, String, String, String, String, Float, Double, Int)) = {
		if(testSpecialDsUsed) {
			val index = (d_id+(d_w_id * DISTRICTS_UNDER_A_WAREHOUSE))
			districtArr(index) = updateFunc(districtArr(index))
		} else {
			districtTbl.update((d_id,d_w_id), updateFunc)
		}
	}

	/*Func*/ def findDistrict(w_id:Int, d_id:Int) = {
		districtArr(d_id+(w_id * DISTRICTS_UNDER_A_WAREHOUSE))
	}

	override def onInsertOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String): Unit = {
      orderLineTbl += ((ol_o_id, ol_d_id, ol_w_id, ol_number), (ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

	override def onUpdateOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String): Unit = {
      orderLineTbl.update((ol_o_id, ol_d_id, ol_w_id, ol_number) , (ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

    /*Func*/ def orderLineTblSlice[P](part:Int, partKey:P, f: (((Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String))) => Unit) = {
		orderLineTbl.slice(0, partKey).foreach(f)
    }
    /*Func*/ def orderLineTblSliceEntry[P](part:Int, partKey:P, f: java.util.Map.Entry[SEntry[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)], Boolean] => Unit) = {
		orderLineTbl.slice(0, partKey).foreachEntry(f)
    }

    override def onInsertCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String) = {
      customerTbl += ((c_id,c_d_id,c_w_id), (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
      var w_tax = 0f
      if(testSpecialDsUsed) {
      	w_tax = warehouseArr(c_w_id)._7
      } else {
      	w_tax = warehouseTbl(c_w_id)._7
      }
      customerWarehouseFinancialInfoMap += ((c_id,c_d_id,c_w_id), (c_discount, c_last, c_credit, w_tax))

      if(testSpecialDsUsed) {
      	for(w <- 1 to NUM_WAREHOUSES) {
	        for(i <- 1 to DISTRICTS_UNDER_A_WAREHOUSE) {
	          orderMaxOrderSetImpl += ((i,w,c_id), new BinaryHeap[Int](true))
	        }
	    }
      }
    }

    /*Func*/ def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int) = {
      customerWarehouseFinancialInfoMap(c_id,d_id,w_id)
    }

    override def onUpdateCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String) = {
      customerTbl.update((c_id,c_d_id,c_w_id),(c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
    }

    override def onUpdateCustomer_byFunc(c_id: Int, c_d_id: Int, c_w_id: Int, updateFunc:((String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String)) => (String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String)) = {
      customerTbl.update((c_id,c_d_id,c_w_id),updateFunc)
    }

    override def onInsertStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String) = {
      stockTbl += ((s_i_id,s_w_id), (s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    override def onUpdateStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String) = {
      stockTbl.update((s_i_id,s_w_id), (s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    override def onUpdateStock_byFunc(s_i_id:Int, s_w_id:Int, updateFunc:((Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String)) => (Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String)) = {
      stockTbl.update((s_i_id,s_w_id), updateFunc)
    }

	/*Func*/ def findStock(item_id:Int, w_id:Int) = {
		stockTbl(item_id,w_id)
	}

	private class MiniCustomer(val cust_id:Int, val cust_first:String) extends Ordered[MiniCustomer] {
		def compare(that: MiniCustomer) = this.cust_first.compareToIgnoreCase(that.cust_first)
		override def toString = "MiniCustomer(%s,%s)".format(cust_id, cust_first)
	} 

    override def findCustomerEntryByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String) = {
      var customers = new ArrayBuffer[MiniCustomer]
      //we should slice over input_c_last
      customerTbl.slice(0, (input_c_d_id, input_c_w_id, input_c_last)).foreach { case ((c_id,_,_) , (c_first,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)) =>
          customers += new MiniCustomer(c_id,c_first)
      }
      if (customers.size == 0) {
        throw new RuntimeException("The customer C_LAST=" + input_c_last + " C_D_ID=" + input_c_d_id + " C_W_ID=" + input_c_w_id + " not found!")
      }
      // println("**********************************")
      // println("Customers before:",customers)
      customers = customers.sorted
      // println("Customers after:",customers)
      // println("**********************************")
      var index: Int = customers.size / 2
      if (customers.size % 2 == 0) {
        index -= 1
      }
      val c_id = customers(index).cust_id
      customerTbl.getEntry((c_id,input_c_d_id,input_c_w_id))
    }
    override def findCustomerEntryById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int) = {
      customerTbl.getEntry((c_id,input_c_d_id,input_c_w_id))
    }

    override def findCustomerByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String) = {
      var customers = new ArrayBuffer[MiniCustomer]
      //we should slice over input_c_last
      customerTbl.slice(0, (input_c_d_id, input_c_w_id, input_c_last)).foreach { case ((c_id,_,_) , (c_first,_,c_last,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)) =>
        customers += new MiniCustomer(c_id,c_first)
      }
      if (customers.size == 0) {
        throw new RuntimeException("The customer C_LAST=" + input_c_last + " C_D_ID=" + input_c_d_id + " C_W_ID=" + input_c_w_id + " not found!")
      }
      // println("**********************************")
      // println("Customers before:",customers)
      customers = customers.sorted
      // println("Customers after:",customers)
      // println("**********************************")
      var index: Int = customers.size / 2
      if (customers.size % 2 == 0) {
        index -= 1
      }
      val c_id = customers(index).cust_id
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = customerTbl((c_id,input_c_d_id,input_c_w_id))
      (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data,c_id)
    }
    override def findCustomerById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int) = {
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = customerTbl((c_id,input_c_d_id,input_c_w_id))
      (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data,c_id)
    }
}
