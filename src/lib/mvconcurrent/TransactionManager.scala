package ddbt.lib.mvconcurrent

import ddbt.lib.util.ThreadInfo
import java.util.concurrent.locks.ReentrantLock
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.ConcurrentHashMap
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
	// type AtomicLongType = java.util.concurrent.atomic.AtomicLong
	type AtomicLongType = ddbt.lib.util.BackOffAtomicLong

	val TRANSACTION_ID_GEN_START = (1L << 32)
	val TRANSACTION_STRAT_TS_GEN_START = 1L

	type Table = String
	type MutableMap[K,V] = ddbt.lib.shm.SHMap[K,V]
  
	type Operation = Int
	val INSERT_OP:Operation = 1
	val DELETE_OP:Operation = 2
	val UPDATE_OP:Operation = 3

	val DEBUG = false
	val ERROR = true
	val DISABLE_GC = false
	val PARALLEL_GC = false

	//@inline //TODO FIX IT: it should be inlined for production use
	def forceDebug(msg: => String)(implicit xact:Transaction) = println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def debug(msg: => String)(implicit xact:Transaction) = if(DEBUG) println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(msg: => String)(implicit xact:Transaction): Unit = if(ERROR) System.err.println("Thread"+Thread.currentThread().getId()+" :> "+xact+": " + msg)
	def error(e: Throwable)(implicit xact:Transaction): Unit = if(ERROR) { error(e.toString); e.getStackTrace.foreach(st => error(st.toString))}
}
class TransactionManager(numConn:Int, isUnitTestEnabled: =>Boolean) {
	@volatile var forcedRollback = 0 // is the rollback requested by transaction program (and it’s a part of benchmark)
	@volatile var failedValidation = 0 // is the rollback due to the validation failure
	@volatile var failedConcurrentUpdate = 0 // is the rollback due to a concurrent update (for the same object), which is not allowed in the reference impl.
	@volatile var failedConcurrentInsert = 0 // is the rollback due to a concurrent insert (for the same object), which is not allowed in the reference impl.

	// def clear = {
	// 	forcedRollback = 0
	// 	failedValidation = 0
	// 	failedConcurrentUpdate = 0
	// 	failedConcurrentInsert = 0
	// }

	val transactionIdGen = new AtomicLongType(TransactionManager.TRANSACTION_ID_GEN_START)
	val startAndCommitTimestampGen = new AtomicLongType(TransactionManager.TRANSACTION_STRAT_TS_GEN_START)
	val isGcActive = new AtomicBoolean(DISABLE_GC)

	val activeXacts = new Array[AtomicReference[Transaction]](numConn)

	{
		for(i <- 0 until numConn) {
			activeXacts(i) = new AtomicReference[Transaction]
		}
	}

	//List of recently committed transactions consists of
	//the most recent committed transaction in its head
	val recentlyCommittedXacts = new AtomicReference[List[Transaction]](List[Transaction]())
	// val allCommittedXacts = new ListBuffer[Transaction]()

	val commitLock = new ReentrantLock

	def begin(name: String, readOnly: Boolean=false)(implicit tInfo: ThreadInfo): Transaction = {
		var xact:Transaction = null
		if(readOnly) {
			val startTS = startAndCommitTimestampGen.get
			val xactId = startTS
			xact = new Transaction(this, name, startTS, xactId, readOnly, tInfo)
		} else {
			val startTS = startAndCommitTimestampGen.getAndIncrement
			val xactId = transactionIdGen.getAndIncrement
			xact = new Transaction(this, name, startTS, xactId, readOnly, tInfo)
			activeXacts(tInfo.tid).set(xact)
		}
		// debug("started at %d".format(startTS))
		xact
	}

	def commit(implicit xact:Transaction):Boolean = /*this.synchronized*/ {
		// debug("commit started")
		val xactThreadId = xact.tInfo.tid
		if(xact.isDefinedReadOnly) {
			true
		} else if(xact.isReadOnly) {
			// we assume that a Transaction object cannot be instantiated
			// without going through the TransactionManager
			// if(activeXacts.get(xactThreadId) == null) {
			// 	// debug("commit failed: transaction does not exist! We have to roll it back...")
			// 	// rollback
			// 	false
			// } else {
				//Note: removing the current xact from the activeXacts is unnecessary
				//under the assumption that the current thread will quickly
				//do the next transaction, otherwise it should be done
				// activeXacts.set(xactThreadId, null)
				xact.xactId = xact.startTS
				// A read-only transaction does not have to be added to the recentlyCommittedXacts
				// recentlyCommittedXacts.synchronized {
				// 	//appenda the transaction to the recently commmitted transactions list
				// 	recentlyCommittedXacts += xact
				// 	if(isUnitTestEnabled) allCommittedXacts += xact
				// }
				// debug("(read-only) commit succeeded (with commitTS = %d)".format(xact.commitTS))
				true
			// }
		} else {
			commitLock.lock
			var alreadyUnlocked = false
			try {
				if(!validate) {
					commitLock.unlock
					alreadyUnlocked = true
					debug("commit failed: transaction validation failed! We have to roll it back...")
					rollback
					false
				} else {
					// we assume that a Transaction object cannot be instantiated
					// without going through the TransactionManager
					// if(activeXacts.get(xactThreadId) == null) {
					// 	// debug("commit failed: transaction does not exist! We have to roll it back...")
					// 	commitLock.unlock
					// 	alreadyUnlocked = true
					// 	rollback
					// 	false
					// } else {
						//Note: removing the current xact from the activeXacts is unnecessary
						//under the assumption that the current thread will quickly
						//do the next transaction, otherwise it should be done
						// activeXacts.set(xactThreadId, null)
						xact.xactId = startAndCommitTimestampGen.getAndIncrement
						// debug("\twith undo buffer(%d) = %%s".format(xact.undoBufferSize, xact.undoBufferToString))
						//appends the transaction to the recently commmitted transactions list
						var currRecentlyCommittedXacts = recentlyCommittedXacts.get
						while(!recentlyCommittedXacts.compareAndSet(currRecentlyCommittedXacts, currRecentlyCommittedXacts :+ xact)) {
							currRecentlyCommittedXacts = recentlyCommittedXacts.get
						}
						commitLock.unlock
						alreadyUnlocked = true
						// debug("commit succeeded (with commitTS = %d)".format(xact.commitTS))
						// if(isUnitTestEnabled) allCommittedXacts += xact
						garbageCollect
						true
					// }
				}
			} finally {
				if(!alreadyUnlocked) commitLock.unlock
			}
		}
	}
	//TODO: should be implemented completely
	// missing:
	//  - validation can run in parallel for the transactions that are in this stage (we do not have to synchronize it)
	//  - attribute-level validation should be supported
	def validate(implicit xact:Transaction): Boolean = {
		// debug("\tvalidation started")
		val concurrentXacts = recentlyCommittedXacts.get.filter(t => t.commitTS > xact.startTS)
		// debug("\t\tconcurrentXacts = " + concurrentXacts)
		concurrentXacts.foreach { t =>
			var dv = t.undoBuffer
			while(dv != null) {
				// debug("\t\tchecking whether " + dv + " (in " + t + ") matches predicates in " + xact)
				if(xact.matchesPredicates(dv)) {
					// debug("\tvalidation failed")
					failedValidation += 1
					return false
				}
				else {
					// debug("\t\t\t did not match!")
				}
				dv = dv.nextInUndoBuffer
			}
		}
		// debug("\tvalidation succeeded")
		true
	}

	def rollback(implicit xact:Transaction) = {
		// debug("rollback started")
		//Note: removing the current xact from the activeXacts is unnecessary
		//under the assumption that the current thread will quickly
		//do the next transaction, otherwise it should be done
		// activeXacts.set(xact.tInfo.tid, null)
		// debug("\twith undo buffer(%d) = %%s".format(xact.undoBufferSize, xact.undoBufferToString))
		var dv = xact.undoBuffer
		while(dv != null) {
			// debug("\t\tremoved => (" + dv.getTable+"," + dv.getKey + ", " + dv + ")")
			dv.remove
			dv = dv.nextInUndoBuffer
		}
		// debug("rollback finished")
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
		// debug("GC started")
		var currRecentlyCommittedXacts = recentlyCommittedXacts.get
		if(!currRecentlyCommittedXacts.isEmpty) {
			var oldestActiveXactStartTS = TransactionManager.TRANSACTION_ID_GEN_START

			val len = activeXacts.length
			var i = 0
			while(i < len) {
				val t = activeXacts(i).get
				if((null ne t) && (xact ne t /*because we did not remove current xact from active xact list*/) && t.startTS < oldestActiveXactStartTS) {
					oldestActiveXactStartTS = t.startTS
				}
				i += 1
			}
			// debug("\tactiveXacts = " + activeXacts)
			// debug("\toldestActiveXactStartTS = " + oldestActiveXactStartTS)

			var reachedLimit = false
			while(!reachedLimit && !currRecentlyCommittedXacts.isEmpty) {
				implicit val xact = currRecentlyCommittedXacts.head
				if(xact.commitTS < oldestActiveXactStartTS) {
					// debug("\tremoving xact = " + xact.transactionId)
					var dv = xact.undoBuffer
					while(dv != null) {
						val nextDV = dv.next
						dv.next = null
						if(nextDV != null) {
							// debug("\t\tremoved version => (" + nextDV.getTable+"," + nextDV.getKey + ", " + nextDV + ")")
							nextDV.gcRemove
						}
						else if(dv.op == DELETE_OP){
							// debug("\t\tcould not remove version because of no next => (" + nextDV.getTable+"," + nextDV.getKey + ", " + nextDV + ")")
						}
						if(dv.isDeleted) {
							// debug("\t\tand also removed itself => (" + nextDV.getTable+"," + dv.getKey + ", " + dv + ")")
							dv.remove
						}
						else if(nextDV != null){
							// debug("\t\t, and current version is => (" + dv.getTable+"," + dv.getKey + ", " + dv + ")")
						}
						dv = dv.nextInUndoBuffer
					}
					// recentlyCommittedXacts.synchronized {
					// here is the only place that we are removing from the recentlyCommittedXacts
					// and only one thread can be active here, so no synchronization is required
						while(!recentlyCommittedXacts.compareAndSet(currRecentlyCommittedXacts, currRecentlyCommittedXacts.tail)) {
							currRecentlyCommittedXacts = recentlyCommittedXacts.get
						}
						currRecentlyCommittedXacts = currRecentlyCommittedXacts.tail
					// }
				} else {
					// debug("\treached gc limit T"+xact.transactionId+" vs. oldestActiveXactStartTS=" + oldestActiveXactStartTS)
					reachedLimit = true
				}
			}
		}
		else {
			// debug("\tnothing to be done!")
		}
		// debug("GC finished")
	}
}
