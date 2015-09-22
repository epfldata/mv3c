package ddbt.lib.mvconcurrent

import scala.collection.mutable._
import ddbt.lib.util.XactCommand

import TransactionManager._

/**
 * Transaction is the encapsulation of all the data required for
 * running a transaction program in a concurrent execution
 * environment.
 */
class Transaction(val tm: TransactionManager, val name: String, val startTS: Long, var xactId: Long, var committed:Boolean=false) {
	// val DEFAULT_UNDO_BUFFER_SIZE = 64

	var undoBuffer:DeltaVersion[_,_] = null // a singly linked list of DeltaVersions, based on nextInUndoBuffer field of DeltaVersion
	//TODO can we se a simple ListBuffer instead? (the same as MVC3T)
	private[this] var predicates:Predicate = null // a singly linked list of predicates, based on next field of Predicate

	var command: XactCommand = null

	def setCommand(c: XactCommand) {
		command = c
	}

	def commitTS = xactId

	def transactionId = startTS //if(xactId < TransactionManager.TRANSACTION_ID_GEN_START) xactId else (xactId - TransactionManager.TRANSACTION_ID_GEN_START)

	def isCommitted = (xactId < TransactionManager.TRANSACTION_ID_GEN_START)

	def isReadOnly = (undoBuffer == null)

	def undoBufferSize = {
		var res = 0
		var iter = undoBuffer
		while(iter != null) {
			iter = iter.nextInUndoBuffer
		}
		res
	}
	def undoBufferToString = {
		val res = new StringBuilder("[")
		var iter = undoBuffer
		while(iter != null) {
			res.append(iter.toString)
			iter = iter.nextInUndoBuffer
			if(iter != null) {
				res.append(", ")
			}
		}
		res.append("]")
		res
	}
	def addToUndoBuffer(dv: DeltaVersion[_,_]) {
		if(undoBuffer == null) undoBuffer = dv
		else {
			dv.nextInUndoBuffer = undoBuffer
			undoBuffer = dv
		}
	}

	def addPredicate(p:Predicate) {
		if(predicates == null) predicates = p
		else {
			p.next = predicates
			predicates = p
		}
		// predicates += p
		// val pList = predicates.getOrElse(p.tbl, null)
		// if(pList != null) pList += p
		// else {
		// 	val pBuf = new HashSet[Predicate]
		// 	pBuf += p
		// 	predicates += ((p.tbl, pBuf))
		// }
	}
	def commit = tm.commit(this)
	def rollback = tm.rollback(this)

	override def toString = "T"+transactionId+"<"+name+">"

	def matchesPredicates(dv:DeltaVersion[_,_]): Boolean = {
		val preds = predicates
		if(preds != null) dv.op match {
			case INSERT_OP => if(imageMatchesPredicates(dv, preds)) return true
			case DELETE_OP => {
				val nextDv = dv.next
				if(nextDv == null) throw new RuntimeException("A deleted version should always have the next pointer.")
				else if(imageMatchesPredicates(nextDv, preds)) return true
			}
			case UPDATE_OP => {
				val nextDv = dv.next
				// An updated version after GC might not have a next pointer
				// if(nextDv == null) throw new RuntimeException("An updated version should always have the next pointer. The update version is " + dv + " and dv.next = " + dv.next + " and dv.prev = " + dv.prev)
				// else 
				if(imageMatchesPredicates(dv, preds) || ((nextDv ne null) && imageMatchesPredicates(nextDv, preds))) return true
			}
			case x => throw new IllegalStateException("DeltaVersion operation " + x + " does not exist.")
		}
		false
	}

	def imageMatchesPredicates(dv:DeltaVersion[_,_], predsHead: Predicate): Boolean = {
		var p = preds
		do {
			if(p.matches(dv)) {
				debug("\t\t\t matched " + p + "!")(this)
				return true
			}
			p = p.next
		} while(p != null)
		false
	}
}
