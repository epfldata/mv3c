package ddbt.tpcc.lib.concurrent

import ConcurrentSHIndex._
import ConcurrentSHMap.SEntry

object ConcurrentSHIndex {
  implicit val ord: math.Ordering[Any] = null
  implicit def sntryOrd[K,V](implicit ord: math.Ordering[K]) = Ordering.by{ e:SEntry[K,V] => e.key }
  val EMPTY_INDEX_ENTRY = new ConcurrentSHIndexEntry[Any,Any]
}

class ConcurrentSHIndexEntry[K,V](implicit ord: math.Ordering[K]) {
  val s:ConcurrentSHSet[SEntry[K,V]] = new ConcurrentSHSet[SEntry[K,V]]

  def foreach(f: ((K, V)) => Unit): Unit = s.foreach(e => f(e.key, e.value))

  def foreachEntry(f: java.util.Map.Entry[SEntry[K,V], Boolean] => Unit): Unit = s.foreachEntry(e => f(e))
}

class ConcurrentSHIndex[P,K,V](val proj:(K,V)=>P, loadFactor: Float, initialCapacity: Int)(implicit ordP: math.Ordering[P], ordK: math.Ordering[K]) {

  val idx = new ConcurrentSHMap[P,ConcurrentSHIndexEntry[K,V]](loadFactor, initialCapacity)

  def set(entry: SEntry[K,V]):Unit = {
    val p:P = proj(entry.key, entry.value)
    val s = idx.get(p)
    if (s==null) {
      val newIdx = new ConcurrentSHIndexEntry[K,V]
      newIdx.s.add(entry)
      idx.put(p,newIdx)
    } else {
      s.s.add(entry)
    }
  }

  def del(entry: SEntry[K,V]):Unit = del(entry, entry.value)

  def del(entry: SEntry[K,V], v:V):Unit = {
    val p:P = proj(entry.key, v)
    val s=idx.get(p)
    if (s!=null) { 
      s.s.remove(entry)
      if (s.s.size==0) idx.remove(p)
    }
  }

  def slice(part:P):ConcurrentSHIndexEntry[K,V] = idx.get(part) match { 
    case null => EMPTY_INDEX_ENTRY.asInstanceOf[ConcurrentSHIndexEntry[K,V]]
    case s=>s
  }

  def clear:Unit = idx.clear
}
