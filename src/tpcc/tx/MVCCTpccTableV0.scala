package ddbt.tpcc.tx
import java.io._
import scala.collection.mutable._
import java.util.Date
import java.sql.Connection
import java.sql.Statement
import java.sql.ResultSet
import ddbt.tpcc.loadtest.Util._
import ddbt.tpcc.loadtest.DatabaseConnector._
import ddbt.tpcc.lib.shm.SHMap
import ddbt.tpcc.lib.shm.SEntry
import ddbt.tpcc.lib.shm.SHMapPooled
import ddbt.tpcc.lib.BinaryHeap
import ddbt.tpcc.loadtest.TpccConstants._

import TpccTable._
import MVCCTpccTableV0._

object MVCCTpccTableV0 {
	def testSpecialDsUsed = false

	class Transaction( val xactId:Int ) {
	}

	class TransactionManager {
		var maxXact = 0
		def begin = {
			maxXact += 1
			new Transaction(maxXact)
		}
		def commit(implicit xact:Transaction) = {

		}
		def rollback(implicit xact:Transaction) = {

		}
	}
}
/**
 * Tables for TPC-C Benchmark (with all operations reflected through its API
 * (and not directly via internal tables)
 *
 * @author Mohammad Dashti
 */
class MVCCTpccTableV0 extends TpccTable(7) {
	val tm = new TransactionManager

	def begin = tm.begin
	def commit(implicit xact:Transaction) = tm.commit
	def rollback(implicit xact:Transaction) = tm.rollback

	override def testSpecialDsUsed = MVCCTpccTableV0.testSpecialDsUsed

	override val newOrderTbl = new SHMap[(Int,Int,Int),Boolean](0.9f, 262144, (k:(Int,Int,Int),v:Boolean) => ((k._2, k._3)) )

	override val historyTbl = new SHMap[(Int,Int,Int,Int,Int,Date,Float,String),Boolean]/*(0.9f, 4194304)*/

	override val warehouseTbl = new SHMap[Int,(String,String,String,String,String,String,Float,Double)]

	override val itemPartialTbl = new SHMap[Int,(/*Int,*/String,Float,String)]/*(1f, 262144)*/

	override val orderTbl = new SHMap[(Int,Int,Int),(Int,Date,Option[Int],Int,Boolean)](/*0.9f, 4194304,*/ (k:(Int,Int,Int), v:(Int,Date,Option[Int],Int,Boolean)) => ((k._2, k._3, v._1)) )

	override val districtTbl = new SHMap[(Int,Int),(String,String,String,String,String,String,Float,Double,Int)]/*(1f, 32)*/

	override val orderLineTbl = new SHMap[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)](/*0.9f, 33554432, List((0.9f, 4194304)),*/ (k:(Int,Int,Int,Int), v:(Int,Int,Option[Date],Int,Float,String)) => ((k._1, k._2, k._3)) )

	override val customerTbl = new SHMap[(Int,Int,Int),(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)] (/*1f, 65536, List((1f, 16384)),*/ (k:(Int,Int,Int), v:(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)) => ((k._2, k._3, v._3)) )

	override val stockTbl = new SHMap[(Int,Int),(Int,String,String,String,String,String,String,String,String,String,String,Int,Int,Int,String)]/*(1f, 262144)*/

	override val customerWarehouseFinancialInfoMap = new SHMap[(Int,Int,Int),(Float,String,String,Float)]/*(1f, 65536)*/

	def onInsert_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int)(implicit xact:Transaction) = {
		newOrderTbl += ((no_o_id, no_d_id, no_w_id), true)
	}

	def onDelete_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int)(implicit xact:Transaction) = {
		newOrderTbl -= ((no_o_id, no_d_id, no_w_id))
	}

    /*Func*/ def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int)(implicit xact:Transaction):Option[Int] = {
      var first_no_o_id:Option[Int] = None
      newOrderTbl.slice(0, (no_d_id_input, no_w_id_input)).foreach { case ((no_o_id,_,_),_) =>
        if(no_o_id <= first_no_o_id.getOrElse(Integer.MAX_VALUE)) {
          first_no_o_id = Some(no_o_id)
        }
      }
      first_no_o_id
    }

	def onInsert_HistoryTbl(h_c_id:Int, h_c_d_id:Int, h_c_w_id:Int, h_d_id:Int, h_w_id:Int, h_date:Date, h_amount:Float, h_data:String)(implicit xact:Transaction) = {
		historyTbl += ((h_c_id,h_c_d_id,h_c_w_id,h_d_id,h_w_id,roundDate(h_date),h_amount,h_data), true)
	}

	def onInsert_Item(i_id:Int, i_im_id:Int, i_name:String, i_price:Float, i_data:String)(implicit xact:Transaction) = {
		itemPartialTbl += (i_id, (/*i_im_id,*/i_name,i_price,i_data))
	}

	/*Func*/ def findItem(item_id:Int)(implicit xact:Transaction) = {
		itemPartialTbl(item_id)
	}

	def onInsert_Order(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int, o_entry_d:Date, o_carrier_id:Option[Int], o_ol_cnt:Int, o_all_local:Boolean)(implicit xact:Transaction) = {
		orderTbl += ((o_id,o_d_id,o_w_id), (o_c_id,o_entry_d,o_carrier_id,o_ol_cnt,o_all_local))
	}

	/*Func*/ def findMaxOrder(o_w_id_arg:Int, o_d_id_arg:Int, c_id_arg:Int)(implicit xact:Transaction) = {
		var max_o_id = -1
		orderTbl.slice(0,(o_d_id_arg,o_w_id_arg, c_id_arg)).foreach { case ((o_id,_,_), (_,_,_,_,_)) =>
			if(o_id > max_o_id) {
				max_o_id = o_id
			}
		}
		max_o_id
	}

	/*Func*/ def findOrder(max_o_id:Int, o_w_id_arg:Int, o_d_id_arg:Int)(implicit xact:Transaction) = {
		orderTbl((max_o_id,o_d_id_arg,o_w_id_arg))
	}

	def onUpdate_Order_forDelivery(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int/*, o_entry_d:Date*/, o_carrier_id:Option[Int]/*, o_ol_cnt:Int, o_all_local:Boolean*/)(implicit xact:Transaction) = {
		orderTbl.update((o_id,o_d_id,o_w_id),(currentVal/*:(Int, java.util.Date, Option[Int], Int, Boolean))*/ => ((o_c_id,currentVal._2,o_carrier_id,currentVal._4,currentVal._5))))
	}

	def onUpdate_Order_byFunc(o_id:Int, o_d_id:Int, o_w_id:Int, updateFunc:((Int, Date, Option[Int], Int, Boolean)) => (Int, Date, Option[Int], Int, Boolean))(implicit xact:Transaction) = {
		orderTbl.update((o_id,o_d_id,o_w_id),updateFunc)
	}

	def onInsert_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		warehouseTbl += (w_id, (w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		warehouseTbl(w_id) = ((w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse_byFunc(w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double)) => (String, String, String, String, String, String, Float, Double))(implicit xact:Transaction) = {
		warehouseTbl.update(w_id,updateFunc)
	}

	def onInsert_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int)(implicit xact:Transaction) = {
		districtTbl += ((d_id,d_w_id), (d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int)(implicit xact:Transaction) = {
		districtTbl += ((d_id,d_w_id), (d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District_forNewOrder(d_id:Int, d_w_id:Int/*, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String*/, d_tax:Float/*, d_ytd:Float*/, d_next_o_id:Int)(implicit xact:Transaction) = {
		val (d_name,d_street1,d_street2,d_city,d_state,d_zip,_,d_ytd,_) = districtTbl(d_id,d_w_id)
		districtTbl((d_id,d_w_id)) = ((d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District_byFunc(d_id:Int, d_w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double, Int)) => (String, String, String, String, String, String, Float, Double, Int))(implicit xact:Transaction) = {
		districtTbl.update((d_id,d_w_id), updateFunc)
	}

	/*Func*/ def findDistrict(w_id:Int, d_id:Int)(implicit xact:Transaction) = {
		districtTbl((d_id,w_id))
	}

	def onInsertOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
		orderLineTbl += ((ol_o_id, ol_d_id, ol_w_id, ol_number), (ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

	def onUpdateOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
		orderLineTbl((ol_o_id, ol_d_id, ol_w_id, ol_number)) = ((ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

    /*Func*/ def orderLineTblSlice[P](part:Int, partKey:P, f: (((Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String))) => Unit)(implicit xact:Transaction) = {
		orderLineTbl.slice(0, partKey).foreach(f)
    }
    /*Func*/ def orderLineTblSliceEntry[P](part:Int, partKey:P, f: java.util.Map.Entry[SEntry[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)], Boolean] => Unit)(implicit xact:Transaction) = {
		orderLineTbl.slice(0, partKey).foreachEntry(f)
    }

    def onInsertCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      customerTbl += ((c_id,c_d_id,c_w_id), (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
      var w_tax = 0f
      w_tax = warehouseTbl(c_w_id)._7
      customerWarehouseFinancialInfoMap += ((c_id,c_d_id,c_w_id), (c_discount, c_last, c_credit, w_tax))
    }

    /*Func*/ def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int)(implicit xact:Transaction) = {
      customerWarehouseFinancialInfoMap(c_id,d_id,w_id)
    }

    def onUpdateCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      customerTbl((c_id,c_d_id,c_w_id)) = ((c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
    }

    def onUpdateCustomer_byFunc(c_id: Int, c_d_id: Int, c_w_id: Int, updateFunc:((String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String)) => (String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String))(implicit xact:Transaction) = {
      customerTbl.update((c_id,c_d_id,c_w_id),updateFunc)
    }

    def onUpdateCustomer_byEntry(c: SEntry[(Int,Int,Int),(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)], c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      c.value = (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment/*+h_amount*/,c_payment_cnt/*+1*/,c_delivery_cnt,c_data)
    }

    def onInsertStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      stockTbl += ((s_i_id,s_w_id), (s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      stockTbl((s_i_id,s_w_id)) = ((s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock_byFunc(s_i_id:Int, s_w_id:Int, updateFunc:((Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String)) => (Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String))(implicit xact:Transaction) = {
      stockTbl.update((s_i_id,s_w_id), updateFunc)
    }

	/*Func*/ def findStock(item_id:Int, w_id:Int)(implicit xact:Transaction) = {
		stockTbl(item_id,w_id)
	}

	private class MiniCustomer(val cust_id:Int, val cust_first:String) extends Ordered[MiniCustomer] {
		def compare(that: MiniCustomer) = this.cust_first.compareToIgnoreCase(that.cust_first)
		override def toString = "MiniCustomer(%s,%s)".format(cust_id, cust_first)
	} 

    def findCustomerEntryByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String)(implicit xact:Transaction) = {
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
    def findCustomerEntryById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int)(implicit xact:Transaction) = {
      customerTbl.getEntry((c_id,input_c_d_id,input_c_w_id))
    }

    def findCustomerByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String)(implicit xact:Transaction) = {
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
    def findCustomerById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int)(implicit xact:Transaction) = {
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = customerTbl((c_id,input_c_d_id,input_c_w_id))
      (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data,c_id)
    }
}
