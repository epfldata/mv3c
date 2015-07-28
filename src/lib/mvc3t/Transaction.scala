package ddbt.lib.mvc3t

import scala.collection.mutable._
import ddbt.lib.util.XactCommand

import TransactionManager._

/**
 * Transaction is the encapsulation of all the data required for
 * running a transaction program in a concurrent execution
 * environment.
 */
class Transaction(val tm: TransactionManager, val name: String, val startTS: Long, var xactId: Long, var committed:Boolean=false) {
	val DEFAULT_UNDO_BUFFER_SIZE = 64

	val undoBuffer = new HashSet[DeltaVersion[_,_]]()
	var predicates = new MutableMap[Table, HashSet[Predicate[_,_]]]
	var predicateResultCache = new MutableMap[Predicate[_,_], AnyRef]
	val closureTransitions = new ListBuffer[ClosureTransition]()

	var command: XactCommand = null

	def setCommand(c: XactCommand) {
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
