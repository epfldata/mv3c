package ddbt.tpcc.tx
import java.io._
import scala.collection.mutable._
import java.util.Date
import java.sql.Connection
import java.sql.Statement
import java.sql.ResultSet
import ddbt.tpcc.loadtest.Util._
import ddbt.tpcc.loadtest.DatabaseConnector._
import ddbt.tpcc.lib.concurrent.ConcurrentSHMap
import ddbt.tpcc.lib.mvconcurrent.ConcurrentSHMapMVCC
import ddbt.tpcc.lib.mvconcurrent.ConcurrentSHMapMVCC.SEntryMVCC
import ddbt.tpcc.lib.mvconcurrent.ConcurrentSHMapMVCC.DeltaVersion
import ddbt.tpcc.loadtest.TpccConstants._

import TpccTable._
import MVCCTpccTableV3._
import java.util.concurrent.atomic.AtomicLong

object MVCCTpccTableV3 {
	type MutableMap[K,V] = ddbt.tpcc.lib.shm.SHMap[K,V]

	val DEBUG = true
	def debug(msg: => String) = if(DEBUG) println(msg)

	class Transaction(val tm: TransactionManager, val name: String, val startTS: Long, var xactId: Long, var committed:Boolean=false) {
		val DEFAULT_UNDO_BUFFER_SIZE = 64

		type Predicate = String
		val undoBuffer = new MutableMap[Any, DeltaVersion[_,_]](DEFAULT_UNDO_BUFFER_SIZE)
		var predicates = List[Predicate]()

		def commitTS = xactId

		def transactionId = if(xactId < TransactionManager.TRANSACTION_ID_GEN_START) xactId else (xactId - TransactionManager.TRANSACTION_ID_GEN_START + 1)

		def isCommitted = (xactId < TransactionManager.TRANSACTION_ID_GEN_START)

		def addPredicate(p:Predicate) = {
			predicates = p :: predicates
		}
		def commit = tm.commit(this)
		def rollback = tm.rollback(this)
	}

	object TransactionManager {
		val TRANSACTION_ID_GEN_START = (1L << 32)
	}

	class TransactionManager {

		var transactionIdGen = new AtomicLong(TransactionManager.TRANSACTION_ID_GEN_START)
		var startAndCommitTimestampGen = new AtomicLong(1L)

		val activeXacts = new ConcurrentSHMap[Long,Transaction]
		var recentlyCommittedXacts = List[Transaction]()

		def begin(name: String) = {
			val xactId = transactionIdGen.getAndIncrement()
			val startTS = startAndCommitTimestampGen.getAndIncrement()
			val xact = new Transaction(this, name, startTS, xactId)
			debug("T%d (%s) started at %d".format(xact.transactionId, name, startTS))
			xact
		}
		def commit(implicit xact:Transaction) = {
			//TODO: should be implemented completely
			// missing:
			//  - validation phase
			this.synchronized {
				val xactId = xact.xactId
				activeXacts -= xactId
				debug("T%d (%s) committed at %d\n\twith undo buffer(%d) = %%s".format(xact.transactionId, xact.name, xact.commitTS, xact.undoBuffer.size, xact.undoBuffer))
				xact.xactId = startAndCommitTimestampGen.getAndIncrement()
				recentlyCommittedXacts = xact :: recentlyCommittedXacts
			}
		}
		def rollback(implicit xact:Transaction) = {
			//TODO: should be implemented completely
			// missing:
			//  - removing the undo buffer
			this.synchronized {
				activeXacts -= xact.xactId
				debug("T%d (%s) rolled back at %d\n\twith undo buffer(%d) = %%s".format(xact.transactionId, xact.name, xact.commitTS, xact.undoBuffer.size, xact.undoBuffer))
			}
		}

		/////// TABLES \\\\\\\
		val newOrderTbl = new ConcurrentSHMapMVCC[(Int,Int,Int),Tuple1[Boolean]](/*0.9f, 262144,*/ (k:(Int,Int,Int),v:Tuple1[Boolean]) => ((k._2, k._3)) )

		val historyTbl = new ConcurrentSHMapMVCC[(Int,Int,Int,Int,Int,Date,Float,String),Tuple1[Boolean]]/*(0.9f, 4194304)*/

		val warehouseTbl = new ConcurrentSHMapMVCC[Int,(String,String,String,String,String,String,Float,Double)]

		val itemPartialTbl = new ConcurrentSHMapMVCC[Int,(/*Int,*/String,Float,String)]/*(1f, 262144)*/

		val orderTbl = new ConcurrentSHMapMVCC[(Int,Int,Int),(Int,Date,Option[Int],Int,Boolean)](/*0.9f, 4194304,*/ (k:(Int,Int,Int), v:(Int,Date,Option[Int],Int,Boolean)) => ((k._2, k._3, v._1)) )

		val districtTbl = new ConcurrentSHMapMVCC[(Int,Int),(String,String,String,String,String,String,Float,Double,Int)]/*(1f, 32)*/

		val orderLineTbl = new ConcurrentSHMapMVCC[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)](/*0.9f, 33554432, List((0.9f, 4194304)),*/ (k:(Int,Int,Int,Int), v:(Int,Int,Option[Date],Int,Float,String)) => ((k._1, k._2, k._3)) )

		val customerTbl = new ConcurrentSHMapMVCC[(Int,Int,Int),(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)] (/*1f, 65536, List((1f, 16384)),*/ (k:(Int,Int,Int), v:(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)) => ((k._2, k._3, v._3)) )

		val stockTbl = new ConcurrentSHMapMVCC[(Int,Int),(Int,String,String,String,String,String,String,String,String,String,String,Int,Int,Int,String)]/*(1f, 262144)*/

		val customerWarehouseFinancialInfoMap = new ConcurrentSHMapMVCC[(Int,Int,Int),(Float,String,String,Float)]/*(1f, 65536)*/
	}

	class MVCCException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends RuntimeException(message, cause) {
		xact.rollback //rollback the transaction upon any exception
	}
	class MVCCConcurrentWriteException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause)
	class MVCCRecordAlreayExistsException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause)

	// this should not be modified
	// it is set to false in order to make the inheritance
	// from 
	def testSpecialDsUsed = false
}
/**
 * Tables for TPC-C Benchmark (with all operations reflected through its API
 * (and not directly via internal tables)
 *
 * @author Mohammad Dashti
 */
class MVCCTpccTableV3 extends TpccTable(7) {

	override val newOrderTbl = null
	override val historyTbl = null
	override val warehouseTbl = null
	override val itemPartialTbl = null
	override val orderTbl = null
	override val districtTbl = null
	override val orderLineTbl = null
	override val customerTbl = null
	override val stockTbl = null
	override val customerWarehouseFinancialInfoMap = null

	val tm = new TransactionManager

	def begin = tm.begin("adhoc")
	def begin(name: String) = tm.begin(name)
	def commit(implicit xact:Transaction) = tm.commit
	def rollback(implicit xact:Transaction) = tm.rollback

	override def testSpecialDsUsed = MVCCTpccTableV3.testSpecialDsUsed

	def onInsert_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int)(implicit xact:Transaction) = {
		tm.newOrderTbl += ((no_o_id, no_d_id, no_w_id), Tuple1(true))
	}

	def onDelete_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int)(implicit xact:Transaction) = {
		tm.newOrderTbl -= ((no_o_id, no_d_id, no_w_id))
	}

    /*Func*/ def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int)(implicit xact:Transaction):Option[Int] = {
      xact.addPredicate("P1_findFirstNewOrder")
      var first_no_o_id:Option[Int] = None
      tm.newOrderTbl.slice(0, (no_d_id_input, no_w_id_input)).foreach { case ((no_o_id,_,_),_) =>
        if(no_o_id <= first_no_o_id.getOrElse(Integer.MAX_VALUE)) {
          first_no_o_id = Some(no_o_id)
        }
      }
      first_no_o_id
    }

	def onInsert_HistoryTbl(h_c_id:Int, h_c_d_id:Int, h_c_w_id:Int, h_d_id:Int, h_w_id:Int, h_date:Date, h_amount:Float, h_data:String)(implicit xact:Transaction) = {
		tm.historyTbl += ((h_c_id,h_c_d_id,h_c_w_id,h_d_id,h_w_id,roundDate(h_date),h_amount,h_data), Tuple1(true))
	}

	def onInsert_Item(i_id:Int, i_im_id:Int, i_name:String, i_price:Float, i_data:String)(implicit xact:Transaction) = {
		tm.itemPartialTbl += (i_id, (/*i_im_id,*/i_name,i_price,i_data))
	}

	/*Func*/ def findItem(item_id:Int)(implicit xact:Transaction) = {
		xact.addPredicate("P2_findItem")
		tm.itemPartialTbl(item_id)
	}

	def onInsert_Order(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int, o_entry_d:Date, o_carrier_id:Option[Int], o_ol_cnt:Int, o_all_local:Boolean)(implicit xact:Transaction) = {
		tm.orderTbl += ((o_id,o_d_id,o_w_id), (o_c_id,o_entry_d,o_carrier_id,o_ol_cnt,o_all_local))
	}

	/*Func*/ def findMaxOrder(o_w_id_arg:Int, o_d_id_arg:Int, c_id_arg:Int)(implicit xact:Transaction) = {
		xact.addPredicate("P2_findMaxOrder")
		var max_o_id = -1
		tm.orderTbl.slice(0,(o_d_id_arg,o_w_id_arg, c_id_arg)).foreach { case ((o_id,_,_), (_,_,_,_,_)) =>
			if(o_id > max_o_id) {
				max_o_id = o_id
			}
		}
		max_o_id
	}

	/*Func*/ def findOrder(max_o_id:Int, o_w_id_arg:Int, o_d_id_arg:Int)(implicit xact:Transaction) = {
		xact.addPredicate("P2_findOrder")
		tm.orderTbl((max_o_id,o_d_id_arg,o_w_id_arg))
	}

	def onUpdate_Order_forDelivery(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int/*, o_entry_d:Date*/, o_carrier_id:Option[Int]/*, o_ol_cnt:Int, o_all_local:Boolean*/)(implicit xact:Transaction) = {
		tm.orderTbl.update((o_id,o_d_id,o_w_id),(currentVal/*:(Int, java.util.Date, Option[Int], Int, Boolean))*/ => ((o_c_id,currentVal._2,o_carrier_id,currentVal._4,currentVal._5))))
	}

	def onUpdate_Order_byFunc(o_id:Int, o_d_id:Int, o_w_id:Int, updateFunc:((Int, Date, Option[Int], Int, Boolean)) => (Int, Date, Option[Int], Int, Boolean))(implicit xact:Transaction) = {
		tm.orderTbl.update((o_id,o_d_id,o_w_id),updateFunc)
	}

	def onInsert_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		tm.warehouseTbl += (w_id, (w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		tm.warehouseTbl(w_id) = ((w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse_byFunc(w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double)) => (String, String, String, String, String, String, Float, Double))(implicit xact:Transaction) = {
		tm.warehouseTbl.update(w_id,updateFunc)
	}

	def onInsert_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int)(implicit xact:Transaction) = {
		tm.districtTbl += ((d_id,d_w_id), (d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District(d_id:Int, d_w_id:Int, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String, d_tax:Float, d_ytd:Double, d_next_o_id:Int)(implicit xact:Transaction) = {
		tm.districtTbl += ((d_id,d_w_id), (d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District_forNewOrder(d_id:Int, d_w_id:Int/*, d_name:String, d_street1:String, d_street2:String, d_city:String, d_state:String, d_zip:String*/, d_tax:Float/*, d_ytd:Float*/, d_next_o_id:Int)(implicit xact:Transaction) = {
		val (d_name,d_street1,d_street2,d_city,d_state,d_zip,_,d_ytd,_) = tm.districtTbl(d_id,d_w_id)
		tm.districtTbl((d_id,d_w_id)) = ((d_name,d_street1,d_street2,d_city,d_state,d_zip,d_tax,d_ytd,d_next_o_id))
	}

	def onUpdate_District_byFunc(d_id:Int, d_w_id:Int, updateFunc:((String, String, String, String, String, String, Float, Double, Int)) => (String, String, String, String, String, String, Float, Double, Int))(implicit xact:Transaction) = {
		tm.districtTbl.update((d_id,d_w_id), updateFunc)
	}

	/*Func*/ def findDistrict(w_id:Int, d_id:Int)(implicit xact:Transaction) = {
		tm.districtTbl((d_id,w_id))
	}

	def onInsertOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
		tm.orderLineTbl += ((ol_o_id, ol_d_id, ol_w_id, ol_number), (ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

	def onUpdateOrderLine(ol_o_id:Int, ol_d_id:Int, ol_w_id:Int, ol_number:Int, ol_i_id:Int, ol_supply_w_id:Int, ol_delivery_d:Option[Date], ol_quantity:Int, ol_amount:Float, ol_dist_info:String)(implicit xact:Transaction): Unit = {
		tm.orderLineTbl((ol_o_id, ol_d_id, ol_w_id, ol_number)) = ((ol_i_id, ol_supply_w_id, ol_delivery_d, ol_quantity, ol_amount, ol_dist_info))
    }

    /*Func*/ def orderLineTblSlice[P](part:Int, partKey:P, f: ((Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)) => Unit)(implicit xact:Transaction) = {
		tm.orderLineTbl.slice(0, partKey).foreach(f)
    }
    /*Func*/ def orderLineTblSliceEntry[P](part:Int, partKey:P, f: java.util.Map.Entry[SEntryMVCC[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)], Boolean] => Unit)(implicit xact:Transaction) = {
		tm.orderLineTbl.slice(0, partKey).foreachEntry(f)
    }

    def onInsertCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      tm.customerTbl += ((c_id,c_d_id,c_w_id), (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
      var w_tax = 0f
      w_tax = tm.warehouseTbl(c_w_id)._7
      tm.customerWarehouseFinancialInfoMap += ((c_id,c_d_id,c_w_id), (c_discount, c_last, c_credit, w_tax))
    }

    /*Func*/ def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int)(implicit xact:Transaction) = {
      tm.customerWarehouseFinancialInfoMap(c_id,d_id,w_id)
    }

    def onUpdateCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      tm.customerTbl((c_id,c_d_id,c_w_id)) = ((c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
    }

    def onUpdateCustomer_byFunc(c_id: Int, c_d_id: Int, c_w_id: Int, updateFunc:((String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String)) => (String, String, String, String, String, String, String, String, String, Date, String, Float, Float, Float, Float, Int, Int, String))(implicit xact:Transaction) = {
      tm.customerTbl.update((c_id,c_d_id,c_w_id),updateFunc)
    }

    def onUpdateCustomer_byEntry(c: DeltaVersion[(Int,Int,Int),(String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)], c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
    	c.setEntryValue((c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment/*+h_amount*/,c_payment_cnt/*+1*/,c_delivery_cnt,c_data))
    }

    def onInsertStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      tm.stockTbl += ((s_i_id,s_w_id), (s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      tm.stockTbl((s_i_id,s_w_id)) = ((s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock_byFunc(s_i_id:Int, s_w_id:Int, updateFunc:((Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String)) => (Int, String, String, String, String, String, String, String, String, String, String, Int, Int, Int, String))(implicit xact:Transaction) = {
      tm.stockTbl.update((s_i_id,s_w_id), updateFunc)
    }

	/*Func*/ def findStock(item_id:Int, w_id:Int)(implicit xact:Transaction) = {
		tm.stockTbl(item_id,w_id)
	}

	private class MiniCustomer(val cust_id:Int, val cust_first:String) extends Ordered[MiniCustomer] {
		def compare(that: MiniCustomer) = this.cust_first.compareToIgnoreCase(that.cust_first)
		override def toString = "MiniCustomer(%s,%s)".format(cust_id, cust_first)
	} 

    def findCustomerEntryByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String)(implicit xact:Transaction) = {
		var customers = new ArrayBuffer[MiniCustomer]
		//we should slice over input_c_last
		tm.customerTbl.slice(0, (input_c_d_id, input_c_w_id, input_c_last)).foreach { case ((c_id,_,_) , (c_first,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)) =>
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
		tm.customerTbl.getEntry((c_id,input_c_d_id,input_c_w_id))
    }
    def findCustomerEntryById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int)(implicit xact:Transaction) = {
      tm.customerTbl.getEntry((c_id,input_c_d_id,input_c_w_id))
    }

    def findCustomerByName(input_c_w_id: Int, input_c_d_id: Int, input_c_last: String)(implicit xact:Transaction) = {
      var customers = new ArrayBuffer[MiniCustomer]
      //we should slice over input_c_last
      tm.customerTbl.slice(0, (input_c_d_id, input_c_w_id, input_c_last)).foreach { case ((c_id,_,_) , (c_first,_,c_last,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)) =>
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
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = tm.customerTbl((c_id,input_c_d_id,input_c_w_id))
      (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data,c_id)
    }
    def findCustomerById(input_c_w_id: Int, input_c_d_id: Int, c_id: Int)(implicit xact:Transaction) = {
      val (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data) = tm.customerTbl((c_id,input_c_d_id,input_c_w_id))
      (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data,c_id)
    }

    override def toTpccTable = {
    	val res = new TpccTable(7)
		implicit val xact = this.begin
		val THE_VALUE_DOES_NOT_EXIST = -1 //TODO: should be FIXED
		tm.newOrderTbl.foreach { case (k,v) => res.onInsert_NewOrder(k._1,k._2,k._3) }
		tm.historyTbl.foreach { case (k,v) => res.onInsert_HistoryTbl(k._1,k._2,k._3,k._4,k._5,k._6,k._7,k._8) }
		tm.warehouseTbl.foreach { case (k,v) => res.onInsert_Warehouse(k,v._1,v._2,v._3,v._4,v._5,v._6,v._7,v._8) }
		tm.itemPartialTbl.foreach { case (k,v) => res.onInsert_Item(k,THE_VALUE_DOES_NOT_EXIST,v._1,v._2,v._3) }
		tm.customerTbl.foreach { case (k,v) => res.onInsertCustomer(k._1,k._2,k._3,v._1,v._2,v._3,v._4,v._5,v._6,v._7,v._8,v._9,v._10,v._11,v._12,v._13,v._14,v._15,v._16,v._17,v._18) }
		tm.orderTbl.foreach { case (k,v) => res.onInsert_Order(k._1,k._2,k._3,v._1,v._2,v._3,v._4,v._5) }
		tm.districtTbl.foreach { case (k,v) => res.onInsert_District(k._1,k._2,v._1,v._2,v._3,v._4,v._5,v._6,v._7,v._8,v._9) }
		tm.orderLineTbl.foreach { case (k,v) => res.onInsertOrderLine(k._1,k._2,k._3,k._4,v._1,v._2,v._3,v._4,v._5,v._6) }
		tm.stockTbl.foreach { case (k,v) => res.onInsertStock(k._1,k._2,v._1,v._2,v._3,v._4,v._5,v._6,v._7,v._8,v._9,v._10,v._11,v._12,v._13,v._14,v._15) }
		this.commit
	    res
    }
}
