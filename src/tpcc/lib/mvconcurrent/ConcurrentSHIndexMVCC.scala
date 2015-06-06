package ddbt.tpcc.lib.mvconcurrent

import ConcurrentSHIndexMVCC._
import ConcurrentSHMapMVCC.DeltaVersion
import ddbt.tpcc.lib.concurrent.ConcurrentSHMap
import ddbt.tpcc.lib.concurrent.ConcurrentSHSet
import ddbt.tpcc.tx._
import MVCCTpccTableV3._

object ConcurrentSHIndexMVCC {
  implicit val ord: math.Ordering[Any] = null
  implicit def deltaVersionOrd[K,V <: Product](implicit ord: math.Ordering[K]) = Ordering.by{ e:DeltaVersion[K,V] => e.getKey }
  val EMPTY_INDEX_ENTRY = new ConcurrentSHIndexMVCCEntry[Any,Any,Product](0,(k,v) => 0)
}

class ConcurrentSHIndexMVCCEntry[P,K,V <: Product](val p: P, val proj:(K,V)=>P)(implicit ord: math.Ordering[K]) {
  //Q: Why do we index DeltaVersion and not SEntryMVCC?
  //A: DeltaVersion is the base data stored in the store, and might migrate between
  //   different sub-types of SEntryMVCC (e.g. TreeNode). So, a reference to it might
  //   get changed during the execution.
  val s:ConcurrentSHSet[DeltaVersion[K,V]] = new ConcurrentSHSet[DeltaVersion[K,V]]

  // @inline //inlining is disabled during development
  final def foreach(f: (K,V) => Unit)(implicit xact:Transaction): Unit = s.foreach{ e =>
    if(e.isVisible) {
      f(e.entry.key, e.getImage)
      // println ((e.entry.map.tbl, e.entry.key, e.getImage) + " is visible!")
    }
    // else {
    //   println ((e.entry.map.tbl, e.entry.key, e) + " is not visible!")
    // }
  }

  // @inline //inlining is disabled during development
  final def foreachEntry(f: java.util.Map.Entry[DeltaVersion[K,V], Boolean] => Unit)(implicit xact:Transaction): Unit = s.foreachEntry{ e =>
    if(e.getKey.isVisible) {
      if (e.getKey != e.getKey.entry.getTheValue) throw new RuntimeException("Wrong foreacEntry => "+e.getKey.entry.key)
      f(e)
      // println ((e.getKey.entry.map.tbl,e.getKey.entry.key, e.getKey.getImage) + " (entry) is visible!")
    }
    // else {
    //   println ((e.getKey.entry.map.tbl,e.getKey.entry.key, e.getKey) + " (entry) is not visible!")
    // }
  }
}

class ConcurrentSHIndexMVCC[P,K,V <: Product](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int)(implicit ordP: math.Ordering[P], ordK: math.Ordering[K]) {

  val idx = new ConcurrentSHMap[P,ConcurrentSHIndexMVCCEntry[P,K,V]](loadFactor, initialCapacity)

  // @inline //inlining is disabled during development
  final def set(entry: DeltaVersion[K,V])(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.getKey, entry.getImage)
    val s = idx.get(p)
    if (s==null) {
      val newIdx = new ConcurrentSHIndexMVCCEntry[P,K,V](p, proj)
      newIdx.s.add(entry)
      idx.put(p,newIdx)
    } else {
      s.s.add(entry)
    }
  }

  // @inline //inlining is disabled during development
  final def del(entry: DeltaVersion[K,V]):Unit = del(entry, entry.getImage)

  // @inline //inlining is disabled during development
  final def del(entry: DeltaVersion[K,V], v:V):Unit = {
    val p:P = proj(entry.getKey, v)
    val s=idx.get(p)
    if (s!=null) { 
      s.s.remove(entry)
      if (s.s.size==0) idx.remove(p)
    }
  }

  // @inline //inlining is disabled during development
  final def slice(part:P)(implicit xact:Transaction):ConcurrentSHIndexMVCCEntry[P,K,V] = idx.get(part) match { 
    case null => EMPTY_INDEX_ENTRY.asInstanceOf[ConcurrentSHIndexMVCCEntry[P,K,V]]
    case s=>s
  }

  // @inline //inlining is disabled during development
  final def clear(implicit xact:Transaction):Unit = idx.clear
}
