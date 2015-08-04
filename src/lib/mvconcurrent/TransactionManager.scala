package ddbt.lib.mvconcurrent

import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicBoolean
import scala.collection.mutable._
import scala.concurrent._
import ExecutionContext.Implicits.global

import TransactionManager._

/**
 * TransactionManager class has the responsibility of managing
 * all the concurrent transactions in the system under a specific
 * concurrency control mechanism (that is MVCC).
 */
object TransactionManager {
	val TRANSACTION_ID_GEN_START = (1L << 32)
	val TRANSACTION_STRAT_TS_GEN_START = 1L

	type Table = String
	type MutableMap[K,V] = ddbt.lib.shm.SHMap[K,V]
  
	type Operation = Int
	val INSERT_OP:Operation = 1
	val DELETE_OP:Operation = 2
	val UPDATE_OP:Operation = 3

	val DEBUG = false
	val ERROR = false
	val DISABLE_GC = false
	val PARALLEL_GC = true

	//@inline //TODO FIX IT: it should be inlined for production use
	def forceDebug(msg: => String)(implicit xact:Transaction) = println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def debug(msg: => String)(implicit xact:Transaction) = if(DEBUG) println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(msg: => String)(implicit xact:Transaction): Unit = if(ERROR) System.err.println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(e: Throwable)(implicit xact:Transaction): Unit = if(ERROR) { error(e.toString); e.getStackTrace.foreach(st => error(st.toString))}
}
class TransactionManager(isUnitTestEnabled: =>Boolean) {
	var forcedRollback = 0 // is the rollback requested by transaction program (and it’s a part of benchmark)
	var failedValidation = 0 // is the rollback due to the validation failure
	var failedConcurrentUpdate = 0 // is the rollback due to a concurrent update (for the same object), which is not allowed in the reference impl.
	var failedConcurrentInsert = 0 // is the rollback due to a concurrent insert (for the same object), which is not allowed in the reference impl.

	// def clear = {
	// 	forcedRollback = 0
	// 	failedValidation = 0
	// 	failedConcurrentUpdate = 0
	// 	failedConcurrentInsert = 0
	// }

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
		concurrentXacts.foreach { t =>
			t.undoBuffer.foreach { dv =>
				debug("\t\tchecking whether " + dv + " (in " + t + ") matches predicates in " + xact)
				if(xact.matchesPredicates(dv)) {
					debug("\tvalidation failed")
					failedValidation += 1
					return false
				}
				else {
					debug("\t\t\t did not match!")
				}
			}
		}
		debug("\tvalidation succeeded")
		true
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
}
