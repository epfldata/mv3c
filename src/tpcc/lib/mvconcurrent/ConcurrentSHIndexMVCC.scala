package ddbt.tpcc.lib.mvconcurrent

import ConcurrentSHIndexMVCC._
import ConcurrentSHMapMVCC.DeltaVersion
import ddbt.tpcc.lib.concurrent.ConcurrentSHMap
import ddbt.tpcc.lib.concurrent.ConcurrentSHSet
import ddbt.tpcc.tx._
import MVCCTpccTableV3._

object ConcurrentSHIndexMVCC {
  implicit val ord: math.Ordering[Any] = null
  implicit def deltaVersionOrd[K,V <: Product](implicit ord: math.Ordering[K]) = Ordering.by{ e:DeltaVersion[K,V] => e.entry.key }
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
    val ent = e.entry
    val ev = ent.getTheValue
    val img = if(ev ne null) ev.getImage else null.asInstanceOf[V]
    if((ev ne null) && (null != img) && (p == proj(ent.key, img))) {
      f(ent.key, img)
    }
    // else {
    //   println ((ent.key, ev) + " is not visible!")
    // }
  }

  // @inline //inlining is disabled during development
  final def foreachEntry(f: java.util.Map.Entry[DeltaVersion[K,V], Boolean] => Unit)(implicit xact:Transaction): Unit = s.foreachEntry{ e =>
    val ent = e.getKey.entry
    val ev = ent.getTheValue
    val img = if(ev ne null) ev.getImage else null.asInstanceOf[V]
    if((ev ne null) && (null != img) && (p == proj(ent.key, img))) {
      f(e)
    }
    // else {
    //   println ((ent.key, ev) + " is not visible HERE!")
    // }
  }
}

class ConcurrentSHIndexMVCC[P,K,V <: Product](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int)(implicit ordP: math.Ordering[P], ordK: math.Ordering[K]) {

  val idx = new ConcurrentSHMap[P,ConcurrentSHIndexMVCCEntry[P,K,V]](loadFactor, initialCapacity)

  // @inline //inlining is disabled during development
  final def set(entry: DeltaVersion[K,V])(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.entry.key, entry.getImage)
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
  final def del(entry: DeltaVersion[K,V])(implicit xact:Transaction):Unit = del(entry, entry.getImage)

  // @inline //inlining is disabled during development
  final def del(entry: DeltaVersion[K,V], v:V)(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.entry.key, v)
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
