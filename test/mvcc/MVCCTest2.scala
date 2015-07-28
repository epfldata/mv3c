package ddbt.lib.mvconcurrent

import ddbt.tpcc.tx._
import MVCCTpccTableV3._
import org.scalatest._
import MVCCTestParams._

class MVCCSpec2 extends FlatSpec with Matchers {

  object SingleHashKey {
    def apply(_1:Int, _2:String) = new SingleHashKey(_1,_2)
    implicit def orderingSingleHashKey[A <: SingleHashKey]: Ordering[A] = Ordering.by(e => (e._1, e._2))
  }

  class SingleHashKey(val _1:Int, val _2:String) {
    override def equals(o: Any) = {
      if(!o.isInstanceOf[SingleHashKey]) false
      else {
        val other = o.asInstanceOf[SingleHashKey]
        (_1,_2) == (other._1,other._2)
      }
    }

    override def hashCode = 10

    override def toString = (_1,_2).toString
  }

  val tm = new TransactionManager
  tm.isGcActive.set(disableGC)
  val tbl = new ConcurrentSHMapMVCC[SingleHashKey,(Int,String)]("Test2Map", (k:SingleHashKey,v:(Int,String)) => k._1 )

  "A MVCC table (with a treeified bin)" should "be able to insert an element and store it properly (before reaching threshold" in {
    ConcurrentSHMapMVCC.TREEIFY_THRESHOLD should be (8) //we assume that threshold is 8
    implicit val xact = tm.begin("T1")
    tbl += (SingleHashKey(1,"z"),(2,"a"))
    tbl += (SingleHashKey(1,"x"),(5,"e"))
    tbl += (SingleHashKey(2,"z"),(6,"f"))
    tbl += (SingleHashKey(2,"y"),(7,"g"))
    tbl += (SingleHashKey(3,"z"),(8,"h"))
    tbl += (SingleHashKey(3,"y"),(9,"i"))
    tbl += (SingleHashKey(4,"z"),(10,"j"))
    tbl += (SingleHashKey(4,"y"),(11,"k"))
    tbl += (SingleHashKey(4,"x"),(12,"l"))
    tbl.get(SingleHashKey(1,"z")) should be ((2,"a"))
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "persistent an inserted element after the commit" in {
    implicit val xact = tm.begin("T2")
    try {
      tbl.get(SingleHashKey(1,"z")) should be ((2,"a"))
      tbl.size should be (9)
      xact.commit should be (true)
    } catch {
      case e:Throwable => {
        xact.rollback
        throw e
      }
    }
  }

  it should "throw an exception on inserting an existing key" in {
    implicit val xact = tm.begin("T3")
    a [MVCCRecordAlreadyExistsException] should be thrownBy {
      tbl += (SingleHashKey(1,"z"),(3,"b"))
    }
    tbl.size should be (9)
    xact.rollback
  }

  it should "throw an exception on inserting an already inserted entry even by current transaction" in {
    implicit val xact = tm.begin("T4")
    tbl += (SingleHashKey(1,"y"),(4,"c"))
    a [MVCCRecordAlreadyExistsException] should be thrownBy {
      tbl += (SingleHashKey(1,"y"),(5,"d"))
    }
    tbl.size should be (10)
    xact.rollback
  }

  it should "remove the stale versions produced by aborted xacts" in {
    implicit val xact = tm.begin("T5")
    tbl.get(SingleHashKey(1,"y")) should be (null)
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "create the correct slices (after insertions)" in {
    implicit val xact = tm.begin("T6")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (7)
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "execute the foreach over all visible elements (1)" in {
    implicit val xact = tm.begin("T7")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (70)
    xact.commit should be (true)
  }

  it should "be able to delete an element" in {
    implicit val xact = tm.begin("T8")
    tbl -= (SingleHashKey(1,"z"))
    tbl.get(SingleHashKey(1,"z")) should be (null)
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "create the correct slices (after deletion)" in {
    implicit val xact = tm.begin("T9")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (5)
    // tbl.size should be (10) or (9) //it might get deleted by gc
    xact.commit should be (true)
  }

  it should "execute the foreach over all visible elements (2)" in {
    implicit val xact = tm.begin("T10")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (68)
    xact.commit should be (true)
  }

  it should "be able to re-insert an already deleted element" in {
    implicit val xact = tm.begin("T11")
    tbl += (SingleHashKey(1,"z"), (22,"aa"))
    tbl.get(SingleHashKey(1,"z")) should be ((22,"aa"))
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "execute the foreach over all visible elements (3)" in {
    implicit val xact = tm.begin("T12")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (90)
    xact.commit should be (true)
  }

  it should "be able to update an already inserted element" in {
    implicit val xact = tm.begin("T13")
    tbl(SingleHashKey(1,"z")) = (222,"aaa")
    tbl.get(SingleHashKey(1,"z")) should be ((222,"aaa"))
    tbl.size should be (9)
    xact.commit should be (true)
  }

  it should "create the correct slices (after update 1)" in {
    implicit val xact = tm.begin("T14")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (227)
    // tbl.size should be (10) or (9) //it might get deleted by gc
    xact.commit should be (true)
  }

  it should "execute the foreach over all visible elements (4)" in {
    implicit val xact = tm.begin("T15")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (290)
    xact.commit should be (true)
  }

  it should "be able to update via delta version" in {
    implicit val xact = tm.begin("T16")
    val ent = tbl.getEntry(SingleHashKey(1,"z"))
    ent.setEntryValue((2222,"aaa"))
    
    tbl.get(SingleHashKey(1,"z")) should be ((2222,"aaa"))
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit should be (true)
  }

  it should "create the correct slices (after update 2)" in {
    implicit val xact = tm.begin("T17")
    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (2227)
    // tbl.size should be (5) or (4) //it might get deleted by gc
    xact.commit should be (true)
  }

  it should "execute the foreach over all visible elements (5)" in {
    implicit val xact = tm.begin("T18")
    var sum = 0
    tbl.foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (2290)
    xact.commit should be (true)
  }

  it should "handle the validation failure correctly" in {
    implicit val xact1 = tm.begin("T19")
    
    {
      implicit val xact2 = tm.begin("T20")
      tbl.+=(SingleHashKey(1,"y"),(4,"c"))(xact2)
      xact2.commit should be (true)
    }

    var sum = 0
    tbl.slice(0,1).foreach { case (k,v) =>
      sum += v._1
    }
    sum should be (2227)

    xact1.commit should be (false)
  }

  it should "handle cleanup correctly after rollback (including the removal of pointers in DeltaVersion)" in {
    {
      implicit val xact2 = tm.begin("T21")
      tbl.update(SingleHashKey(1,"y"),(44,"c"))
      tbl.get(SingleHashKey(1,"y")) should be ((44,"c"))
      xact2.rollback
    }

    {
      implicit val xact1 = tm.begin("T22")
      tbl.get(SingleHashKey(1,"y")) should be ((4,"c"))
      tbl.update(SingleHashKey(1,"y"),(444,"c"))
      tbl.get(SingleHashKey(1,"y")) should be ((444,"c"))
      xact1.commit should be (true)
    }
  }

  it should "be able to untreeify after passing the threshold" in {
    ConcurrentSHMapMVCC.UNTREEIFY_THRESHOLD should be (6) //we assume that threshold is 8
    implicit val xact = tm.begin("T23")
    for( i <- 1 to 4){
      tbl -= (SingleHashKey(i,"z"))
    }
    tbl.size should be (10)
    xact.commit should be (true)
  }

}