package ddbt.tpcc.lib.concurrent

import SHSet._
import java.util.Map

object SHSet {
  /**
   * Default value for inner ConcurrentSHMap in SHSet
   */
  val DEFAULT_PRESENT_VALUE: Boolean = true

  val PARALLELISM_THRESHOLD = 128
}

class SHSet[K] {
  var map: ConcurrentSHMap[K,Boolean] = new ConcurrentSHMap[K,Boolean]

  // def this(initialCapacity: Int, lf: Float) {
  //   this()
  //   map = new ConcurrentSHMap[K,Boolean](initialCapacity, lf)
  // }

  // def this(initialCapacity: Int) {
  //   this(initialCapacity, DEFAULT_LOAD_FACTOR)
  // }

  /**
   * Initialization hook for subclasses. This method is called
   * in all constructors and pseudo-constructors (clone, readObject)
   * after ConcurrentSHMap has been initialized but before any entries have
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
  final def isEmpty: Boolean = map.isEmpty

  /**
   * Returns <tt>true</tt> if this map contains a mapping for the
   * specified key.
   *
   * @param   key   The key whose presence in this map is to be tested
   * @return <tt>true</tt> if this map contains a mapping for the specified
   *         key.
   */
  @inline
  final def contains(key: K): Boolean = map.contains(key)

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
  final def add(key: K): Boolean = {
    val e: Boolean = map.put(key,DEFAULT_PRESENT_VALUE)
    (e == DEFAULT_PRESENT_VALUE)
  }

  def +=(key: K): Boolean = add(key)

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
  final def remove(key: K): Boolean = {
    val e: Boolean = map.remove(key)
    (e == DEFAULT_PRESENT_VALUE)
  }

  @inline
  final def -=(key: K): Boolean = remove(key)

  /**
   * Removes all of the mappings from this map.
   * The map will be empty after this call returns.
   */
  @inline
  final def clear:Unit = map.clear

  @inline
  final def capacity: Int = map.table.length

  @inline
  final def size:Int = map.size

  // def loadFactor: Float = {
  //   return loadFactor
  // }

  @inline
  final def foreach(f: K => Unit): Unit = map.forEachKey(PARALLELISM_THRESHOLD, f)

  @inline
  final def foreachEntry(f: Map.Entry[K, Boolean] => Unit): Unit = map.forEachEntry(PARALLELISM_THRESHOLD, f)

  override def toString: String = {
    var res = new StringBuilder("[")
    var first = true
    map.forEachEntry(Long.MaxValue,{ e =>
      if(first) first = false
      else res.append(", ")
      res.append(e.getKey.toString)
    })
    res.append("]")
    res.toString
  }
}