package ddbt.lib.mvc3t

import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicBoolean
import scala.collection.mutable._
import scala.concurrent._
import ExecutionContext.Implicits.global

import TransactionManager._

/**
 * TransactionManager class has the responsibility of managing
 * all the concurrent transactions in the system under a specific
 * concurrency control mechanism (that is MVC3T).
 */
object TransactionManager {
	val TRANSACTION_ID_GEN_START = (1L << 63)
	val TRANSACTION_STRAT_TS_GEN_START = 1L

	type Table = String
	type MutableMap[K,V] = ddbt.lib.shm.SHMap[K,V]
	// type Conflict[K,V<:Product] = (Predicate[K,V],DeltaVersion[K,V])
	// type Conflict = (Predicate[_,_],Transaction)
  
	type Operation = Int
	val INSERT_OP:Operation = 1 << 0 // 001
	val DELETE_OP:Operation = 1 << 1 // 010
	val UPDATE_OP:Operation = 1 << 2 // 100

	val DEBUG = false
	val ERROR = true
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
	//List of recently committed transactions consists of
	//the most recent committed transaction in its head
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
				// recentlyCommittedXacts.synchronized {
				// 	//prepend the transaction to the recently commmitted transactions list
				// 	recentlyCommittedXacts.+=:(xact)
				// 	if(isUnitTestEnabled) allCommittedXacts += xact
				// }
				debug("(read-only) commit succeeded (with commitTS = %d)".format(xact.commitTS))
				true
			} else if(!validate) {
				debug("commit failed: transaction validation failed! We have to roll it back...")
				rollback
				false
			} else {
				xact.moveCommittedRecordsToFront
				activeXacts.synchronized {
					xact.xactId = startAndCommitTimestampGen.getAndIncrement()
					activeXacts -= xactId
				}
				debug("\twith undo buffer(%d) = %%s".format(xact.undoBuffer.size, xact.undoBuffer))
				recentlyCommittedXacts.synchronized {
					//prepend the transaction to the recently commmitted transactions list
					recentlyCommittedXacts.+=:(xact)
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
		if(!xact.undoBuffer.isEmpty) {
			debug("\tvalidation started")
			//1- Find all the transaction that ran concurrenctly with the current xact
			val concurrentXacts = recentlyCommittedXacts.filter(t => t.commitTS > xact.startTS)
			debug("\t\tconcurrentXacts = " + concurrentXacts)
			//2- For each concurrent transaction, check if any version created/updated/deleted
			//   produced by that, has a conflict with any predicate in the current transaction
			//   and collect the conflicting predicates
			// var conflicts = Set[Conflict]()
			// concurrentXacts.foreach { t =>
			// 	t.undoBuffer.foreach { dv =>
			// 		debug("\t\tchecking whether " + dv + " (in " + t + ") matches predicates in " + xact)
			// 		conflicts = conflicts ++ xact.matchesPredicates(dv)
			// 	}
			// }

			val (conflicts, newTS) = xact.findConflictsWith(concurrentXacts)

			if(conflicts.isEmpty) {
				//3- If there's no conflict, then there is no validation error
				debug("\tvalidation succeeded")
				true
			} else {
				//4- If there is a conflict, then we will try to resolve it
				debug("\tvalidation failed")
				debug("\tconflicts are => " + conflicts)
				debug("\tundoBuffer contains => " + xact.undoBuffer)

				//5- Find the new timestamp for the transaction, that won't
				//   introduce the current conflicts and collect the confluct predicates
				//   at the same time
				// var newTS = TRANSACTION_STRAT_TS_GEN_START
				// val conflictPreds = conflicts.map{ c =>
				// 	if(c._2.commitTS > newTS) newTS = c._2.commitTS
				// 	c._1
				// }

				//6- Set the new timestamp for the current transaction
				xact.startTS = newTS + 1

				// xact.closureTransitions.foreach{ ct =>
				// 	var activated = false
				// 	ct.handlers.foreach{ h =>
				// 		if(conflictPreds.exists( p => h.preds.contains(p))) {
				// 			debug("\thandler activated for " + h.preds)
				// 			activated = true
				// 			h.handler(ct.outputVersions._1)
				// 		} else {
				// 			debug("\th.preds = " + h.preds)
				// 		}
				// 	}
				// 	if(activated) {
				// 		ct.outputVersions._2.foreach { innerCt =>
				// 			innerCt.handlers.foreach{ h =>
				// 				debug("\t\tsub-handler activated")
				// 				h.handler(innerCt.outputVersions._1)
				// 			}
				// 		}
				// 	}
				// }
				conflicts.foreach(_.compensateAction)

				// failedValidation += 1
				// false
				true
			}
		} else {
			debug("\tno validation required for read-only xact.")
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
				// activeXacts.foreach{ a => if(a._2.startTS < oldestActiveXactStartTS) oldestActiveXactStartTS = a._2.startTS }
				oldestActiveXactStartTS = activeXacts.last._2.startTS
				// if(oldestActiveXactStartTS != activeXacts.last._2.startTS) {
				// 	throw new RuntimeException("oldestActiveXactStartTS = %d while activeXacts.last._2.startTS = %d".format(oldestActiveXactStartTS, activeXacts.last._2.startTS))
				// } else {
				// 	println("fine")
				// }
				// debug("\tactiveXacts = " + activeXacts)
				// debug("\toldestActiveXactStartTS = " + oldestActiveXactStartTS)
			}

			var reachedLimit = false
			while(!reachedLimit && !recentlyCommittedXacts.isEmpty) {
				val xact = recentlyCommittedXacts.head
				implicit val theXact = xact
				if(xact.commitTS < oldestActiveXactStartTS) {
					// debug("\tremoving xact = " + xact)
					xact.undoBuffer.foreach{ dv =>
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
					}
					recentlyCommittedXacts.synchronized {
						recentlyCommittedXacts.remove(0)
					}
				} else {
					// debug("\treached gc limit "+xact+" vs. oldestActiveXactStartTS=" + oldestActiveXactStartTS)
					reachedLimit = true
				}
			}
		}
		else {
			// debug("\tnothing to be done!")
		}
		debug("GC finished")
	}
}