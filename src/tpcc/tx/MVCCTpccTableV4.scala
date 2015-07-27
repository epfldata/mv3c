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
import ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T
import ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T.SEntryMVCC
import ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T.DeltaVersion
import ConcurrentSHMapMVC3T.{Predicate, Table, INSERT_OP, DELETE_OP, UPDATE_OP}
import ddbt.tpcc.loadtest.TpccConstants._
import scala.concurrent._
import ExecutionContext.Implicits.global

import TpccTable._
import MVCCTpccTableV4._
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicBoolean

object MVCCTpccTableV4 {
	type MutableMap[K,V] = ddbt.tpcc.lib.shm.SHMap[K,V]
	type DeltaVersion[K,V<:Product] = ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T.DeltaVersion[K,V]
	type ClosureTransition = ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T.ClosureTransition
	type ChangeHandler[K,V<:Product] = ddbt.tpcc.lib.mvc3t.ConcurrentSHMapMVC3T.ChangeHandler[K,V]
	// type Conflict[K,V<:Product] = (Predicate[K,V],DeltaVersion[K,V])
	type Conflict = (Predicate[_,_],DeltaVersion[_,_])

	val DEBUG = false
	val ERROR = false
	val DISABLE_GC = false
	val PARALLEL_GC = true

	val TRUE_TUPLE = Tuple1(true)

	val NEWORDER_TBL = "newOrderTbl"
	val HISTORY_TBL = "historyTbl"
	val WAREHOUSE_TBL = "warehouseTbl"
	val ITEM_TBL = "itemPartialTbl"
	val ORDER_TBL = "orderTbl"
	val DISTRICT_TBL = "districtTbl"
	val ORDERLINE_TBL = "orderLineTbl"
	val CUSTOMER_TBL = "customerTbl"
	val STOCK_TBL = "stockTbl"
	// val CUSTOMERWAREHOUSE_TBL = "customerWarehouseFinancialInfoMap"

	val TABLES = List(NEWORDER_TBL,HISTORY_TBL,WAREHOUSE_TBL,ITEM_TBL,ORDER_TBL,DISTRICT_TBL,ORDERLINE_TBL,CUSTOMER_TBL,STOCK_TBL/*,CUSTOMERWAREHOUSE_TBL*/)

	type NewOrderTblKey = (Int,Int,Int)
	type NewOrderTblValue = Tuple1[Boolean]
	type HistoryTblKey = (Int,Int,Int,Int,Int,Date,Float,String)
	type HistoryTblValue = Tuple1[Boolean]
	type WarehouseTblKey = Int
	type WarehouseTblValue = (String,String,String,String,String,String,Float,Double)
	type ItemTblKey = Int
	type ItemTblValue = (/*Int,*/String,Float,String)
	type OrderTblKey = (Int,Int,Int)
	type OrderTblValue = (Int,Date,Option[Int],Int,Boolean)
	type DistrictTblKey = (Int,Int)
	type DistrictTblValue = (String, String, String, String, String, String, Float, Double, Int)
	type OrderLineTblKey = (Int,Int,Int,Int)
	type OrderLineTblValue = (Int,Int,Option[Date],Int,Float,String)
	type CustomerTblKey = (Int,Int,Int)
	type CustomerTblValue = (String,String,String,String,String,String,String,String,String,Date,String,Float,Float,Float,Float,Int,Int,String)
	type StockTblKey = (Int,Int)
	type StockTblValue = (Int,String,String,String,String,String,String,String,String,String,String,Int,Int,Int,String)

	var forcedRollback = 0 // is the rollback requested by transaction program (and it’s a part of benchmark)
	var failedValidation = 0 // is the rollback due to the validation failure
	var failedConcurrentUpdate = 0 // is the rollback due to a concurrent update (for the same object), which is not allowed in the reference impl.
	var failedConcurrentInsert = 0 // is the rollback due to a concurrent insert (for the same object), which is not allowed in the reference impl.

	def clear = {
		forcedRollback = 0
		failedValidation = 0
		failedConcurrentUpdate = 0
		failedConcurrentInsert = 0
	}

	//@inline //TODO FIX IT: it should be inlined for production use
	def forceDebug(msg: => String)(implicit xact:Transaction) = println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def debug(msg: => String)(implicit xact:Transaction) = if(DEBUG) println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(msg: => String)(implicit xact:Transaction): Unit = if(ERROR) System.err.println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(e: Throwable)(implicit xact:Transaction): Unit = if(ERROR) { error(e.toString); e.getStackTrace.foreach(st => error(st.toString))}

	class Transaction(val tm: TransactionManager, val name: String, val startTS: Long, var xactId: Long, var committed:Boolean=false) {
		val DEFAULT_UNDO_BUFFER_SIZE = 64

		val undoBuffer = new HashSet[DeltaVersion[_,_]]()
		var predicates = new MutableMap[Table, HashSet[Predicate[_,_]]](TABLES.size * 2)
		var predicateResultCache = new MutableMap[Predicate[_,_], AnyRef](TABLES.size * 2)
		val closureTransitions = new ListBuffer[ClosureTransition]()

		var command: TpccCommand = null

		def setCommand(c: TpccCommand) {
			command = c
		}

		def commitTS = xactId

		def transactionId = startTS //if(xactId < TransactionManager.TRANSACTION_ID_GEN_START) xactId else (xactId - TransactionManager.TRANSACTION_ID_GEN_START)

		def isCommitted = (xactId < TransactionManager.TRANSACTION_ID_GEN_START)

		def isReadOnly = undoBuffer.isEmpty

		def addPredicate(p:Predicate[_,_]) = {
			val pList = predicates.getNullOnNotFound(p.tbl.tblName)
			if(pList != null) pList += p
			else {
				val pBuf = new HashSet[Predicate[_,_]]
				pBuf += p
				predicates += (p.tbl.tblName, pBuf)
			}
		}

		def findPredicateCachedResult(p:Predicate[_,_]):Option[AnyRef] = {
			val res = predicateResultCache.getNullOnNotFound(p)
			if(res == null) None else Some(res)
		}

		def cachePredicateResult(p:Predicate[_,_], res: AnyRef) {
			predicateResultCache += (p, res)
		}

		def commit = tm.commit(this)
		def rollback = tm.rollback(this)

		override def toString = "T"+transactionId+"<"+name+">"

		def matchesPredicates(dv:DeltaVersion[_,_]): Set[Conflict] = {
			if(dv == null) println("dv is null in matchesPredicates")
			val preds = predicates.getNullOnNotFound(dv.getTable)
			var res = Set[Conflict]()
			if(preds != null) dv.op match {
				case INSERT_OP => {
					res = res ++ imageMatchesPredicates(dv, preds)
				}
				case DELETE_OP => {
					val nextDv = dv.next
					if(nextDv == null) throw new RuntimeException("A deleted version should always have the next pointer.")
					else {
						res = res ++ imageMatchesPredicates(nextDv, preds)
					}
				}
				case UPDATE_OP => {
					val nextDv = dv.next
					// An updated version after GC might not have a next pointer
					// if(nextDv == null) throw new RuntimeException("An updated version should always have the next pointer. The update version is " + dv + " and dv.next = " + dv.next + " and dv.prev = " + dv.prev)
					// else 
					res = res ++ imageMatchesPredicates(dv, preds)
					if(nextDv ne null) res = res ++ imageMatchesPredicates(nextDv, preds)
				}
				case x => throw new IllegalStateException("DeltaVersion operation " + x + " does not exist.")
			}
			res
		}

		def imageMatchesPredicates(dv:DeltaVersion[_,_], preds: HashSet[Predicate[_,_]]): Set[Conflict] = {
			var res = Set[(Predicate[_,_], DeltaVersion[_,_])]()
			preds.foreach { p => if((p.tbl == dv.getMap) && p.matches(dv)) {
					debug("\t\t\t matched " + p + "!")(this)
					import scala.language.existentials
					res = res + ((p.asInstanceOf[Predicate[_,Product]], dv.asInstanceOf[DeltaVersion[_,Product]]))
					// return true
				}
			}
			res
		}

		def addClosureTransition(ct: ClosureTransition) {
			closureTransitions += ct
		}
	}

	object TransactionManager {
		val TRANSACTION_ID_GEN_START = (1L << 32)
		val TRANSACTION_STRAT_TS_GEN_START = 1L
	}

	class TransactionManager(isUnitTestEnabled: =>Boolean) {


		val transactionIdGen = new AtomicLong(TransactionManager.TRANSACTION_ID_GEN_START)
		val startAndCommitTimestampGen = new AtomicLong(TransactionManager.TRANSACTION_STRAT_TS_GEN_START)
		val isGcActive = new AtomicBoolean(DISABLE_GC)

		val activeXacts = new LinkedHashMap[Long,Transaction]
		val recentlyCommittedXacts = new ListBuffer[Transaction]()
		val allCommittedXacts = new ListBuffer[Transaction]()

		def begin(name: String) = activeXacts.synchronized {
			val xactId = transactionIdGen.getAndIncrement()
			val startTS = startAndCommitTimestampGen.getAndIncrement()
			implicit val xact = new Transaction(this, name, startTS, xactId)
			activeXacts.put(xactId, xact)
			debug("started at %d".format(startTS))
			xact
		}
		def commit(implicit xact:Transaction):Boolean = this.synchronized {
			debug("commit started")
			val xactId = xact.xactId
			if(activeXacts.contains(xactId)){
				if(xact.isReadOnly) {
					xact.xactId = xact.startTS
					activeXacts.synchronized {
						activeXacts -= xactId
					}
					recentlyCommittedXacts.synchronized {
						recentlyCommittedXacts += xact
						if(isUnitTestEnabled) allCommittedXacts += xact
					}
					debug("(read-only) commit succeeded (with commitTS = %d)".format(xact.commitTS))
					true
				} else if(!validate) {
					debug("commit failed: transaction validation failed! We have to roll it back...")
					rollback
					false
				} else {
					activeXacts.synchronized {
						xact.xactId = startAndCommitTimestampGen.getAndIncrement()
						activeXacts -= xactId
					}
					debug("\twith undo buffer(%d) = %%s".format(xact.undoBuffer.size, xact.undoBuffer))
					recentlyCommittedXacts.synchronized {
						recentlyCommittedXacts += xact
						if(isUnitTestEnabled) allCommittedXacts += xact
					}
					garbageCollect
					debug("commit succeeded (with commitTS = %d)".format(xact.commitTS))
					true
				}
			} else {
				debug("commit failed: transaction does not exist! We have to roll it back...")
				rollback
				false
			}
		}
		//TODO: should be implemented completely
		// missing:
		//  - validation can run in parallel for the transactions that are in this stage (we do not have to synchronize it)
		//  - attribute-level validation should be supported
		def validate(implicit xact:Transaction): Boolean = {
			debug("\tvalidation started")
			val concurrentXacts = recentlyCommittedXacts.filter(t => t.startTS > xact.startTS)
			debug("\t\tconcurrentXacts = " + concurrentXacts)
			var conflicts = Set[Conflict]()
			concurrentXacts.foreach { t =>
				t.undoBuffer.foreach { dv =>
					debug("\t\tchecking whether " + dv + " (in " + t + ") matches predicates in " + xact)
					conflicts = conflicts ++ xact.matchesPredicates(dv)
				}
			}

			if(conflicts.isEmpty) {
				debug("\tvalidation succeeded")
				true
			} else {
				forceDebug("\tvalidation failed")
				// forceDebug("\tconflicts are => " + conflicts)
				// forceDebug("\tundoBuffer contains => " + xact.undoBuffer)
				val conflictPreds = conflicts.map(_._1)
				xact.closureTransitions.foreach{ ct =>
					var activated = false
					ct.handlers.foreach{ h =>
						if(conflictPreds.exists( p => h.preds.contains(p))) {
							forceDebug("\thandler activated for " + h.preds)
							activated = true
							h.handler(ct.outputVersions._1)
						} else {
							// forceDebug("\th.preds = " + h.preds)
						}
					}
					if(activated) {
						ct.outputVersions._2.foreach { innerCt =>
							innerCt.handlers.foreach{ h =>
								forceDebug("\t\tsub-handler activated")
								h.handler(innerCt.outputVersions._1)
							}
						}
					}
				}

				// failedValidation += 1
				// false
				true
			}
		}

		def rollback(implicit xact:Transaction) = this.synchronized {
			debug("rollback started")
			activeXacts.synchronized {
				activeXacts -= xact.xactId
			}
			debug("\twith undo buffer(%d) = %%s".format(xact.undoBuffer.size, xact.undoBuffer))
			xact.undoBuffer.foreach{ dv => 
				debug("\t\tremoved => (" + dv.getTable+"," + dv.getKey + ", " + dv + ")")
				dv.remove
			}
			garbageCollect
			debug("rollback finished")
		}

		private def garbageCollect(implicit xact:Transaction) {
			if(isGcActive.compareAndSet(false,true)) {
				if(PARALLEL_GC) {
					Future {
						gcInternal
						isGcActive.set(false)
					}
				} else {
					gcInternal
					isGcActive.set(false)
				}
			}
		}

		private def gcInternal(implicit xact:Transaction) {
			debug("GC started")
			if(!recentlyCommittedXacts.isEmpty) {
				var oldestActiveXactStartTS = TransactionManager.TRANSACTION_ID_GEN_START
				activeXacts.synchronized {
					activeXacts.foreach{ a => if(a._2.startTS < oldestActiveXactStartTS) oldestActiveXactStartTS = a._2.startTS }
					debug("\tactiveXacts = " + activeXacts)
					debug("\toldestActiveXactStartTS = " + oldestActiveXactStartTS)
				}

				var reachedLimit = false
				while(!reachedLimit && !recentlyCommittedXacts.isEmpty) {
					val xact = recentlyCommittedXacts.head
					implicit val theXact = xact
					if(xact.commitTS < oldestActiveXactStartTS) {
						debug("\tremoving xact = " + xact.transactionId)
						xact.undoBuffer.foreach{ dv =>
							val nextDV = dv.next
							dv.next = null
							if(nextDV != null) {
								debug("\t\tremoved version => (" + nextDV.getTable+"," + nextDV.getKey + ", " + nextDV + ")")
								nextDV.gcRemove
							}
							else if(dv.op == DELETE_OP){
								debug("\t\tcould not remove version because of no next => (" + nextDV.getTable+"," + nextDV.getKey + ", " + nextDV + ")")
							}
							if(dv.isDeleted) {
								debug("\t\tand also removed itself => (" + nextDV.getTable+"," + dv.getKey + ", " + dv + ")")
								dv.remove
							}
							else if(nextDV != null){
								debug("\t\t, and current version is => (" + dv.getTable+"," + dv.getKey + ", " + dv + ")")
							}
						}
						recentlyCommittedXacts.synchronized {
							recentlyCommittedXacts.remove(0)
						}
					} else {
						debug("\treached gc limit T"+xact.transactionId+" vs. oldestActiveXactStartTS=" + oldestActiveXactStartTS)
						reachedLimit = true
					}
				}
			}
			else {
				debug("\tnothing to be done!")
			}
			debug("GC finished")
		}

		/////// TABLES \\\\\\\
		val newOrderTbl = new ConcurrentSHMapMVC3T[NewOrderTblKey,NewOrderTblValue](NEWORDER_TBL,/*0.9f, 262144,*/ (k:NewOrderTblKey,v:NewOrderTblValue) => ((k._2, k._3)) )

		val historyTbl = new ConcurrentSHMapMVC3T[HistoryTblKey,HistoryTblValue](HISTORY_TBL/*0.9f, 4194304)*/)

		val warehouseTbl = new ConcurrentSHMapMVC3T[WarehouseTblKey,WarehouseTblValue](WAREHOUSE_TBL)

		val itemPartialTbl = new ConcurrentSHMapMVC3T[ItemTblKey,ItemTblValue](ITEM_TBL/*1f, 262144*/)

		val orderTbl = new ConcurrentSHMapMVC3T[OrderTblKey,OrderTblValue](ORDER_TBL,/*0.9f, 4194304,*/ (k:OrderTblKey, v:OrderTblValue) => ((k._2, k._3, v._1)) )

		val districtTbl = new ConcurrentSHMapMVC3T[DistrictTblKey,DistrictTblValue](DISTRICT_TBL/*1f, 32*/)

		val orderLineTbl = new ConcurrentSHMapMVC3T[OrderLineTblKey,OrderLineTblValue](ORDERLINE_TBL,/*0.9f, 33554432, List((0.9f, 4194304)),*/ (k:OrderLineTblKey, v:OrderLineTblValue) => ((k._1, k._2, k._3)) )

		val customerTbl = new ConcurrentSHMapMVC3T[CustomerTblKey,CustomerTblValue] (CUSTOMER_TBL,/*1f, 65536, List((1f, 16384)),*/ (k:CustomerTblKey, v:CustomerTblValue) => ((k._2, k._3, v._3)) )

		val stockTbl = new ConcurrentSHMapMVC3T[StockTblKey,StockTblValue](STOCK_TBL/*1f, 262144*/)

		// val customerWarehouseFinancialInfoMap = new ConcurrentSHMapMVC3T[(Int,Int,Int),(Float,String,String,Float)](CUSTOMERWAREHOUSE_TBL/*1f, 65536*/)
	}

	class MVCCException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends RuntimeException(message, cause) {
		// xact.rollback //rollback the transaction upon any exception
	}
	class MVCCConcurrentWriteException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause) {
		failedConcurrentUpdate += 1
	}
	class MVCCRecordAlreadyExistsException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause) {
		failedConcurrentInsert += 1
	}

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
class MVCCTpccTableV4 extends TpccTable(7) {

	override val newOrderTbl = null
	override val historyTbl = null
	override val warehouseTbl = null
	override val itemPartialTbl = null
	override val orderTbl = null
	override val districtTbl = null
	override val orderLineTbl = null
	override val customerTbl = null
	override val stockTbl = null
	// override val customerWarehouseFinancialInfoMap = null

	val tm = new TransactionManager(isUnitTestEnabled)

	def begin = tm.begin("adhoc")
	def begin(name: String) = tm.begin(name)
	def commit(implicit xact:Transaction) = tm.commit
	def rollback(implicit xact:Transaction) = tm.rollback

	/**
	 * This method should retun the list of committed TPC-C commands
	 * by the serialization order
	 */
	override def getListOfCommittedCommands: Seq[TpccCommand] = {
		if(tm.allCommittedXacts.isEmpty) {
			if(isUnitTestEnabled)
				throw new RuntimeException("No transaction is committed")
			else
				throw new RuntimeException("No committed transaction is collected because isUnitTestEnabled = " + isUnitTestEnabled)
		} else {
			tm.allCommittedXacts/*.filter(_.committed)*/.sortWith(_.commitTS < _.commitTS).map(_.command)
		}
	}

	override def testSpecialDsUsed = MVCCTpccTableV4.testSpecialDsUsed

	def onInsert_NewOrder(no_o_id:Int, no_d_id:Int, no_w_id:Int)(implicit xact:Transaction) = {
		tm.newOrderTbl += ((no_o_id, no_d_id, no_w_id), Tuple1(true))
	}

	def onDelete_NewOrder(newOrder: DeltaVersion[NewOrderTblKey,NewOrderTblValue])(implicit xact:Transaction) = {
		tm.newOrderTbl -= newOrder
	}

    /*Func*/ def findFirstNewOrder(no_w_id_input:Int, no_d_id_input:Int)(implicit xact:Transaction) = {
      var first_no_o_id = Integer.MAX_VALUE
      var first_newOrder:Option[DeltaVersion[NewOrderTblKey,NewOrderTblValue]] = None
      tm.newOrderTbl.slice(0, (no_d_id_input, no_w_id_input)).foreachEntry { newOrder => newOrder.getKey.getKey match { case (no_o_id,_,_) =>
          if(no_o_id <= first_no_o_id) {
            first_no_o_id = no_o_id
            first_newOrder = Some(newOrder.getKey)
          }
        }
      }
      first_newOrder
    }

	def onInsert_HistoryTbl(h_c_id:Int, h_c_d_id:Int, h_c_w_id:Int, h_d_id:Int, h_w_id:Int, h_date:Date, h_amount:Float, h_data:String)(implicit xact:Transaction) = {
		tm.historyTbl += ((h_c_id,h_c_d_id,h_c_w_id,h_d_id,h_w_id,roundDate(h_date),h_amount,h_data), Tuple1(true))
	}

	def onInsert_Item(i_id:Int, i_im_id:Int, i_name:String, i_price:Float, i_data:String)(implicit xact:Transaction) = {
		tm.itemPartialTbl += (i_id, (/*i_im_id,*/i_name,i_price,i_data))
	}

	/*Func*/ def findItem(item_id:Int)(implicit xact:Transaction) = {
		tm.itemPartialTbl.getEntry(item_id)
	}

	def onInsert_Order(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int, o_entry_d:Date, o_carrier_id:Option[Int], o_ol_cnt:Int, o_all_local:Boolean)(implicit xact:Transaction) = {
		tm.orderTbl += ((o_id,o_d_id,o_w_id), (o_c_id,o_entry_d,o_carrier_id,o_ol_cnt,o_all_local))
	}

	/*Func*/ def findMaxOrder(o_w_id_arg:Int, o_d_id_arg:Int, c_id_arg:Int)(implicit xact:Transaction) = {
		var max_o_id = -1
		tm.orderTbl.slice(0,(o_d_id_arg,o_w_id_arg, c_id_arg)).foreach { case ((o_id,_,_), (_,_,_,_,_)) =>
			if(o_id > max_o_id) {
				max_o_id = o_id
			}
		}
		max_o_id
	}

	/*Func*/ def findOrder(max_o_id:Int, o_w_id_arg:Int, o_d_id_arg:Int)(implicit xact:Transaction) = {
		tm.orderTbl((max_o_id,o_d_id_arg,o_w_id_arg))
	}

	def onUpdate_Order_forDelivery(o_id:Int, o_d_id:Int, o_w_id:Int, o_c_id:Int/*, o_entry_d:Date*/, o_carrier_id:Option[Int]/*, o_ol_cnt:Int, o_all_local:Boolean*/)(implicit xact:Transaction) = {
		tm.orderTbl.update((o_id,o_d_id,o_w_id),(currentVal/*:(Int, java.util.Date, Option[Int], Int, Boolean))*/ => ((o_c_id,currentVal._2,o_carrier_id,currentVal._4,currentVal._5))))
	}

	def onUpdate_Order_byFunc(o_id:Int, o_d_id:Int, o_w_id:Int, updateFunc:DeltaVersion[OrderTblKey,OrderTblValue] => OrderTblValue)(implicit xact:Transaction) = {
		tm.orderTbl.updateEntry((o_id,o_d_id,o_w_id),updateFunc)
	}

	def onInsert_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		tm.warehouseTbl += (w_id, (w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse(w_id:Int, w_name:String, w_street_1:String, w_street_2:String, w_city:String, w_state:String, w_zip:String, w_tax:Float, w_ytd:Double)(implicit xact:Transaction) = {
		tm.warehouseTbl(w_id) = ((w_name,w_street_1,w_street_2,w_city,w_state,w_zip,w_tax,w_ytd))
	}

	def onUpdate_Warehouse_byFunc(w_id:Int, updateFunc:DeltaVersion[WarehouseTblKey,WarehouseTblValue] => WarehouseTblValue)(implicit xact:Transaction) = {
		tm.warehouseTbl.updateEntry(w_id,updateFunc)
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

	def onUpdate_District_byFunc(d_id:Int, d_w_id:Int, updateFunc:DeltaVersion[DistrictTblKey,DistrictTblValue] => DistrictTblValue)(implicit xact:Transaction) = {
		tm.districtTbl.updateEntry((d_id,d_w_id), updateFunc)
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
    /*Func*/ def orderLineTblSliceEntry[P](part:Int, partKey:P, f: DeltaVersion[(Int,Int,Int,Int),(Int,Int,Option[Date],Int,Float,String)] => Unit)(implicit xact:Transaction) = {
		tm.orderLineTbl.slice(0, partKey).foreachEntry( e => f(e.getKey) )
    }

    def onInsertCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      tm.customerTbl += ((c_id,c_d_id,c_w_id), (c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
      var w_tax = 0f
      w_tax = tm.warehouseTbl(c_w_id)._7
      // tm.customerWarehouseFinancialInfoMap += ((c_id,c_d_id,c_w_id), (c_discount, c_last, c_credit, w_tax))
    }

    /*Func*/ def findCustomerWarehouseFinancialInfo(w_id:Int, d_id:Int, c_id:Int)(implicit xact:Transaction) = {
      // tm.customerWarehouseFinancialInfoMap(c_id,d_id,w_id)
      (tm.customerTbl.getEntry((c_id,d_id,w_id)), tm.warehouseTbl.getEntry(w_id))
    }

    def onUpdateCustomer(c_id: Int, c_d_id: Int, c_w_id: Int, c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
      tm.customerTbl((c_id,c_d_id,c_w_id)) = ((c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment,c_payment_cnt,c_delivery_cnt,c_data))
    }

    def onUpdateCustomer_byFunc(c_id: Int, c_d_id: Int, c_w_id: Int, updateFunc:DeltaVersion[CustomerTblKey,CustomerTblValue] => CustomerTblValue)(implicit xact:Transaction) = {
      tm.customerTbl.updateEntry((c_id,c_d_id,c_w_id),updateFunc)
    }

    def onUpdateCustomer_byEntry(c: DeltaVersion[CustomerTblKey,CustomerTblValue], c_first:String, c_middle:String, c_last:String, c_street_1:String, c_street_2:String, c_city:String, c_state:String, c_zip:String, c_phone:String, c_since:Date, c_credit:String, c_credit_lim:Float, c_discount:Float, c_balance:Float, c_ytd_payment:Float, c_payment_cnt:Int, c_delivery_cnt:Int, c_data:String)(implicit xact:Transaction) = {
    	c.setEntryValue((c_first,c_middle,c_last,c_street_1,c_street_2,c_city,c_state,c_zip,c_phone,c_since,c_credit,c_credit_lim,c_discount,c_balance,c_ytd_payment/*+h_amount*/,c_payment_cnt/*+1*/,c_delivery_cnt,c_data))
    }

    def onInsertStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      tm.stockTbl += ((s_i_id,s_w_id), (s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock(s_i_id:Int, s_w_id:Int, s_quantity:Int, s_dist_01:String, s_dist_02:String, s_dist_03:String, s_dist_04:String, s_dist_05:String, s_dist_06:String, s_dist_07:String, s_dist_08:String, s_dist_09:String, s_dist_10:String, s_ytd:Int, s_order_cnt:Int, s_remote_cnt:Int, s_data:String)(implicit xact:Transaction) = {
      tm.stockTbl((s_i_id,s_w_id)) = ((s_quantity, s_dist_01,s_dist_02,s_dist_03,s_dist_04,s_dist_05,s_dist_06,s_dist_07,s_dist_08,s_dist_09,s_dist_10,s_ytd,s_order_cnt,s_remote_cnt,s_data))
    }

    def onUpdateStock_byFunc(s_i_id:Int, s_w_id:Int, updateFunc:DeltaVersion[StockTblKey,StockTblValue] => StockTblValue)(implicit xact:Transaction) = {
      tm.stockTbl.updateEntry((s_i_id,s_w_id), updateFunc)
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

    override def getAllMapsInfoStr:String = {
    	("forcedRollback => " + forcedRollback + "\n") +
		("failedValidation => " + failedValidation + "\n") +
		("failedConcurrentUpdate => " + failedConcurrentUpdate + "\n") +
		("failedConcurrentInsert => " + failedConcurrentInsert + "\n")
		//+ (",%d\n,%d\n,%d\n,%d\n".format(forcedRollback,failedValidation,failedConcurrentUpdate,failedConcurrentInsert))
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
