package ddbt.tpcc.lib.mvshm

import ddbt.tpcc.tx._
import MVCCTpccTableV1._
import SHMapMVCC._
import ddbt.Utils.ind

/**
 * Super Hash Map
 *
 * Java HashMap converted to Scala and optimized for high-performance
 * execution
 *
 * @author Mohammad Dashti
 */
object SHMapMVCC {
  /**
   * Applies a supplemental hash function to a given hashCode, which
   * defends against poor quality hash functions.  This is critical
   * because SHMapMVCC uses power-of-two length hash tables, that
   * otherwise encounter collisions for hashCodes that do not differ
   * in lower bits. Note: Null keys always map to hash 0, thus index 0.
   */
  @inline
  final def hash(hv: Int): Int = {
    var h = hv ^ (hv >>> 20) ^ (hv >>> 12)
    h ^ (h >>> 7) ^ (h >>> 4)
  }

  /**
   * Returns index for hash code h.
   */
  @inline
  final def indexFor(h: Int, length: Int): Int = h & (length - 1)

  /**
   * The default initial capacity - MUST be a power of two.
   */
  final val DEFAULT_INITIAL_CAPACITY: Int = 16
  /**
   * The maximum capacity, used if a higher value is implicitly specified
   * by either of the constructors with arguments.
   * MUST be a power of two <= 1<<30.
   */
  final val MAXIMUM_CAPACITY: Int = 1 << 30
  /**
   * The load factor used when none specified in constructor.
   */
  final val DEFAULT_LOAD_FACTOR: Float = 0.75f
  
  type Operation = Int
  val INSERT_OP:Operation = 1
  val DELETE_OP:Operation = 2
  val UPDATE_OP:Operation = 3
}

final case class DeltaVersion[K,V <: Product](val xact:Transaction, val entry:SEntryMVCC[K,V], val beforeImg:V, val op: Operation, val colIds:List[Int]=Nil /*all columns*/, var next: DeltaVersion[K,V]=null, var prev: DeltaVersion[K,V]=null)

final class SEntryMVCC[K,V <: Product](var hash: Int=0, var key: K=null, var next: SEntryMVCC[K, V]=null, var value: DeltaVersion[K,V]=null) { self =>

  @inline
  final def getKey: K = key

  @inline
  final def getValue(implicit xact:Transaction): V = value.beforeImg

  @inline
  final def setValue(newValue: V)(implicit xact:Transaction): V = {
    val oldValue: V = if(value == null) null.asInstanceOf[V] else value.beforeImg
    value = DeltaVersion(xact,this,newValue,INSERT_OP)
    oldValue
  }

  // def setAll(h: Int, k: K, v: V, n: SEntryMVCC[K, V]):SEntryMVCC[K, V] = {
  //   hash = h
  //   key = k
  //   value = v
  //   next = n
  //   self
  // }

  override def equals(o: Any): Boolean = {
    throw new UnsupportedOperationException("The equals method is not supported in SEntryMVCC. You should use the overloaded method accepting the Transaction as an extra parameter.")
  }
  @inline
  final def equals(o: Any)(implicit xact:Transaction): Boolean = {
    if (!(o.isInstanceOf[SEntryMVCC[K, V]])) return false
    val e: SEntryMVCC[K, V] = o.asInstanceOf[SEntryMVCC[K, V]]
    val k1: K = getKey
    val k2: K = e.getKey
    if (k1 == k2) {
      val v1: V = getValue
      val v2: V = e.getValue
      (v1 == v2)
    } else false
  }

  override def hashCode: Int = getKey.hashCode

  @inline
  final override def toString: String = {
    throw new UnsupportedOperationException("The toString method is not supported in SEntryMVCC. You should use the overloaded method accepting the Transaction as an extra parameter.")
  }
  def toString(implicit xact:Transaction): String = {
    "(" + getKey + " -> " + getValue + ")"
  }
}

final class SHMapMVCC[K,V <: Product](initialCapacity: Int, val loadFactor: Float, lfInitIndex: List[(Float,Int)], projs:Seq[(K,V)=>_]) {
  /**
   * Constructs an empty <tt>SHMapMVCC</tt> with the specified initial
   * capacity and load factor.
   *
   * @param  initialCapacity the initial capacity
   * @param  loadFactor      the load factor
   * @throws IllegalArgumentException if the initial capacity is negative
   *                                  or the load factor is nonpositive
   */
  // def this(initialCapacity: Int, lf: Float) {
  //   this()
  //   if (initialCapacity < 0) throw new RuntimeException("Illegal initial capacity: " + initialCapacity)
  //   var capacity: Int = 1
  //   if (initialCapacity > MAXIMUM_CAPACITY) {
  //     capacity = MAXIMUM_CAPACITY
  //   } else {
  //     while (capacity < initialCapacity) capacity <<= 1
  //   }
  //   if (lf <= 0 /*|| Float.isNaN(lf)*/) throw new RuntimeException("Illegal load factor: " + lf)
  //   loadFactor = lf
  //   threshold = (capacity * lf).asInstanceOf[Int]
  //   table = new Array[SEntryMVCC[K, V]](capacity)
  // }

  /**
   * Constructs an empty <tt>SHMapMVCC</tt> with the specified initial
   * capacity and the default load factor (0.75).
   *
   * @param  initialCapacity the initial capacity.
   * @throws IllegalArgumentException if the initial capacity is negative.
   */
  // def this(initialCapacity: Int) {
  //   this(initialCapacity, DEFAULT_LOAD_FACTOR)
  // }

  def this(loadFactor: Float, initialCapacity: Int, lfInitIndex: List[(Float,Int)], projs:(K,V)=>_ *) {
    this(initialCapacity, loadFactor, lfInitIndex, projs)
  }

  def this(loadFactor: Float, initialCapacity: Int, projs:(K,V)=>_ *) {
    this(initialCapacity, loadFactor, projs.map{p => ((DEFAULT_LOAD_FACTOR, DEFAULT_INITIAL_CAPACITY))}.toList, projs)
  }

  def this(initialCapacity: Int, projs:(K,V)=>_ *) {
    this(initialCapacity, DEFAULT_LOAD_FACTOR, projs.map{p => ((DEFAULT_LOAD_FACTOR, DEFAULT_INITIAL_CAPACITY))}.toList, projs)
  }

  def this(projs:(K,V)=>_ *) {
    this(DEFAULT_INITIAL_CAPACITY, DEFAULT_LOAD_FACTOR, projs.map{p => ((DEFAULT_LOAD_FACTOR, DEFAULT_INITIAL_CAPACITY))}.toList, projs)
  }

  /**
   * Initialization hook for subclasses. This method is called
   * in all constructors and pseudo-constructors (clone, readObject)
   * after SHMapMVCC has been initialized but before any entries have
   * been inserted.  (In the absence of this method, readObject would
   * require explicit knowledge of subclasses.)
   */
  // def init:Unit = {
  // }

  /**
   * Returns the number of key-value mappings in this map.
   *
   * @return the number of key-value mappings in this map
   */
  // def size: Int = size

  /**
   * Returns <tt>true</tt> if this map contains no key-value mappings.
   *
   * @return <tt>true</tt> if this map contains no key-value mappings
   */
  @inline
  final def isEmpty: Boolean = (size == 0)

  /**
   * Returns the value to which the specified key is mapped,
   * or {@code null} if this map contains no mapping for the key.
   *
   * <p>More formally, if this map contains a mapping from a key
   * {@code k} to a value {@code v} such that {@code (key==null ? k==null :
     * key.equals(k))}, then this method returns {@code v}; otherwise
   * it returns {@code null}.  (There can be at most one such mapping.)
   *
   * <p>A return value of {@code null} does not <i>necessarily</i>
   * indicate that the map contains no mapping for the key; it's also
   * possible that the map explicitly maps the key to {@code null}.
   * The {@link #contains contains} operation may be used to
   * distinguish these two cases.
   *
   * @see #put(Object, Object)
   */
  @inline
  final def apply(key: K)(implicit xact:Transaction): V = {
    val e = getEntry(key)
    if(e == null) null.asInstanceOf[V] else e.getValue
  }
  // def get(key: K): V = apply(key)
  // def getEntry(key: K)(implicit xact:Transaction): SEntryMVCC[K,V] = {
  //   // if (key == null) return getForNullKey
  //   val hs: Int = hash(key.hashCode)
  //   var e: SEntryMVCC[K, V] = table(indexFor(hs, table.length))
  //   while (e != null) {
  //     val k: K = e.key
  //     if (e.hash == hs && key == k) return e
  //     e = e.next
  //   }
  //   throw new java.util.NoSuchElementException
  // }
  // def getNullOnNotFound(key: K)(implicit xact:Transaction): V = {
  //   // if (key == null) return getForNullKey
  //   val hs: Int = hash(key.hashCode)
  //   var e: SEntryMVCC[K, V] = table(indexFor(hs, table.length))
  //   while (e != null) {
  //     val k: K = e.key
  //     if (e.hash == hs && key == k) return e.getValue
  //     e = e.next
  //   }
  //   null.asInstanceOf[V]
  // }

  /**
   * Offloaded version of get() to look up null keys.  Null keys map
   * to index 0.  This null case is split out into separate methods
   * for the sake of performance in the two most commonly used
   * operations (get and put), but incorporated with conditionals in
   * others.
   */
  // private def getForNullKey(implicit xact:Transaction): V = {
  //   var e: SEntryMVCC[K, V] = table(0)
  //   while (e != null) {
  //     if (e.key == null) return e.getValue
  //     e = e.next
  //   }
  //   null.asInstanceOf[V]
  // }

  /**
   * Returns <tt>true</tt> if this map contains a mapping for the
   * specified key.
   *
   * @param   key   The key whose presence in this map is to be tested
   * @return <tt>true</tt> if this map contains a mapping for the specified
   *         key.
   */
  @inline
  final def contains(key: K)(implicit xact:Transaction): Boolean = (getEntry(key) != null)

  /**
   * Returns the entry associated with the specified key in the
   * SHMapMVCC.  Returns null if the SHMapMVCC contains no mapping
   * for the key.
   */
  @inline
  final def getEntry(key: K)(implicit xact:Transaction): SEntryMVCC[K, V] = {
    val hs: Int = /*if ((key == null)) 0 else*/ hash(key.hashCode)
    var e: SEntryMVCC[K, V] = table(indexFor(hs, table.length))
    while (e != null) {
      val k: K = e.key
      if (e.hash == hs && key == k) return e
      e = e.next
    }
    null.asInstanceOf[SEntryMVCC[K, V]]
  }

  /**
   * Associates the specified value with the specified key in this map.
   * If the map previously contained a mapping for the key, the old
   * value is replaced.
   *
   * @param key key with which the specified value is to be associated
   * @param value value to be associated with the specified key
   * @return the previous value associated with <tt>key</tt>, or
   *         <tt>null</tt> if there was no mapping for <tt>key</tt>.
   *         (A <tt>null</tt> return can also indicate that the map
   *         previously associated <tt>null</tt> with <tt>key</tt>.)
   */
  @inline
  final def insert(key: K, value: V)(implicit xact:Transaction): V = {
    // if (key == null) return putForNullKey(value)
    val hs: Int = hash(key.hashCode)
    val i: Int = indexFor(hs, table.length)
    var e: SEntryMVCC[K, V] = table(i)
    while (e != null) {
      val k: K = e.key
      if (e.hash == hs && key == k) {
        val oldValue: V = e.getValue
        e.setValue(value)
        if (idxs != Nil) idxs.foreach{ idx => {
            val pOld = idx.proj(k,oldValue)
            val pNew = idx.proj(k,value)
            if(pNew != pOld) {
              idx.del(e, oldValue)
              idx.set(e)
            }
          }
        }
        return oldValue
      }
      e = e.next
    }
    e = addSEntryMVCC(hs, key, value, i)
    if (idxs!=Nil) idxs.foreach(_.set(e))
    null.asInstanceOf[V]
  }

  @inline
  final def +=(key: K, value: V)(implicit xact:Transaction): V = insert(key, value)

  //def +=(kv: (K, V)): V = put(kv._1, kv._2)

  @inline
  final def put(key: K, value: V)(implicit xact:Transaction): V = insert(key, value)

  @inline
  final def update(key: K, value: V)(implicit xact:Transaction): V = update(key, x => value)

  @inline
  final def update(key: K, valueUpdateFunc: V=>V)(implicit xact:Transaction): V = {
    // if (key == null) return putForNullKey(value)
    val hs: Int = hash(key.hashCode)
    val i: Int = indexFor(hs, table.length)
    var e: SEntryMVCC[K, V] = table(i)
    while (e != null) {
      val k: K = e.key
      if (e.hash == hs && key == k) {
        val oldValue: V = e.getValue
        val value:V = valueUpdateFunc(oldValue)
        e.setValue(value)
        if (idxs != Nil) idxs.foreach{ idx => {
            val pOld = idx.proj(k,oldValue)
            val pNew = idx.proj(k,value)
            if(pNew != pOld) {
              idx.del(e, oldValue)
              idx.set(e)
            }
          }
        }
        return oldValue
      }
      e = e.next
    }
    null.asInstanceOf[V]
  }

  /**
   * Offloaded version of put for null keys
   */
  // private def putForNullKey(value: V): V = {
  //   var e: SEntryMVCC[K, V] = table(0)
  //   while (e != null) {
  //     if (e.key == null) {
  //       val oldValue: V = e.getValue
  //       e.setValue(value)
  //       return oldValue
  //     }
  //     e = e.next
  //   }
  //   addSEntryMVCC(0, null, value, 0)
  //   null
  // }

  /**
   * This method is used instead of put by constructors and
   * pseudoconstructors (clone, readObject).  It does not resize the table,
   * check for comodification, etc.  It calls createSEntryMVCC rather than
   * addSEntryMVCC.
   */
  // private def putForCreate(key: K, value: V):Unit {
  //   val hash: Int = if ((key == null)) 0 else hash(key.hashCode)
  //   val i: Int = indexFor(hash, table.length)
  //   {
  //     var e: SEntryMVCC[K, V] = table(i)
  //     while (e != null) {
  //       {
  //         var k: K = null
  //         if (e.hash == hash && ((({
  //           k = e.key; k
  //         })) eq key || (key != null && (key == k)))) {
  //           e.setValue(value)
  //           return
  //         }
  //       }
  //       e = e.next
  //     }
  //   }
  //   createSEntryMVCC(hash, key, value, i)
  // }
  //
  // private def putAllForCreate(m: Nothing):Unit {
  //   import scala.collection.JavaConversions._
  //   for (e <- m.entrySet) putForCreate(e.getKey, e.getValue)
  // }

  /**
   * Rehashes the contents of this map into a new array with a
   * larger capacity.  This method is called automatically when the
   * number of keys in this map reaches its threshold.
   *
   * If current capacity is MAXIMUM_CAPACITY, this method does not
   * resize the map, but sets threshold to Integer.MAX_VALUE.
   * This has the effect of preventing future calls.
   *
   * @param newCapacity the new capacity, MUST be a power of two;
   *                    must be greater than current capacity unless current
   *                    capacity is MAXIMUM_CAPACITY (in which case value
   *                    is irrelevant).
   */
  @inline
  final def resize(newCapacity: Int)(implicit xact:Transaction):Unit = {
    val oldTable: Array[SEntryMVCC[K, V]] = table
    val oldCapacity: Int = oldTable.length
    if (oldCapacity == MAXIMUM_CAPACITY) {
      threshold = Integer.MAX_VALUE
      return
    }
    val newTable: Array[SEntryMVCC[K, V]] = new Array[SEntryMVCC[K, V]](newCapacity)
    transfer(newTable)
    table = newTable
    threshold = (newCapacity * loadFactor).asInstanceOf[Int]
  }

  /**
   * Transfers all entries from current table to newTable.
   */
  @inline
  final def transfer(newTable: Array[SEntryMVCC[K, V]])(implicit xact:Transaction) {
    val src: Array[SEntryMVCC[K, V]] = table
    val newCapacity: Int = newTable.length
    var j: Int = 0
    while (j < src.length) {
      var e: SEntryMVCC[K, V] = src(j)
      if (e != null) {
        src(j) = null
        do {
          val next: SEntryMVCC[K, V] = e.next
          val i: Int = indexFor(e.hash, newCapacity)
          e.next = newTable(i)
          newTable(i) = e
          e = next
        } while (e != null)
      }
      j += 1
    }
  }

  /**
   * Removes the mapping for the specified key from this map if present.
   *
   * @param  key key whose mapping is to be removed from the map
   * @return the previous value associated with <tt>key</tt>, or
   *         <tt>null</tt> if there was no mapping for <tt>key</tt>.
   *         (A <tt>null</tt> return can also indicate that the map
   *         previously associated <tt>null</tt> with <tt>key</tt>.)
   */
  @inline
  final def remove(key: K)(implicit xact:Transaction): V = {
    val e = removeSEntryMVCCForKey(key)
    (if (e == null) null.asInstanceOf[V] else e.getValue)
  }

  @inline
  final def delete(key: K)(implicit xact:Transaction): V = remove(key)

  @inline
  final def -=(key: K)(implicit xact:Transaction): V = remove(key)

  /**
   * Removes and returns the entry associated with the specified key
   * in the SHMapMVCC.  Returns null if the SHMapMVCC contains no mapping
   * for this key.
   */
  @inline
  final def removeSEntryMVCCForKey(key: K)(implicit xact:Transaction): SEntryMVCC[K, V] = {
    val hs: Int = /*if ((key == null)) 0 else*/ hash(key.hashCode)
    val i: Int = indexFor(hs, table.length)
    var prev: SEntryMVCC[K, V] = table(i)
    var e: SEntryMVCC[K, V] = prev
    while (e != null) {
      val next: SEntryMVCC[K, V] = e.next
      var k: K = e.key
      if (e.hash == hs && key == k) {
        if (idxs!=Nil) idxs.foreach(_.del(e))
        size -= 1
        if (prev eq e) table(i) = next
        else prev.next = next
        return e
      }
      prev = e
      e = next
    }
    e
  }

  /**
   * Special version of remove for SEntryMVCCSet.
   */
  // def removeMapping(entry: SEntryMVCC[K, V]): SEntryMVCC[K, V] = {
  //   // if (!(o.isInstanceOf[Any])) return null
  //   // val entry: SEntryMVCC[K, V] = o.asInstanceOf[SEntryMVCC[K, V]]
  //   val key: K = entry.getKey
  //   val hs: Int = /*if ((key == null)) 0 else*/ hash(key.hashCode)
  //   val i: Int = indexFor(hs, table.length)
  //   var prev: SEntryMVCC[K, V] = table(i)
  //   var e: SEntryMVCC[K, V] = prev
  //   while (e != null) {
  //     val next: SEntryMVCC[K, V] = e.next
  //     if (e.hash == hs && e == entry) {
  //       size -= 1
  //       if (prev eq e) table(i) = next
  //       else prev.next = next
  //       return e
  //     }
  //     prev = e
  //     e = next
  //   }
  //   e
  // }

  /**
   * Removes all of the mappings from this map.
   * The map will be empty after this call returns.
   */
  @inline
  final def clear(implicit xact:Transaction):Unit = {
    var i: Int = 0
    while (i < table.length) {
      table(i) = null
      i += 1
    }
    size = 0
    if (idxs!=Nil) idxs.foreach(_.clear)
  }

  /**
   * Returns <tt>true</tt> if this map maps one or more keys to the
   * specified value.
   *
   * @param value value whose presence in this map is to be tested
   * @return <tt>true</tt> if this map maps one or more keys to the
   *         specified value
   */
  // def containsValue(value: V): Boolean = {
  //   if (value == null) return containsNullValue
  //   val tab: Array[SEntryMVCC[K, V]] = table
  //   var i: Int = 0
  //   while (i < tab.length) {
  //     var e: SEntryMVCC[K, V] = tab(i)
  //     while (e != null) {
  //       if (value == e.getValue) return true
  //       e = e.next
  //     }
  //     i += 1
  //   }
  //   false
  // }

  /**
   * Special-case code for containsValue with null argument
   */
  // private def containsNullValue: Boolean = {
  //   var i: Int = 0
  //   while (i < table.length) {
  //     var e: SEntryMVCC[K, V] = table(i)
  //     while (e != null) {
  //       if (e.getValue == null) return true
  //       e = e.next
  //     }
  //     i += 1
  //   }
  //   false
  // }

  /**
   * Adds a new entry with the specified key, value and hash code to
   * the specified bucket.  It is the responsibility of this
   * method to resize the table if appropriate.
   *
   * Subclass overrides this to alter the behavior of put method.
   */
  @inline
  final def addSEntryMVCC(hash: Int, key: K, value: V, bucketIndex: Int)(implicit xact:Transaction):SEntryMVCC[K, V] = {
    val tmp: SEntryMVCC[K, V] = table(bucketIndex)

    val e = new SEntryMVCC[K, V](hash, key, tmp)
    e.setValue(value)
    table(bucketIndex) = e
    if (size >= threshold) resize(2 * table.length)
    size += 1
    e
  }

  /**
   * Like addSEntryMVCC except that this version is used when creating entries
   * as part of Map construction or "pseudo-construction" (cloning,
   * deserialization).  This version needn't worry about resizing the table.
   *
   * Subclass overrides this to alter the behavior of SHMapMVCC(Map),
   * clone, and readObject.
   */
  // def createSEntryMVCC(hash: Int, key: K, value: V, bucketIndex: Int):Unit = {
  //   val e: SEntryMVCC[K, V] = table(bucketIndex)
  //   table(bucketIndex) = new SEntryMVCC[K, V](hash, key, value, e)
  //   size += 1
  // }

  @inline
  final def capacity(implicit xact:Transaction): Int = table.length

  // def loadFactor: Float = {
  //   return loadFactor
  // }

  @inline
  final def foreach(f: ((K, V)) => Unit)(implicit xact:Transaction): Unit = foreachEntry(e => f(e.key, e.getValue))

  @inline
  final def foreachEntry(f: SEntryMVCC[K, V] => Unit)(implicit xact:Transaction): Unit = {
    var i: Int = 0
    while (i < table.length) {
      var e: SEntryMVCC[K, V] = table(i)
      while (e != null) {
        f(e)
        e = e.next
      }
      i += 1
    }
  }

  override def toString: String = {
    throw new UnsupportedOperationException("The toString method is not supported in SEntryMVCC. You should use the overloaded method accepting the Transaction as an extra parameter.")
  }
  def toString(implicit xact:Transaction): String = {
    var res = new StringBuilder("[")
    var first = true
    foreachEntry ({ e =>
      if(first) first = false
      else res.append(", ")
      res.append(e.toString)
    })
    res.append("]")
    res.toString
  }

  override def equals(other:Any):Boolean = {
    throw new UnsupportedOperationException("The equals method is not supported in SEntryMVCC. You should use the overloaded method accepting the Transaction as an extra parameter.")
  }

  def equals(other:Any)(implicit xact:Transaction):Boolean = {
    if(!other.isInstanceOf[SHMapMVCC[K,V]]) false
    else {
      val map2 = other.asInstanceOf[SHMapMVCC[K,V]]
      map2.foreach({ case (k,v) =>
        if(!contains(k) || (!(apply(k) equals v) && (v != apply(k)))) {
          return false
        }
      })
      foreach({ case (k,v) =>
        if(!map2.contains(k) || (!(map2(k) equals v) && (v != map2(k)))) {
          return false
        }
      })
      true
    }
  }

  /**
   * The table, resized as necessary. Length MUST Always be a power of two.
   */
  var table: Array[SEntryMVCC[K, V]] = new Array[SEntryMVCC[K, V]](initialCapacity)
  /**
   * The number of key-value mappings contained in this map.
   */
  var size: Int = 0
  /**
   * The next size value at which to resize (capacity * load factor).
   * @serial
   */
  var threshold: Int = (initialCapacity * loadFactor).asInstanceOf[Int]

  //init

  val idxs:List[SIndexMVCC[_,K,V]] = {
    def idx[P](f:(K,V)=>P, lf:Float, initC:Int) = new SIndexMVCC[P,K,V](f,lf,initC)
    projs.zip(lfInitIndex).toList.map{case (p, (lf, initC)) => idx(p , lf, initC)}
  }

  @inline
  final def slice[P](part:Int, partKey:P)(implicit xact:Transaction):SIndexMVCCEntry[K,V] = {
    val ix=idxs(part)
    ix.asInstanceOf[SIndexMVCC[P,K,V]].slice(partKey) // type information P is erased anyway
  }

  def getInfoStr(implicit xact:Transaction):String = {
    val res = new StringBuilder("MapInfo => {\n")
    res.append("\tsize => ").append(size).append("\n")
    .append("\tcapacity => ").append(capacity).append("\n")
    .append("\tthreshold => ").append(threshold).append("\n")
    var i = 0
    var elemCount = 0
    val contentSize = new Array[Int](table.length)
    while(i < table.length) {
      var counter = 0
      var e: SEntryMVCC[K, V] = table(i)
      while(e != null) {
        counter += 1
        e = e.next
      }
      elemCount += counter
      contentSize(i) = counter
      i += 1
    }
    res.append("\telemCount => ").append(elemCount).append("\n")
    .append("\tmaxElemsInCell => ").append(contentSize.max).append("\n")
    .append("\tavgElemsInCell => ").append("%.2f".format(elemCount/(table.length).asInstanceOf[Double])).append("\n")
    //.append("\tcontentSize => ").append(java.util.Arrays.toString(contentSize)).append("\n")
    i = 0
    if(i < idxs.size) {
      res.append("\tindices => ")
    }

    while(i < idxs.size) {
      res.append("\n\tind(%d) ".format(i+1)).append(ind(idxs(i).idx.getInfoStr))
      i += 1
      if(!(i < idxs.size)) {
        res.append("\n")
      }
    }
    res.append("}").toString
  }
}
