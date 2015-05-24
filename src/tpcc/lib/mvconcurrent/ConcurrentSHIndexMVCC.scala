package ddbt.tpcc.lib.mvconcurrent

import ConcurrentSHIndexMVCC._
import ConcurrentSHMapMVCC.DeltaVersion
import ddbt.tpcc.lib.concurrent.ConcurrentSHMap
import ddbt.tpcc.lib.concurrent.ConcurrentSHSet
import ddbt.tpcc.tx._
import MVCCTpccTableV3._

object ConcurrentSHIndexMVCC {
  val EMPTY_INDEX_ENTRY = new ConcurrentSHIndexMVCCEntry[Any,Product]
}

class ConcurrentSHIndexMVCCEntry[K,V <: Product] {
  val s:ConcurrentSHSet[DeltaVersion[K,V]] = new ConcurrentSHSet[DeltaVersion[K,V]]

  @inline
  final def foreach(f: (K,V) => Unit)(implicit xact:Transaction): Unit = s.foreach(e => f(e.entry.key, e.getImage))

  @inline
  final def foreachEntry(f: java.util.Map.Entry[DeltaVersion[K,V], Boolean] => Unit)(implicit xact:Transaction): Unit = s.foreachEntry(e => f(e))
}

class ConcurrentSHIndexMVCC[P,K,V <: Product](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int) {

  val idx = new ConcurrentSHMap[P,ConcurrentSHIndexMVCCEntry[K,V]](loadFactor, initialCapacity)

  @inline
  final def set(entry: DeltaVersion[K,V])(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.entry.key, entry.getImage)
    val s = idx.get(p)
    if (s==null) {
      val newIdx = new ConcurrentSHIndexMVCCEntry[K,V]
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
  final def slice(part:P)(implicit xact:Transaction):ConcurrentSHIndexMVCCEntry[K,V] = idx.get(part) match { 
    case null => EMPTY_INDEX_ENTRY.asInstanceOf[ConcurrentSHIndexMVCCEntry[K,V]]
    case s=>s
  }

  @inline
  final def clear(implicit xact:Transaction):Unit = idx.clear
}
