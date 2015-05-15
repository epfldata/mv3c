package ddbt.tpcc.lib.mvshm

import SIndexMVCC._
import ddbt.tpcc.lib.shm.SHMap
import ddbt.tpcc.lib.shm.SHSet
import ddbt.tpcc.tx._
import MVCCTpccTableV1._

object SIndexMVCC {
  val EMPTY_INDEX_ENTRY = new SIndexMVCCEntry[Any,Product]
}

class SIndexMVCCEntry[K,V <: Product] {
  val s:SHSet[SEntry[K,V]] = new SHSet[SEntry[K,V]]

  def foreach(f: ((K, V)) => Unit)(implicit xact:Transaction): Unit = s.foreach(e => f(e.key, e.value))

  def foreachEntry(f: ddbt.tpcc.lib.shm.SEntry[SEntry[K,V], Boolean] => Unit)(implicit xact:Transaction): Unit = s.foreachEntry(e => f(e))
}

class SIndexMVCC[P,K,V <: Product](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int) {

  val idx = new SHMap[P,SIndexMVCCEntry[K,V]](loadFactor, initialCapacity)

  def set(entry: SEntry[K,V])(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.key, entry.value)
    val s = idx.getNullOnNotFound(p)
    if (s==null) {
      val newIdx = new SIndexMVCCEntry[K,V]
      newIdx.s.add(entry)
      idx.put(p,newIdx)
    } else {
      s.s.add(entry)
    }
  }

  def del(entry: SEntry[K,V])(implicit xact:Transaction):Unit = del(entry, entry.value)

  def del(entry: SEntry[K,V], v:V)(implicit xact:Transaction):Unit = {
    val p:P = proj(entry.key, v)
    val s=idx.getNullOnNotFound(p)
    if (s!=null) { 
      s.s.remove(entry)
      if (s.s.size==0) idx.remove(p)
    }
  }

  def slice(part:P)(implicit xact:Transaction):SIndexMVCCEntry[K,V] = idx.getNullOnNotFound(part) match { 
    case null => EMPTY_INDEX_ENTRY.asInstanceOf[SIndexMVCCEntry[K,V]]
    case s=>s
  }

  def clear(implicit xact:Transaction):Unit = idx.clear
}
