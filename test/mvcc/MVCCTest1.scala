package ddbt.tpcc.lib.mvconcurrent

import ddbt.tpcc.tx._
import MVCCTpccTableV3._
import org.scalatest._
import MVCCTestParams._

object MVCCTestParams {
  val disableGC = MVCCTpccTableV3.DISABLE_GC
}
class MVCCSpec1 extends FlatSpec with Matchers {

  object Key {
    def apply(_1:Int, _2:String) = new Key(_1,_2)
    implicit def orderingKey[A <: Key]: Ordering[A] = Ordering.by(e => (e._1, e._2))
  }

  class Key(val _1:Int, val _2:String) {
    override def equals(o: Any) = {
      if(!o.isInstanceOf[Key]) false
      else {
        val other = o.asInstanceOf[Key]
        (_1,_2) == (other._1,other._2)
      }
    }

    override def hashCode = (_1,_2).hashCode

    override def toString = (_1,_2).toString
  }

  val tm = new TransactionManager
  tm.isGcActive.set(disableGC)
  val tbl = new ConcurrentSHMapMVCC[Key,(Int,String)]("Test1Map", (k:Key,v:(Int,String)) => k._1 )

  "A MVCC table" should "be able to insert an element and store it properly (before reaching threshold" in {
    ConcurrentSHMapMVCC.TREEIFY_THRESHOLD should be (8) //we assume that threshold is 8
    implicit val xact = tm.begin("T1")
    tbl += (Key(1,"z"),(2,"a"))
    tbl += (Key(1,"x"),(5,"e"))
    tbl += (Key(2,"z"),(6,"f"))
    tbl += (Key(2,"y"),(7,"g"))
    tbl.get(Key(1,"z")) should be ((2,"a"))
    tbl.size should be (4)
    xact.commit
  }

  it should "persistent an inserted element after the commit" in {
    implicit val xact = tm.begin("T2")
    tbl.get(Key(1,"z")) should be ((2,"a"))
    tbl.size should be (4)
    xact.commit
  }

  it should "throw an exception on inserting an existing key" in {
    implicit val xact = tm.begin("T3")
    a [MVCCRecordAlreayExistsException] should be thrownBy {
      tbl += (Key(1,"z"),(3,"b"))
    }
    tbl.size should be (4)
    xact.rollback
  }

  it should "throw an exception on inserting an already inserted entry even by current transaction" in {
    implicit val xact = tm.begin("T4")
    tbl += (Key(1,"y"),(4,"c"))
    a [MVCCRecordAlreayExistsException] should be thrownBy {
      tbl += (Key(1,"y"),(5,"d"))
    }
    tbl.size should be (5)
    xact.rollback
  }

  it should "remove the stale versions produced by aborted xacts" in {
    implicit val xact = tm.begin("T5")
    tbl.get(Key(1,"y")) should be (null)
    tbl.size should be (4)
    xact.commit
  }

  it should "create the correct slices (after insertions)" in {
    implicit val xact = tm.begin("T6")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (7)
    tbl.size should be (4)
    xact.commit
  }

  it should "execute the foreach over all visible elements (1)" in {
    implicit val xact = tm.begin("T7")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (20)
    xact.commit
  }

  it should "be able to delete an element" in {
    implicit val xact = tm.begin("T8")
    tbl -= (Key(1,"z"))
    tbl.get(Key(1,"z")) should be (null)
    tbl.size should be (4)
    xact.commit
  }

  it should "create the correct slices (after deletion)" in {
    implicit val xact = tm.begin("T9")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (5)
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit
  }

  it should "execute the foreach over all visible elements (2)" in {
    implicit val xact = tm.begin("T10")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (18)
    xact.commit
  }

  it should "be able to re-insert an already deleted element" in {
    implicit val xact = tm.begin("T11")
    tbl += (Key(1,"z"), (22,"aa"))
    tbl.get(Key(1,"z")) should be ((22,"aa"))
    tbl.size should be (4)
    xact.commit
  }

  it should "execute the foreach over all visible elements (3)" in {
    implicit val xact = tm.begin("T12")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (40)
    xact.commit
  }

  it should "be able to update an already inserted element" in {
    implicit val xact = tm.begin("T13")
    tbl(Key(1,"z")) = (222,"aaa")
    tbl.get(Key(1,"z")) should be ((222,"aaa"))
    tbl.size should be (4)
    xact.commit
  }

  it should "create the correct slices (after update 1)" in {
    implicit val xact = tm.begin("T14")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (227)
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit
  }

  it should "execute the foreach over all visible elements (4)" in {
    implicit val xact = tm.begin("T15")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (240)
    xact.commit
  }

  it should "be able to update via delta version" in {
    implicit val xact = tm.begin("T16")
    val ent = tbl.getEntry(Key(1,"z"))
    ent.setEntryValue((2222,"aaa"))
    
    tbl.get(Key(1,"z")) should be ((2222,"aaa"))
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit
  }

  it should "create the correct slices (after update 2)" in {
    implicit val xact = tm.begin("T17")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (2227)
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit
  }

  it should "execute the foreach over all visible elements (5)" in {
    implicit val xact = tm.begin("T18")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (2240)
    xact.commit
  }

}