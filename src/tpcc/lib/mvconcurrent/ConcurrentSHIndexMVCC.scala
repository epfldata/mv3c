package ddbt.tpcc.lib.mvconcurrent

import ConcurrentSHIndexMVCC._
import ConcurrentSHMapMVCC.DeltaVersion
import ddbt.tpcc.lib.concurrent.ConcurrentSHMap
import ddbt.tpcc.lib.concurrent.ConcurrentSHSet
import ddbt.tpcc.tx._
import MVCCTpccTableV3._

object ConcurrentSHIndexMVCC {
  val EMPTY_INDEX_ENTRY = new ConcurrentSHIndexMVCCEntry[Any,Any,Product](0,(k,v) => 0)
}

class ConcurrentSHIndexMVCCEntry[P,K,V <: Product](val p: P, val proj:(K,V)=>P) {
  val s:ConcurrentSHSet[DeltaVersion[K,V]] = new ConcurrentSHSet[DeltaVersion[K,V]]

  @inline
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

  @inline
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

class ConcurrentSHIndexMVCC[P,K,V <: Product](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int) {

  val idx = new ConcurrentSHMap[P,ConcurrentSHIndexMVCCEntry[P,K,V]](loadFactor, initialCapacity)

  @inline
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

  @inline
  final def del(entry: DeltaVersion[K,V])(implicit xact:Transaction):Unit = del(entry, entry.getImage)

  @inline
  final def del(entry: DeltaVersion[K,V], v:V)(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.entry.key, v)
    val s=idx.get(p)
    if (s!=null) { 
      s.s.remove(entry)
      if (s.s.size==0) idx.remove(p)
    }
  }

  @inline
  final def slice(part:P)(implicit xact:Transaction):ConcurrentSHIndexMVCCEntry[P,K,V] = idx.get(part) match { 
    case null => EMPTY_INDEX_ENTRY.asInstanceOf[ConcurrentSHIndexMVCCEntry[P,K,V]]
    case s=>s
  }

  @inline
  final def clear(implicit xact:Transaction):Unit = idx.clear
}
