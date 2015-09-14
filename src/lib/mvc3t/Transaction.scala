package ddbt.lib.mvc3t

import scala.collection.mutable._
import ddbt.lib.util.XactCommand

import TransactionManager._

/**
 * Transaction is the encapsulation of all the data required for
 * running a transaction program in a concurrent execution
 * environment.
 */
class Transaction(val tm: TransactionManager, val name: String, var startTS: Long, var xactId: Long, var committed:Boolean=false) {
	val initialStartTS = startTS

	val DEFAULT_UNDO_BUFFER_SIZE = 64

	val undoBuffer = new HashSet[DeltaVersion[_,_]]()
	// var predicates = new MutableMap[Table, ListBuffer[Predicate[_,_]]]
	var predicates = ListBuffer[Predicate[_,_]]()
	// var predicateResultCache = new MutableMap[Predicate[_,_], AnyRef]
	// val closureTransitions = new ListBuffer[ClosureTransition]()

	var command: XactCommand = null

	def setCommand(c: XactCommand) {
		command = c
	}

	def commitTS = xactId

	def transactionId = initialStartTS //if(xactId < TransactionManager.TRANSACTION_ID_GEN_START) xactId else (xactId - TransactionManager.TRANSACTION_ID_GEN_START)

	def isCommitted = (xactId < TransactionManager.TRANSACTION_ID_GEN_START)

	def isReadOnly = undoBuffer.isEmpty

	def addPredicate(p:Predicate[_,_], parents:List[Predicate[_,_]]) = {
		if (parents == Nil) {
			// val pList = predicates.getNullOnNotFound(p.tbl.tblName)
			// if(pList != null) pList += p
			// else {
			// 	val pBuf = new ListBuffer[Predicate[_,_]]
			// 	pBuf += p
			// 	predicates += (p.tbl.tblName, pBuf)
			// }
			predicates += p
		} else p.dependsOn(parents)
	}

	def moveCommittedRecordsToFront {
		undoBuffer.foreach(dv => if(!dv.isRemoved) dv.moveToFront(this))
	}

	// def findPredicateCachedResult(p:Predicate[_,_]):Option[AnyRef] = {
	// 	val res = predicateResultCache.getNullOnNotFound(p)
	// 	if(res == null) None else Some(res)
	// }

	// def cachePredicateResult(p:Predicate[_,_], res: AnyRef) {
	// 	predicateResultCache += (p, res)
	// }

	def commit = tm.commit(this)
	def rollback = tm.rollback(this)

	override def toString = "T"+transactionId+"<"+name+">"+(if(startTS != initialStartTS) "(with startTS = %d)".format(startTS) else "")+(if(isCommitted) "(C)" else "")

	def findConflictsWith(concurrentXacts: ListBuffer[Transaction]):(List[Predicate[_,_]],Long) = {
		var newTS = TRANSACTION_STRAT_TS_GEN_START
		if(!predicates.isEmpty) {
			concurrentXacts.foreach { t =>
				if(!allPredicatesHaveConflict) {
					if(markConflictsWith(t)) {
						if(t.commitTS > newTS) newTS = t.commitTS
					}
				}
				// if(!predicates.isEmpty) {
				// 	// val conflictsWithT = 
				// 	// predicates.foreach { p =>
				// 	// 	if(p.matches())
				// 	// }
				// 	val cList = matchesPredicates(t.undoBuffer)
				// 	if(cList ne Nil) {
				// 		if(t.commitTS > newTS) newTS = t.commitTS
				// 		conflicts = conflicts ++ cList
				// 	}
				// 	// t.undoBuffer.foreach { dv =>
				// 	// 	debug("\t\tchecking whether " + dv + " (in " + t + ") matches predicates in " + xact)
				// 	// 	conflicts = conflicts ++ xact.matchesPredicates(dv)
				// 	// }
				// }
			}
		}
		var conflicts = List[Predicate[_,_]]()

		val predsQueue = new Queue[Predicate[_,_]]()

		// predsQueue ++= predicates
		// while(!predsQueue.isEmpty) {
		// 	val p = predsQueue.dequeue
		// 	if(p.hasConflict) p match {
		// 		case pi@InsertSingleRecPredicate(_,_) => pi.outputVersion.remove(this)
		// 		case pu@UpdateSingleRecPredicate(_,_) => pu.outputVersion.remove(this)
		// 		case pd@DeleteSingleRecPredicate(_,_,_) => pd.outputVersion.remove(this)
		// 		case _ => ()
		// 	}
		// 	predsQueue ++= p.children
		// }
		// predsQueue.clear

		predicates.foreach( p => if(!p.hasConflict) predsQueue += p else conflicts = p :: conflicts )
		while(!predsQueue.isEmpty) {
			val p = predsQueue.dequeue
			p.children = p.children.filter { c =>
				if(c.hasConflict) {
					conflicts = c :: conflicts
					false
				} else {
					predsQueue += c
					true
				}
			}
		}
		//Remove the predicates that had conflict
		predicates = predicates.filter(p => !p.hasConflict)
		(conflicts,newTS)
	}

	def allPredicatesHaveConflict = predicates.forall(p => p.hasConflict)

	def markConflictsWith(t:Transaction): Boolean = {
		var isConflicting = false
		val predsQueue = new Queue[Predicate[_,_]]()
		predicates.foreach( p => if(!p.hasConflict) predsQueue += p )
		while(!predsQueue.isEmpty) {
			val p = predsQueue.dequeue
			if(p.matches(t.undoBuffer)) {
				p.hasConflict = true
				debug(this + " conflicts with " + t + " on " + p)(this)
				isConflicting = true
			} else if(p.children ne Nil) {
				predsQueue ++= p.children
			}
		}
		isConflicting
	}
	// def matchesPredicates(dv:Set[DeltaVersion[_,_]]): List[Predicate] = {
	// 	predicates.foreach { case (tbl, preds) =>

	// 	}
	// 	if(dv == null) println("dv is null in matchesPredicates")
	// 	val preds = predicates.getNullOnNotFound(dv.getTable)
	// 	var res = Set[Conflict]()
	// 	if(preds != null) dv.op match {
	// 		case INSERT_OP => {
	// 			res = res ++ imageMatchesPredicates(dv, preds)
	// 		}
	// 		case DELETE_OP => {
	// 			val nextDv = dv.next
	// 			if(nextDv == null) throw new RuntimeException("A deleted version should always have the next pointer.")
	// 			else {
	// 				res = res ++ imageMatchesPredicates(nextDv, preds)
	// 			}
	// 		}
	// 		case UPDATE_OP => {
	// 			val nextDv = dv.next
	// 			// An updated version after GC might not have a next pointer
	// 			// if(nextDv == null) throw new RuntimeException("An updated version should always have the next pointer. The update version is " + dv + " and dv.next = " + dv.next + " and dv.prev = " + dv.prev)
	// 			// else 
	// 			res = res ++ imageMatchesPredicates(dv, preds)
	// 			if(nextDv ne null) res = res ++ imageMatchesPredicates(nextDv, preds)
	// 		}
	// 		case x => throw new IllegalStateException("DeltaVersion operation " + x + " does not exist.")
	// 	}
	// 	res
	// }

	// def matchesPredicates(dv:DeltaVersion[_,_]): Set[Conflict] = {
	// 	if(dv == null) println("dv is null in matchesPredicates")
	// 	val preds = predicates.getNullOnNotFound(dv.getTable)
	// 	var res = Set[Conflict]()
	// 	if(preds != null) dv.op match {
	// 		case INSERT_OP => {
	// 			res = res ++ imageMatchesPredicates(dv, preds)
	// 		}
	// 		case DELETE_OP => {
	// 			val nextDv = dv.next
	// 			if(nextDv == null) throw new RuntimeException("A deleted version should always have the next pointer.")
	// 			else {
	// 				res = res ++ imageMatchesPredicates(nextDv, preds)
	// 			}
	// 		}
	// 		case UPDATE_OP => {
	// 			val nextDv = dv.next
	// 			// An updated version after GC might not have a next pointer
	// 			// if(nextDv == null) throw new RuntimeException("An updated version should always have the next pointer. The update version is " + dv + " and dv.next = " + dv.next + " and dv.prev = " + dv.prev)
	// 			// else 
	// 			res = res ++ imageMatchesPredicates(dv, preds)
	// 			if(nextDv ne null) res = res ++ imageMatchesPredicates(nextDv, preds)
	// 		}
	// 		case x => throw new IllegalStateException("DeltaVersion operation " + x + " does not exist.")
	// 	}
	// 	res
	// }

	// def imageMatchesPredicates(dv:DeltaVersion[_,_], preds: ListBuffer[Predicate[_,_]]): Set[Conflict] = {
	// 	var res = Set[(Predicate[_,_], DeltaVersion[_,_])]()
	// 	preds.foreach { p => if((p.tbl == dv.getMap) && p.matches(dv)) {
	// 			debug("\t\t\t matched " + p + "!")(this)
	// 			import scala.language.existentials
	// 			res = res + ((p.asInstanceOf[Predicate[_,Product]], dv.asInstanceOf[DeltaVersion[_,Product]]))
	// 			// return true
	// 		}
	// 	}
	// 	res
	// }

	// def addClosureTransition(ct: ClosureTransition) {
	// 	closureTransitions += ct
	// }
}
