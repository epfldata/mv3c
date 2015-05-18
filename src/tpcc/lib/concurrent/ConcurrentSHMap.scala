package ddbt.tpcc.lib.concurrent

import java.io.{ObjectInputStream, ObjectOutputStream, ObjectStreamField, Serializable}
import java.lang.reflect.ParameterizedType
import java.lang.reflect.Type
import java.util.AbstractMap
import java.util.Arrays
import java.util.Collection
import java.util.Enumeration
import java.util.HashMap
import java.util.Hashtable
import java.util.Iterator
import java.util.Map
import java.util.NoSuchElementException
import java.util.Set
import java.util.Spliterator
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.locks.LockSupport
import java.util.concurrent.locks.ReentrantLock
import java.util.function.BiConsumer
import java.util.function.BiFunction
import java.util.function.Consumer
import java.util.function.DoubleBinaryOperator
import java.util.function.Function
import java.util.function.IntBinaryOperator
import java.util.function.LongBinaryOperator
import java.util.function.ToDoubleBiFunction
import java.util.function.ToDoubleFunction
import java.util.function.ToIntBiFunction
import java.util.function.ToIntFunction
import java.util.function.ToLongBiFunction
import java.util.function.ToLongFunction
import java.util.stream.Stream
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.CountedCompleter;
import java.util.concurrent.ForkJoinPool;

import sun.misc.Unsafe

/**
 * A hash table supporting full concurrency of retrievals and
 * high expected concurrency for updates. This class obeys the
 * same functional specification as {@link java.util.Hashtable}, and
 * includes versions of methods corresponding to each method of
 * {@code Hashtable}. However, even though all operations are
 * thread-safe, retrieval operations do <em>not</em> entail locking,
 * and there is <em>not</em> any support for locking the entire table
 * in a way that prevents all access.  This class is fully
 * interoperable with {@code Hashtable} in programs that rely on its
 * thread safety but not on its synchronization details.
 *
 * <p>Retrieval operations (including {@code get}) generally do not
 * block, so may overlap with update operations (including {@code put}
 * and {@code remove}). Retrievals reflect the results of the most
 * recently <em>completed</em> update operations holding upon their
 * onset. (More formally, an update operation for a given key bears a
 * <em>happens-before</em> relation with any (non-null) retrieval for
 * that key reporting the updated value.)  For aggregate operations
 * such as {@code putAll} and {@code clear}, concurrent retrievals may
 * reflect insertion or removal of only some entries.  Similarly,
 * Iterators, Spliterators and Enumerations return elements reflecting the
 * state of the hash table at some point at or since the creation of the
 * iterator/enumeration.  They do <em>not</em> throw {@link
 * java.util.ConcurrentModificationException ConcurrentModificationException}.
 * However, iterators are designed to be used by only one thread at a time.
 * Bear in mind that the results of aggregate status methods including
 * {@code size}, {@code isEmpty}, and {@code containsValue} are typically
 * useful only when a map is not undergoing concurrent updates in other threads.
 * Otherwise the results of these methods reflect transient states
 * that may be adequate for monitoring or estimation purposes, but not
 * for program control.
 *
 * <p>The table is dynamically expanded when there are too many
 * collisions (i.e., keys that have distinct hash codes but fall into
 * the same slot modulo the table size), with the expected average
 * effect of maintaining roughly two bins per mapping (corresponding
 * to a 0.75 load factor threshold for resizing). There may be much
 * variance around this average as mappings are added and removed, but
 * overall, this maintains a commonly accepted time/space tradeoff for
 * hash tables.  However, resizing this or any other kind of hash
 * table may be a relatively slow operation. When possible, it is a
 * good idea to provide a size estimate as an optional {@code
 * initialCapacity} constructor argument. An additional optional
 * {@code loadFactor} constructor argument provides a further means of
 * customizing initial table capacity by specifying the table density
 * to be used in calculating the amount of space to allocate for the
 * given number of elements.  Also, for compatibility with previous
 * versions of this class, constructors may optionally specify an
 * expected {@code concurrencyLevel} as an additional hint for
 * internal sizing.  Note that using many keys with exactly the same
 * {@code hashCode()} is a sure way to slow down performance of any
 * hash table. To ameliorate impact, when keys are {@link Comparable},
 * this class may use comparison order among keys to help break ties.
 *
 * <p>A {@link Set} projection of a ConcurrentSHMap may be created
 * (using {@link #newKeySet()} or {@link #newKeySet(int)}), or viewed
 * (using {@link #keySet(Object)} when only keys are of interest, and the
 * mapped values are (perhaps transiently) not used or all take the
 * same mapping value.
 *
 * <p>A ConcurrentSHMap can be used as scalable frequency map (a
 * form of histogram or multiset) by using {@link
 * java.util.concurrent.atomic.LongAdder} values and initializing via
 * {@link #computeIfAbsent computeIfAbsent}. For example, to add a count
 * to a {@code ConcurrentSHMap<String,LongAdder> freqs}, you can use
 * {@code freqs.computeIfAbsent(k -> new LongAdder()).increment();}
 *
 * <p>This class and its views and iterators implement all of the
 * <em>optional</em> methods of the {@link Map} and {@link Iterator}
 * interfaces.
 *
 * <p>Like {@link Hashtable} but unlike {@link HashMap}, this class
 * does <em>not</em> allow {@code null} to be used as a key or value.
 *
 * <p>ConcurrentSHMaps support a set of sequential and parallel bulk
 * operations that, unlike most {@link Stream} methods, are designed
 * to be safely, and often sensibly, applied even with maps that are
 * being concurrently updated by other threads; for example, when
 * computing a snapshot summary of the values in a shared registry.
 * There are three kinds of operation, each with four forms, accepting
 * functions with Keys, Values, Entries, and (Key, Value) arguments
 * and/or return values. Because the elements of a ConcurrentSHMap
 * are not ordered in any particular way, and may be processed in
 * different orders in different parallel executions, the correctness
 * of supplied functions should not depend on any ordering, or on any
 * other objects or values that may transiently change while
 * computation is in progress; and except for forEach actions, should
 * ideally be side-effect-free. Bulk operations on {@link java.util.Map.Entry}
 * objects do not support method {@code setValue}.
 *
 * <ul>
 * <li> forEach: Perform a given action on each element.
 * A variant form applies a given transformation on each element
 * before performing the action.</li>
 *
 * <li> search: Return the first available non-null result of
 * applying a given function on each element; skipping further
 * search when a result is found.</li>
 *
 * <li> reduce: Accumulate each element.  The supplied reduction
 * function cannot rely on ordering (more formally, it should be
 * both associative and commutative).  There are five variants:
 *
 * <ul>
 *
 * <li> Plain reductions. (There is not a form of this method for
 * (key, value) function arguments since there is no corresponding
 * return type.)</li>
 *
 * <li> Mapped reductions that accumulate the results of a given
 * function applied to each element.</li>
 *
 * <li> Reductions to scalar doubles, longs, and ints, using a
 * given basis value.</li>
 *
 * </ul>
 * </li>
 * </ul>
 *
 * <p>These bulk operations accept a {@code parallelismThreshold}
 * argument. Methods proceed sequentially if the current map size is
 * estimated to be less than the given threshold. Using a value of
 * {@code Long.MAX_VALUE} suppresses all parallelism.  Using a value
 * of {@code 1} results in maximal parallelism by partitioning into
 * enough subtasks to fully utilize the {@link
 * ForkJoinPool#commonPool()} that is used for all parallel
 * computations. Normally, you would initially choose one of these
 * extreme values, and then measure performance of using in-between
 * values that trade off overhead versus throughput.
 *
 * <p>The concurrency properties of bulk operations follow
 * from those of ConcurrentSHMap: Any non-null result returned
 * from {@code get(key)} and related access methods bears a
 * happens-before relation with the associated insertion or
 * update.  The result of any bulk operation reflects the
 * composition of these per-element relations (but is not
 * necessarily atomic with respect to the map as a whole unless it
 * is somehow known to be quiescent).  Conversely, because keys
 * and values in the map are never null, null serves as a reliable
 * atomic indicator of the current lack of any result.  To
 * maintain this property, null serves as an implicit basis for
 * all non-scalar reduction operations. For the double, long, and
 * int versions, the basis should be one that, when combined with
 * any other value, returns that other value (more formally, it
 * should be the identity element for the reduction). Most common
 * reductions have these properties; for example, computing a sum
 * with basis 0 or a minimum with basis MAX_VALUE.
 *
 * <p>Search and transformation functions provided as arguments
 * should similarly return null to indicate the lack of any result
 * (in which case it is not used). In the case of mapped
 * reductions, this also enables transformations to serve as
 * filters, returning null (or, in the case of primitive
 * specializations, the identity basis) if the element should not
 * be combined. You can create compound transformations and
 * filterings by composing them yourself under this "null means
 * there is nothing there now" rule before using them in search or
 * reduce operations.
 *
 * <p>Methods accepting and/or returning Entry arguments maintain
 * key-value associations. They may be useful for example when
 * finding the key for the greatest value. Note that "plain" Entry
 * arguments can be supplied using {@code new
 * AbstractMap.SimpleEntry(k,v)}.
 *
 * <p>Bulk operations may complete abruptly, throwing an
 * exception encountered in the application of a supplied
 * function. Bear in mind when handling such exceptions that other
 * concurrently executing functions could also have thrown
 * exceptions, or would have done so if the first exception had
 * not occurred.
 *
 * <p>Speedups for parallel compared to sequential forms are common
 * but not guaranteed.  Parallel operations involving brief functions
 * on small maps may execute more slowly than sequential forms if the
 * underlying work to parallelize the computation is more expensive
 * than the computation itself.  Similarly, parallelization may not
 * lead to much actual parallelism if all processors are busy
 * performing unrelated tasks.
 *
 * <p>All arguments to all task methods must be non-null.
 *
 * <p>This class is a member of the
 * <a href="{@docRoot}/../technotes/guides/collections/index.html">
 * Java Collections Framework</a>.
 *
 * @since 1.5
 * @author Doug Lea
 * @param <K> the type of keys maintained by this map
 * @param <V> the type of mapped values
 */

import Comp._

object Comp {

  def refEquals[A](x: A, y: A)(implicit eq: Equiv[A]) = {
    eq.equiv(x, y)
  }

  implicit def anyRefHasRefEquality[A <: AnyRef] = new Equiv[A] {
    def equiv(x: A, y: A) = x eq y
  }

  implicit def anyValHasUserEquality[A <: AnyVal] = new Equiv[A] {
    def equiv(x: A, y: A) = x == y
  }
}
object ConcurrentSHMap {
  /**
   * The largest possible table capacity.  This value must be
   * exactly 1<<30 to stay within Java array allocation and indexing
   * bounds for power of two table sizes, and is further required
   * because the top two bits of 32bit hash fields are used for
   * control purposes.
   */
  private val MAXIMUM_CAPACITY: Int = 1 << 30
  /**
   * The default initial table capacity.  Must be a power of 2
   * (i.e., at least 1) and at most MAXIMUM_CAPACITY.
   */
  private val DEFAULT_CAPACITY: Int = 16
  /**
   * The largest possible (non-power of two) array size.
   * Needed by toArray and related methods.
   */
  private[concurrent] val MAX_ARRAY_SIZE: Int = Integer.MAX_VALUE - 8
  /**
   * The default concurrency level for this table. Unused but
   * defined for compatibility with previous versions of this class.
   */
  private val DEFAULT_CONCURRENCY_LEVEL: Int = 16
  /**
   * The load factor for this table. Overrides of this value in
   * constructors affect only the initial table capacity.  The
   * actual floating point value isn't normally used -- it is
   * simpler to use expressions such as {@code n - (n >>> 2)} for
   * the associated resizing threshold.
   */
  private val LOAD_FACTOR: Float = 0.75f
  /**
   * The bin count threshold for using a tree rather than list for a
   * bin.  Bins are converted to trees when adding an element to a
   * bin with at least this many nodes. The value must be greater
   * than 2, and should be at least 8 to mesh with assumptions in
   * tree removal about conversion back to plain bins upon
   * shrinkage.
   */
  private[concurrent] val TREEIFY_THRESHOLD: Int = 8
  /**
   * The bin count threshold for untreeifying a (split) bin during a
   * resize operation. Should be less than TREEIFY_THRESHOLD, and at
   * most 6 to mesh with shrinkage detection under removal.
   */
  private[concurrent] val UNTREEIFY_THRESHOLD: Int = 6
  /**
   * The smallest table capacity for which bins may be treeified.
   * (Otherwise the table is resized if too many nodes in a bin.)
   * The value should be at least 4 * TREEIFY_THRESHOLD to avoid
   * conflicts between resizing and treeification thresholds.
   */
  private[concurrent] val MIN_TREEIFY_CAPACITY: Int = 64
  /**
   * Minimum number of rebinnings per transfer step. Ranges are
   * subdivided to allow multiple resizer threads.  This value
   * serves as a lower bound to avoid resizers encountering
   * excessive memory contention.  The value should be at least
   * DEFAULT_CAPACITY.
   */
  private val MIN_TRANSFER_STRIDE: Int = 16

  /**
   * The number of bits used for generation stamp in sizeCtl.
   * Must be at least 6 for 32bit arrays.
   */
  private val RESIZE_STAMP_BITS: Int = 16

  /**
   * The maximum number of threads that can help resize.
   * Must fit in 32 - RESIZE_STAMP_BITS bits.
   */
  private val MAX_RESIZERS: Int = (1 << (32 - RESIZE_STAMP_BITS)) - 1
  /**
   * The bit shift for recording size stamp in sizeCtl.
   */
  private val RESIZE_STAMP_SHIFT: Int = 32 - RESIZE_STAMP_BITS
  private[concurrent] val MOVED: Int = -1
  private[concurrent] val TREEBIN: Int = -2
  private[concurrent] val RESERVED: Int = -3
  private[concurrent] val HASH_BITS: Int = 0x7fffffff
  /** Number of CPUS, to place bounds on some sizings */
  private[concurrent] val NCPU: Int = Runtime.getRuntime.availableProcessors
  /** For serialization compatibility. */
  private val serialPersistentFields: Array[ObjectStreamField] = Array(new ObjectStreamField("segments", classOf[Array[ConcurrentSHMap.Segment[_, _]]]), new ObjectStreamField("segmentMask", Integer.TYPE), new ObjectStreamField("segmentShift", Integer.TYPE))

  /**
   * Key-value entry.  This class is never exported out as a
   * user-mutable Map.Entry (i.e., one supporting setValue; see
   * MapEntry below), but can be used for read-only traversals used
   * in bulk tasks.  Subclasses of Node with a negative hash field
   * are special, and contain null keys and values (but are never
   * exported).  Otherwise, keys and vals are never null.
   */
  private[concurrent] class Node[K, V](val hash: Int, val key: K, @volatile var value: V, @volatile var next: ConcurrentSHMap.Node[K, V]) extends Map.Entry[K, V] {

    final def getKey: K = {
      key
    }

    final def getValue: V = {
      value
    }

    final override def hashCode: Int = {
      key.hashCode ^ value.hashCode
    }

    final override def toString: String = {
      key + "=" + value
    }

    final def setValue(value: V): V = {
      throw new UnsupportedOperationException
    }

    final override def equals(o: Any): Boolean = {
      var k: K = null.asInstanceOf[K]
      var v: V = null.asInstanceOf[V]
      var u: V = null.asInstanceOf[V]
      var e: Map.Entry[K, V] = null
      (o.isInstanceOf[Map.Entry[_, _]] && {
        k = {
          e = o.asInstanceOf[Map.Entry[K, V]]; e
        }.getKey; k
      } != null && (({
        v = e.getValue; v
      })) != null && (refEquals(k, key) || (k == key)) && (refEquals(v, ({
        u = value; u
      })) || (v == u)))
    }

    /**
     * Virtualized support for map.get(); overridden in subclasses.
     */
    private[concurrent] def find(h: Int, k: Any): ConcurrentSHMap.Node[K, V] = {
      var e: ConcurrentSHMap.Node[K, V] = this
      if (k != null) {
        do {
          var ek: K = null.asInstanceOf[K]
          if (e.hash == h && (refEquals((({
            ek = e.key; ek
          })), k) || (ek != null && (k == ek)))) return e
        } while ((({
          e = e.next; e
        })) != null)
      }
      return null
    }
  }

  /**
   * Spreads (XORs) higher bits of hash to lower and also forces top
   * bit to 0. Because the table uses power-of-two masking, sets of
   * hashes that vary only in bits above the current mask will
   * always collide. (Among known examples are sets of Float keys
   * holding consecutive whole numbers in small tables.)  So we
   * apply a transform that spreads the impact of higher bits
   * downward. There is a tradeoff between speed, utility, and
   * quality of bit-spreading. Because many common sets of hashes
   * are already reasonably distributed (so don't benefit from
   * spreading), and because we use trees to handle large sets of
   * collisions in bins, we just XOR some shifted bits in the
   * cheapest possible way to reduce systematic lossage, as well as
   * to incorporate impact of the highest bits that would otherwise
   * never be used in index calculations because of table bounds.
   */
  private[concurrent] def spread(h: Int): Int = {
    return (h ^ (h >>> 16)) & HASH_BITS
  }

  /**
   * Returns a power of two table size for the given desired capacity.
   * See Hackers Delight, sec 3.2
   */
  private def tableSizeFor(c: Int): Int = {
    var n: Int = c - 1
    n |= n >>> 1
    n |= n >>> 2
    n |= n >>> 4
    n |= n >>> 8
    n |= n >>> 16
    return if ((n < 0)) 1 else if ((n >= MAXIMUM_CAPACITY)) MAXIMUM_CAPACITY else n + 1
  }

  /**
   * Returns x's Class if it is of the form "class C implements
   * Comparable<C>", else null.
   */
  private[concurrent] def comparableClassFor(x: Any): Class[_] = {
    if (x.isInstanceOf[Comparable[_]]) {
      var c: Class[_] = null
      var ts: Array[Type] = null
      var as: Array[Type] = null
      var t: Type = null
      var p: ParameterizedType = null
      if ((({
        c = x.getClass; c
      })) eq classOf[String]) return c
      if ((({
        ts = c.getGenericInterfaces; ts
      })) != null) {
        {
          var i: Int = 0
          while (i < ts.length) {
            {
              if (((({
                t = ts(i); t
              })).isInstanceOf[ParameterizedType]) && ((({
                p = t.asInstanceOf[ParameterizedType]; p
              })).getRawType eq classOf[Comparable[_]]) && ((({
                as = p.getActualTypeArguments; as
              })) != null) && (as.length == 1) && (as(0) eq c)) return c
            }
            ({
              i += 1; i
            })
          }
        }
      }
    }
    return null
  }

  /**
   * Returns k.compareTo(x) if x matches kc (k's screened comparable
   * class), else 0.
   */
  @SuppressWarnings(Array("rawtypes", "unchecked")) private[concurrent] def compareComparables(kc: Class[_], k: Any, x: Any): Int = {
    (if ((x == null) || (x.getClass ne kc)) 0 else (k.asInstanceOf[Comparable[Any]]).compareTo(x))
  }

  @SuppressWarnings(Array("unchecked")) private[concurrent] def tabAt[K, V](tab: Array[ConcurrentSHMap.Node[K, V]], i: Int): ConcurrentSHMap.Node[K, V] = {
    U.getObjectVolatile(tab, (i.toLong << ASHIFT) + ABASE).asInstanceOf[ConcurrentSHMap.Node[K, V]]
  }

  private[concurrent] def casTabAt[K, V](tab: Array[ConcurrentSHMap.Node[K, V]], i: Int, c: ConcurrentSHMap.Node[K, V], v: ConcurrentSHMap.Node[K, V]): Boolean = {
    U.compareAndSwapObject(tab, (i.toLong << ASHIFT) + ABASE, c, v)
  }

  private[concurrent] def setTabAt[K, V](tab: Array[ConcurrentSHMap.Node[K, V]], i: Int, v: ConcurrentSHMap.Node[K, V]) {
    U.putObjectVolatile(tab, (i.toLong << ASHIFT) + ABASE, v)
  }

  /**
   * Stripped-down version of helper class used in previous version,
   * declared for the sake of serialization compatibility
   */
  @SerialVersionUID(2249069246763182397L)
  private[concurrent] class Segment[K, V](val loadFactor: Float) extends ReentrantLock with Serializable

  /**
   * Creates a new {@link Set} backed by a ConcurrentSHMap
   * from the given type to {@code Boolean.TRUE}.
   *
   * @param <K> the element type of the returned set
   * @return the new set
   * @since 1.8
   */
  def newKeySet[K]: ConcurrentSHMap.KeySetView[K, Boolean] = {
    new ConcurrentSHMap.KeySetView[K, Boolean](new ConcurrentSHMap[K, Boolean], true)
  }

  /**
   * Creates a new {@link Set} backed by a ConcurrentSHMap
   * from the given type to {@code Boolean.TRUE}.
   *
   * @param initialCapacity The implementation performs internal
   *                        sizing to accommodate this many elements.
   * @param <K> the element type of the returned set
   * @return the new set
   * @throws IllegalArgumentException if the initial capacity of
   *                                  elements is negative
   * @since 1.8
   */
  def newKeySet[K](initialCapacity: Int): ConcurrentSHMap.KeySetView[K, Boolean] = {
    return new ConcurrentSHMap.KeySetView[K, Boolean](new ConcurrentSHMap[K, Boolean](initialCapacity), true)
  }

  /**
   * A node inserted at head of bins during transfer operations.
   */
  private[concurrent] final class ForwardingNode[K, V](val nextTable: Array[ConcurrentSHMap.Node[K, V]]) extends Node[K, V](MOVED, null.asInstanceOf[K], null.asInstanceOf[V], null) {

    private[concurrent] override def find(h: Int, k: Any): ConcurrentSHMap.Node[K, V] = {
      {
        var tab: Array[ConcurrentSHMap.Node[K, V]] = nextTable
        while (true) {
          var e: ConcurrentSHMap.Node[K, V] = null
          var n: Int = 0
          if (k == null || tab == null || (({
            n = tab.length; n
          })) == 0 || (({
            e = tabAt(tab, (n - 1) & h); e
          })) == null) return null
          while (true) {
            var continue = false
            var eh: Int = 0
            var ek: K = null.asInstanceOf[K]
            if ((({
              eh = e.hash; eh
            })) == h && (refEquals((({
              ek = e.key; ek
            })), k) || (ek != null && (k == ek)))) return e
            if (eh < 0) {
              if (e.isInstanceOf[ConcurrentSHMap.ForwardingNode[_, _]]) {
                tab = (e.asInstanceOf[ConcurrentSHMap.ForwardingNode[K, V]]).nextTable
                continue = true //todo: continue is not supported
              }
              else return e.find(h, k)
            }
            if(!continue) {
              if ((({
                e = e.next;
                e
              })) == null) return null
            }
          }
        }
      } //todo: labels is not supported
      null
    }
  }

  /**
   * A place-holder node used in computeIfAbsent and compute
   */
  private[concurrent] final class ReservationNode[K, V] extends Node[K, V](RESERVED, null.asInstanceOf[K], null.asInstanceOf[V], null) {
    private[concurrent] override def find(h: Int, k: Any): ConcurrentSHMap.Node[K, V] = {
      return null
    }
  }

  /**
   * Returns the stamp bits for resizing a table of size n.
   * Must be negative when shifted left by RESIZE_STAMP_SHIFT.
   */
  private[concurrent] def resizeStamp(n: Int): Int = {
    return Integer.numberOfLeadingZeros(n) | (1 << (RESIZE_STAMP_BITS - 1))
  }

  /**
   * A padded cell for distributing counts.  Adapted from LongAdder
   * and Striped64.  See their internal docs for explanation.
   */
  @sun.misc.Contended private[concurrent] final class CounterCell {
    @volatile
    private[concurrent] var value: Long = 0L

    private[concurrent] def this(x: Long) {
      this()
      value = x
    }
  }

  /**
   * Returns a list on non-TreeNodes replacing those in given list.
   */
  private[concurrent] def untreeify[K, V](b: ConcurrentSHMap.Node[K, V]): ConcurrentSHMap.Node[K, V] = {
    var hd: ConcurrentSHMap.Node[K, V] = null
    var tl: ConcurrentSHMap.Node[K, V] = null

    {
      var q: ConcurrentSHMap.Node[K, V] = b
      while (q != null) {
        {
          val p: ConcurrentSHMap.Node[K, V] = new ConcurrentSHMap.Node[K, V](q.hash, q.key, q.value, null)
          if (tl == null) hd = p
          else tl.next = p
          tl = p
        }
        q = q.next
      }
    }
    return hd
  }

  /**
   * Nodes for use in TreeBins
   */
  private[concurrent] final class TreeNode[K, V](hash: Int, key: K, value: V, next: ConcurrentSHMap.Node[K, V], var parent: ConcurrentSHMap.TreeNode[K, V]) extends Node[K, V](hash, key, value, next) {
    private[concurrent] var left: ConcurrentSHMap.TreeNode[K, V] = null
    private[concurrent] var right: ConcurrentSHMap.TreeNode[K, V] = null
    private[concurrent] var prev: ConcurrentSHMap.TreeNode[K, V] = null
    private[concurrent] var red: Boolean = false

    private[concurrent] override def find(h: Int, k: Any): ConcurrentSHMap.Node[K, V] = {
      return findTreeNode(h, k, null)
    }

    /**
     * Returns the TreeNode (or null if not found) for the given key
     * starting at given root.
     */
    private[concurrent] final def findTreeNode(h: Int, k: Any, kcIn: Class[_]): ConcurrentSHMap.TreeNode[K, V] = {
      var kc: Class[_] = kcIn
      if (k != null) {
        var p: ConcurrentSHMap.TreeNode[K, V] = this
        do {
          var ph: Int = 0
          var dir: Int = 0
          var pk: K = null.asInstanceOf[K]
          var q: ConcurrentSHMap.TreeNode[K, V] = null
          val pl: ConcurrentSHMap.TreeNode[K, V] = p.left
          val pr: ConcurrentSHMap.TreeNode[K, V] = p.right
          if ((({
            ph = p.hash; ph
          })) > h) p = pl
          else if (ph < h) p = pr
          else if (refEquals((({
            pk = p.key; pk
          })), k) || (pk != null && (k == pk))) return p
          else if (pl == null) p = pr
          else if (pr == null) p = pl
          else if ((kc != null || (({
            kc = comparableClassFor(k); kc
          })) != null) && (({
            dir = compareComparables(kc, k, pk); dir
          })) != 0) p = if ((dir < 0)) pl else pr
          else if ((({
            q = pr.findTreeNode(h, k, kc); q
          })) != null) return q
          else p = pl
        } while (p != null)
      }
      return null
    }
  }

  /**
   * TreeNodes used at the heads of bins. TreeBins do not hold user
   * keys or values, but instead point to list of TreeNodes and
   * their root. They also maintain a parasitic read-write lock
   * forcing writers (who hold bin lock) to wait for readers (who do
   * not) to complete before tree restructuring operations.
   */
  private[concurrent] object TreeBin {
    private[concurrent] val WRITER: Int = 1
    private[concurrent] val WAITER: Int = 2
    private[concurrent] val READER: Int = 4

    /**
     * Tie-breaking utility for ordering insertions when equal
     * hashCodes and non-comparable. We don't require a total
     * order, just a consistent insertion rule to maintain
     * equivalence across rebalancings. Tie-breaking further than
     * necessary simplifies testing a bit.
     */
    private[concurrent] def tieBreakOrder(a: Any, b: Any): Int = {
      var d: Int = 0
      if (a == null || b == null || (({
        d = a.getClass.getName.compareTo(b.getClass.getName); d
      })) == 0) d = (if (System.identityHashCode(a) <= System.identityHashCode(b)) -1 else 1)
      return d
    }

    private[concurrent] def rotateLeft[K, V](rootIn: ConcurrentSHMap.TreeNode[K, V], p: ConcurrentSHMap.TreeNode[K, V]): ConcurrentSHMap.TreeNode[K, V] = {
      var root: ConcurrentSHMap.TreeNode[K, V] = rootIn
      var r: ConcurrentSHMap.TreeNode[K, V] = null
      var pp: ConcurrentSHMap.TreeNode[K, V] = null
      var rl: ConcurrentSHMap.TreeNode[K, V] = null
      if (p != null && (({
        r = p.right; r
      })) != null) {
        if ((({
          rl = ({
            p.right = r.left; p.right
          }); rl
        })) != null) rl.parent = p
        if ((({
          pp = ({
            r.parent = p.parent; r.parent
          }); pp
        })) == null) (({
          root = r; root
        })).red = false
        else if (pp.left eq p) pp.left = r
        else pp.right = r
        r.left = p
        p.parent = r
      }
      return root
    }

    private[concurrent] def rotateRight[K, V](rootIn: ConcurrentSHMap.TreeNode[K, V], p: ConcurrentSHMap.TreeNode[K, V]): ConcurrentSHMap.TreeNode[K, V] = {
      var root: ConcurrentSHMap.TreeNode[K, V] = rootIn
      var l: ConcurrentSHMap.TreeNode[K, V] = null
      var pp: ConcurrentSHMap.TreeNode[K, V] = null
      var lr: ConcurrentSHMap.TreeNode[K, V] = null
      if (p != null && (({
        l = p.left; l
      })) != null) {
        if ((({
          lr = ({
            p.left = l.right; p.left
          }); lr
        })) != null) lr.parent = p
        if ((({
          pp = ({
            l.parent = p.parent; l.parent
          }); pp
        })) == null) (({
          root = l; root
        })).red = false
        else if (pp.right eq p) pp.right = l
        else pp.left = l
        l.right = p
        p.parent = l
      }
      return root
    }

    private[concurrent] def balanceInsertion[K, V](rootIn: ConcurrentSHMap.TreeNode[K, V], xIn: ConcurrentSHMap.TreeNode[K, V]): ConcurrentSHMap.TreeNode[K, V] = {
      var root: ConcurrentSHMap.TreeNode[K, V] = rootIn
      var x: ConcurrentSHMap.TreeNode[K, V] = xIn
      x.red = true

      var xp: ConcurrentSHMap.TreeNode[K, V] = null
      var xpp: ConcurrentSHMap.TreeNode[K, V] = null
      var xppl: ConcurrentSHMap.TreeNode[K, V] = null
      var xppr: ConcurrentSHMap.TreeNode[K, V] = null
      while (true) {
        if ((({
          xp = x.parent; xp
        })) == null) {
          x.red = false
          return x
        }
        else if (!xp.red || (({
          xpp = xp.parent; xpp
        })) == null) return root
        if (xp eq (({
          xppl = xpp.left; xppl
        }))) {
          if ((({
            xppr = xpp.right; xppr
          })) != null && xppr.red) {
            xppr.red = false
            xp.red = false
            xpp.red = true
            x = xpp
          }
          else {
            if (x eq xp.right) {
              root = rotateLeft(root, {x = xp; x})
              xpp = if ((({
                xp = x.parent; xp
              })) == null) null
              else xp.parent
            }
            if (xp != null) {
              xp.red = false
              if (xpp != null) {
                xpp.red = true
                root = rotateRight(root, xpp)
              }
            }
          }
        }
        else {
          if (xppl != null && xppl.red) {
            xppl.red = false
            xp.red = false
            xpp.red = true
            x = xpp
          }
          else {
            if (x eq xp.left) {
              root = rotateRight(root, {x = xp; x})
              xpp = if ((({
                xp = x.parent; xp
              })) == null) null
              else xp.parent
            }
            if (xp != null) {
              xp.red = false
              if (xpp != null) {
                xpp.red = true
                root = rotateLeft(root, xpp)
              }
            }
          }
        }
      }
      null
    }

    private[concurrent] def balanceDeletion[K, V](rootIn: ConcurrentSHMap.TreeNode[K, V], xIn: ConcurrentSHMap.TreeNode[K, V]): ConcurrentSHMap.TreeNode[K, V] = {
      var root: ConcurrentSHMap.TreeNode[K, V] = rootIn
      var x: ConcurrentSHMap.TreeNode[K, V] = xIn

      {
        var xp: ConcurrentSHMap.TreeNode[K, V] = null
        var xpl: ConcurrentSHMap.TreeNode[K, V] = null
        var xpr: ConcurrentSHMap.TreeNode[K, V] = null
        while (true) {
          if ((x == null) || (x eq root)) return root
          else if ((({
            xp = x.parent; xp
          })) == null) {
            x.red = false
            return x
          }
          else if (x.red) {
            x.red = false
            return root
          }
          else if ((({
            xpl = xp.left; xpl
          })) eq x) {
            if ((({
              xpr = xp.right; xpr
            })) != null && xpr.red) {
              xpr.red = false
              xp.red = true
              root = rotateLeft(root, xp)
              xpr = if ((({
                xp = x.parent; xp
              })) == null) null
              else xp.right
            }
            if (xpr == null) x = xp
            else {
              val sl: ConcurrentSHMap.TreeNode[K, V] = xpr.left
              var sr: ConcurrentSHMap.TreeNode[K, V] = xpr.right
              if ((sr == null || !sr.red) && (sl == null || !sl.red)) {
                xpr.red = true
                x = xp
              }
              else {
                if (sr == null || !sr.red) {
                  if (sl != null) sl.red = false
                  xpr.red = true
                  root = rotateRight(root, xpr)
                  xpr = if ((({
                    xp = x.parent; xp
                  })) == null) null
                  else xp.right
                }
                if (xpr != null) {
                  xpr.red = if ((xp == null)) false else xp.red
                  if ((({
                    sr = xpr.right; sr
                  })) != null) sr.red = false
                }
                if (xp != null) {
                  xp.red = false
                  root = rotateLeft(root, xp)
                }
                x = root
              }
            }
          }
          else {
            if (xpl != null && xpl.red) {
              xpl.red = false
              xp.red = true
              root = rotateRight(root, xp)
              xpl = if ((({
                xp = x.parent; xp
              })) == null) null
              else xp.left
            }
            if (xpl == null) x = xp
            else {
              var sl: ConcurrentSHMap.TreeNode[K, V] = xpl.left
              val sr: ConcurrentSHMap.TreeNode[K, V] = xpl.right
              if ((sl == null || !sl.red) && (sr == null || !sr.red)) {
                xpl.red = true
                x = xp
              }
              else {
                if (sl == null || !sl.red) {
                  if (sr != null) sr.red = false
                  xpl.red = true
                  root = rotateLeft(root, xpl)
                  xpl = if ((({
                    xp = x.parent; xp
                  })) == null) null
                  else xp.left
                }
                if (xpl != null) {
                  xpl.red = if ((xp == null)) false else xp.red
                  if ((({
                    sl = xpl.left; sl
                  })) != null) sl.red = false
                }
                if (xp != null) {
                  xp.red = false
                  root = rotateRight(root, xp)
                }
                x = root
              }
            }
          }
        }
      }
      null
    }

    /**
     * Recursive invariant check
     */
    private[concurrent] def checkInvariants[K, V](t: ConcurrentSHMap.TreeNode[K, V]): Boolean = {
      val tp: ConcurrentSHMap.TreeNode[K, V] = t.parent
      val tl: ConcurrentSHMap.TreeNode[K, V] = t.left
      val tr: ConcurrentSHMap.TreeNode[K, V] = t.right
      val tb: ConcurrentSHMap.TreeNode[K, V] = t.prev
      val tn: ConcurrentSHMap.TreeNode[K, V] = t.next.asInstanceOf[ConcurrentSHMap.TreeNode[K, V]]
      if ((tb != null) && (tb.next ne t)) return false
      if ((tn != null) && (tn.prev ne t)) return false
      if ((tp != null) && (t ne tp.left) && (t ne tp.right)) return false
      if ((tl != null) && ((tl.parent ne t) || (tl.hash > t.hash))) return false
      if ((tr != null) && ((tr.parent ne t) || (tr.hash < t.hash))) return false
      if (t.red && tl != null && tl.red && tr != null && tr.red) return false
      if (tl != null && !checkInvariants(tl)) return false
      if (tr != null && !checkInvariants(tr)) return false
      return true
    }

    private val U: Unsafe = MyThreadLocalRandom.getUnsafe
    val k: Class[_] = classOf[ConcurrentSHMap.TreeBin[_, _]]
    private val LOCKSTATE: Long = U.objectFieldOffset(k.getDeclaredField("lockState"))
  }

  private[concurrent] final class TreeBin[K, V] extends Node[K, V](TREEBIN, null.asInstanceOf[K], null.asInstanceOf[V], null) {
    private[concurrent] var root: ConcurrentSHMap.TreeNode[K, V] = null
    @volatile
    private[concurrent] var first: ConcurrentSHMap.TreeNode[K, V] = null
    @volatile
    private[concurrent] var waiter: Thread = null
    @volatile
    private[concurrent] var lockState: Int = 0

    /**
     * Creates bin with initial set of nodes headed by b.
     */
    private[concurrent] def this(b: ConcurrentSHMap.TreeNode[K, V]) {
      this()
      this.first = b
      var r: ConcurrentSHMap.TreeNode[K, V] = null

      {
        var x: ConcurrentSHMap.TreeNode[K, V] = b
        var next: ConcurrentSHMap.TreeNode[K, V] = null
        while (x != null) {
          {
            next = x.next.asInstanceOf[ConcurrentSHMap.TreeNode[K, V]]
            x.left = ({
              x.right = null; x.right
            })
            if (r == null) {
              x.parent = null
              x.red = false
              r = x
            }
            else {
              val k: K = x.key
              val h: Int = x.hash
              var kc: Class[_] = null

              {
                var p: ConcurrentSHMap.TreeNode[K, V] = r
                var break = false
                while (!break) {
                  var dir: Int = 0
                  var ph: Int = 0
                  val pk: K = p.key
                  if ((({
                    ph = p.hash; ph
                  })) > h) dir = -1
                  else if (ph < h) dir = 1
                  else if ((kc == null && (({
                    kc = comparableClassFor(k); kc
                  })) == null) || (({
                    dir = compareComparables(kc, k, pk); dir
                  })) == 0) dir = TreeBin.tieBreakOrder(k, pk)
                  val xp: ConcurrentSHMap.TreeNode[K, V] = p
                  if ((({
                    p = if ((dir <= 0)) p.left else p.right; p
                  })) == null) {
                    x.parent = xp
                    if (dir <= 0) xp.left = x
                    else xp.right = x
                    r = TreeBin.balanceInsertion(r, x)
                    break = true //todo: break is not supported
                  }
                }
              }
            }
          }
          x = next
        }
      }
      this.root = r
      assert(TreeBin.checkInvariants(root))
    }

    /**
     * Acquires write lock for tree restructuring.
     */
    private final def lockRoot {
      if (!TreeBin.U.compareAndSwapInt(this, TreeBin.LOCKSTATE, 0, TreeBin.WRITER)) contendedLock
    }

    /**
     * Releases write lock for tree restructuring.
     */
    private final def unlockRoot {
      lockState = 0
    }

    /**
     * Possibly blocks awaiting root lock.
     */
    private final def contendedLock {
      var waiting: Boolean = false

      {
        var s: Int = 0
        while (true) {
          if (((({
            s = lockState; s
          })) & ~TreeBin.WAITER) == 0) {
            if (TreeBin.U.compareAndSwapInt(this, TreeBin.LOCKSTATE, s, TreeBin.WRITER)) {
              if (waiting) waiter = null
              return
            }
          }
          else if ((s & TreeBin.WAITER) == 0) {
            if (TreeBin.U.compareAndSwapInt(this, TreeBin.LOCKSTATE, s, s | TreeBin.WAITER)) {
              waiting = true
              waiter = Thread.currentThread
            }
          }
          else if (waiting) LockSupport.park(this)
        }
      }
    }

    /**
     * Returns matching node or null if none. Tries to search
     * using tree comparisons from root, but continues linear
     * search when lock not available.
     */
    private[concurrent] final override def find(h: Int, k: Any): ConcurrentSHMap.Node[K, V] = {
      if (k != null) {
        {
          var e: ConcurrentSHMap.Node[K, V] = first
          while (e != null) {
            var s: Int = 0
            var ek: K = null.asInstanceOf[K]
            if (((({
              s = lockState; s
            })) & (TreeBin.WAITER | TreeBin.WRITER)) != 0) {
              if (e.hash == h && (refEquals((({
                ek = e.key; ek
              })), k) || (ek != null && (k == ek)))) return e
              e = e.next
            }
            else if (TreeBin.U.compareAndSwapInt(this, TreeBin.LOCKSTATE, s, s + TreeBin.READER)) {
              var r: ConcurrentSHMap.TreeNode[K, V] = null
              var p: ConcurrentSHMap.TreeNode[K, V] = null
              try {
                p = (if ((({
                  r = root; r
                })) == null) null
                else r.findTreeNode(h, k, null))
              } finally {
                var w: Thread = null
                if (TreeBin.U.getAndAddInt(this, TreeBin.LOCKSTATE, -TreeBin.READER) == (TreeBin.READER | TreeBin.WAITER) && (({
                  w = waiter; w
                })) != null) LockSupport.unpark(w)
              }
              return p
            }
          }
        }
      }
      return null
    }

    /**
     * Finds or adds a node.
     * @return null if added
     */
    private[concurrent] final def putTreeVal(h: Int, k: K, v: V): ConcurrentSHMap.TreeNode[K, V] = {
      var kc: Class[_] = null
      var searched: Boolean = false

      {
        var p: ConcurrentSHMap.TreeNode[K, V] = root
        var break = false
        while (!break) {
          var dir: Int = 0
          var ph: Int = 0
          var pk: K = null.asInstanceOf[K]
          if (p == null) {
            first = ({
              root = new ConcurrentSHMap.TreeNode[K, V](h, k, v, null, null); root
            })
            break = true //todo: break is not supported
          }
          else if ((({
            ph = p.hash; ph
          })) > h) dir = -1
          else if (ph < h) dir = 1
          else if (refEquals((({
            pk = p.key; pk
          })), k) || (pk != null && (k == pk))) return p
          else if ((kc == null && (({
            kc = comparableClassFor(k); kc
          })) == null) || (({
            dir = compareComparables(kc, k, pk); dir
          })) == 0) {
            if (!searched) {
              var q: ConcurrentSHMap.TreeNode[K, V] = null
              var ch: ConcurrentSHMap.TreeNode[K, V] = null
              searched = true
              if (((({
                ch = p.left; ch
              })) != null && (({
                q = ch.findTreeNode(h, k, kc); q
              })) != null) || ((({
                ch = p.right; ch
              })) != null && (({
                q = ch.findTreeNode(h, k, kc); q
              })) != null)) return q
            }
            dir = TreeBin.tieBreakOrder(k, pk)
          }
          if(!break) {
            val xp: ConcurrentSHMap.TreeNode[K, V] = p
            if ((({
              p = if ((dir <= 0)) p.left else p.right;
              p
            })) == null) {
              var x: ConcurrentSHMap.TreeNode[K, V] = null
              val f: ConcurrentSHMap.TreeNode[K, V] = first
              first = ({
                x = new ConcurrentSHMap.TreeNode[K, V](h, k, v, f, xp);
                x
              })
              if (f != null) f.prev = x
              if (dir <= 0) xp.left = x
              else xp.right = x
              if (!xp.red) x.red = true
              else {
                lockRoot
                try {
                  root = TreeBin.balanceInsertion(root, x)
                } finally {
                  unlockRoot
                }
              }
              break = true //todo: break is not supported
            }
          }
        }
      }
      assert(TreeBin.checkInvariants(root))
      return null
    }

    /**
     * Removes the given node, that must be present before this
     * call.  This is messier than typical red-black deletion code
     * because we cannot swap the contents of an interior node
     * with a leaf successor that is pinned by "next" pointers
     * that are accessible independently of lock. So instead we
     * swap the tree linkages.
     *
     * @return true if now too small, so should be untreeified
     */
    private[concurrent] final def removeTreeNode(p: ConcurrentSHMap.TreeNode[K, V]): Boolean = {
      val next: ConcurrentSHMap.TreeNode[K, V] = p.next.asInstanceOf[ConcurrentSHMap.TreeNode[K, V]]
      val pred: ConcurrentSHMap.TreeNode[K, V] = p.prev
      var r: ConcurrentSHMap.TreeNode[K, V] = null
      var rl: ConcurrentSHMap.TreeNode[K, V] = null
      if (pred == null) first = next
      else pred.next = next
      if (next != null) next.prev = pred
      if (first == null) {
        root = null
        return true
      }
      if ((({
        r = root; r
      })) == null || r.right == null || (({
        rl = r.left; rl
      })) == null || rl.left == null) return true
      lockRoot
      try {
        var replacement: ConcurrentSHMap.TreeNode[K, V] = null
        val pl: ConcurrentSHMap.TreeNode[K, V] = p.left
        val pr: ConcurrentSHMap.TreeNode[K, V] = p.right
        if (pl != null && pr != null) {
          var s: ConcurrentSHMap.TreeNode[K, V] = pr
          var sl: ConcurrentSHMap.TreeNode[K, V] = null
          while ((({
            sl = s.left; sl
          })) != null) s = sl
          val c: Boolean = s.red
          s.red = p.red
          p.red = c
          val sr: ConcurrentSHMap.TreeNode[K, V] = s.right
          val pp: ConcurrentSHMap.TreeNode[K, V] = p.parent
          if (s eq pr) {
            p.parent = s
            s.right = p
          }
          else {
            val sp: ConcurrentSHMap.TreeNode[K, V] = s.parent
            if ((({
              p.parent = sp; p.parent
            })) != null) {
              if (s eq sp.left) sp.left = p
              else sp.right = p
            }
            if ((({
              s.right = pr; s.right
            })) != null) pr.parent = s
          }
          p.left = null
          if ((({
            p.right = sr; p.right
          })) != null) sr.parent = p
          if ((({
            s.left = pl; s.left
          })) != null) pl.parent = s
          if ((({
            s.parent = pp; s.parent
          })) == null) r = s
          else if (p eq pp.left) pp.left = s
          else pp.right = s
          if (sr != null) replacement = sr
          else replacement = p
        }
        else if (pl != null) replacement = pl
        else if (pr != null) replacement = pr
        else replacement = p
        if (replacement ne p) {
          val pp: ConcurrentSHMap.TreeNode[K, V] = {replacement.parent = p.parent; replacement.parent}
          if (pp == null) r = replacement
          else if (p eq pp.left) pp.left = replacement
          else pp.right = replacement
          p.left = ({
            p.right = ({
              p.parent = null; p.parent
            }); p.right
          })
        }
        root = if ((p.red)) r else TreeBin.balanceDeletion(r, replacement)
        if (p eq replacement) {
          var pp: ConcurrentSHMap.TreeNode[K, V] = null
          if ((({
            pp = p.parent; pp
          })) != null) {
            if (p eq pp.left) pp.left = null
            else if (p eq pp.right) pp.right = null
            p.parent = null
          }
        }
      } finally {
        unlockRoot
      }
      assert(TreeBin.checkInvariants(root))
      return false
    }
  }

  /**
   * Records the table, its length, and current traversal index for a
   * traverser that must process a region of a forwarded table before
   * proceeding with current table.
   */
  private[concurrent] final class TableStack[K, V] {
    private[concurrent] var length: Int = 0
    private[concurrent] var index: Int = 0
    private[concurrent] var tab: Array[ConcurrentSHMap.Node[K, V]] = null
    private[concurrent] var next: ConcurrentSHMap.TableStack[K, V] = null
  }

  /**
   * Encapsulates traversal for methods such as containsValue; also
   * serves as a base class for other iterators and spliterators.
   *
   * Method advance visits once each still-valid node that was
   * reachable upon iterator construction. It might miss some that
   * were added to a bin after the bin was visited, which is OK wrt
   * consistency guarantees. Maintaining this property in the face
   * of possible ongoing resizes requires a fair amount of
   * bookkeeping state that is difficult to optimize away amidst
   * volatile accesses.  Even so, traversal maintains reasonable
   * throughput.
   *
   * Normally, iteration proceeds bin-by-bin traversing lists.
   * However, if the table has been resized, then all future steps
   * must traverse both the bin at the current index as well as at
   * (index + baseSize); and so on for further resizings. To
   * paranoically cope with potential sharing by users of iterators
   * across threads, iteration terminates if a bounds checks fails
   * for a table read.
   */
  private[concurrent] class Traverser[K, V](var tab: Array[ConcurrentSHMap.Node[K, V]], val baseSize: Int, var baseIndex: Int, var baseLimit: Int) {
    private[concurrent] var nextNode: ConcurrentSHMap.Node[K, V] = null
    private[concurrent] var stack: ConcurrentSHMap.TableStack[K, V] = null
    private[concurrent] var spare: ConcurrentSHMap.TableStack[K, V] = null
    private[concurrent] var index: Int = 0

    /**
     * Advances if possible, returning next valid node, or null if none.
     */
    private[concurrent] final def advance: ConcurrentSHMap.Node[K, V] = {
      var e: ConcurrentSHMap.Node[K, V] = null
      if ((({
        e = nextNode; e
      })) != null) e = e.next
      while (true) {
        var continue = false
        var t: Array[ConcurrentSHMap.Node[K, V]] = null
        var i: Int = 0
        var n: Int = 0
        if (e != null) return {nextNode = e; nextNode}
        if (baseIndex >= baseLimit || (({
          t = tab; t
        })) == null || (({
          n = t.length; n
        })) <= (({
          i = index; i
        })) || i < 0) return {nextNode = null; nextNode}
        if ((({
          e = tabAt(t, i); e
        })) != null && e.hash < 0) {
          if (e.isInstanceOf[ConcurrentSHMap.ForwardingNode[_, _]]) {
            tab = (e.asInstanceOf[ConcurrentSHMap.ForwardingNode[K, V]]).nextTable
            e = null
            pushState(t, i, n)
            continue = true //todo: continue is not supported
          }
          else if (e.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) e = (e.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]).first
          else e = null
        }
        if(!continue) {
          if (stack != null) recoverState(n)
          else if ((({
            index = i + baseSize;
            index
          })) >= n) index = ({
            baseIndex += 1;
            baseIndex
          })
        }
      }
      null.asInstanceOf[ConcurrentSHMap.Node[K, V]]
    }

    /**
     * Saves traversal state upon encountering a forwarding node.
     */
    private def pushState(t: Array[ConcurrentSHMap.Node[K, V]], i: Int, n: Int) {
      var s: ConcurrentSHMap.TableStack[K, V] = spare
      if (s != null) spare = s.next
      else s = new ConcurrentSHMap.TableStack[K, V]
      s.tab = t
      s.length = n
      s.index = i
      s.next = stack
      stack = s
    }

    /**
     * Possibly pops traversal state.
     *
     * @param n length of current table
     */
    private def recoverState(nIn: Int) {
      var n: Int = nIn
      var s: ConcurrentSHMap.TableStack[K, V] = null
      var len: Int = 0
      while ((({
        s = stack; s
      })) != null && (({
        index += (({
          len = s.length; len
        })); index
      })) >= n) {
        n = len
        index = s.index
        tab = s.tab
        s.tab = null
        val next: ConcurrentSHMap.TableStack[K, V] = s.next
        s.next = spare
        stack = next
        spare = s
      }
      if (s == null && (({
        index += baseSize; index
      })) >= n) index = ({
        baseIndex += 1; baseIndex
      })
    }
  }

  /**
   * Base of key, value, and entry Iterators. Adds fields to
   * Traverser to support iterator.remove.
   */
  private[concurrent] class BaseIterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, val map: ConcurrentSHMap[K, V]) extends Traverser[K, V](t, s, i, l) {
    advance

    private[concurrent] var lastReturned: ConcurrentSHMap.Node[K, V] = null

    final def hasNext: Boolean = {
      return nextNode != null
    }

    final def hasMoreElements: Boolean = {
      return nextNode != null
    }

//    final def remove = {
//      var p: ConcurrentSHMap.Node[K, V] = null
//      if ((({
//        p = lastReturned; p
//      })) == null) throw new IllegalStateException
//      lastReturned = null
//      map.replaceNode(p.key, null.asInstanceOf[V], null)
//      ()
//    }
  }

  private[concurrent] final class KeyIterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, m: ConcurrentSHMap[K, V]) extends BaseIterator[K, V](t, s, i, l,m) with Iterator[K] with Enumeration[K] {
    final def next: K = {
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = nextNode; p
      })) == null) throw new NoSuchElementException
      val k: K = p.key
      lastReturned = p
      advance
      return k
    }

    final def nextElement: K = {
      return next
    }
  }

  private[concurrent] final class ValueIterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, m: ConcurrentSHMap[K, V]) extends BaseIterator[K, V](t, s, i, l,m) with Iterator[V] with Enumeration[V] {

    final def next: V = {
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = nextNode; p
      })) == null) throw new NoSuchElementException
      val v: V = p.value
      lastReturned = p
      advance
      return v
    }

    final def nextElement: V = {
      return next
    }
  }

  private[concurrent] final class EntryIterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, m: ConcurrentSHMap[K, V]) extends BaseIterator[K, V](t, s, i, l,m) with Iterator[Map.Entry[K, V]] {

    final def next: Map.Entry[K, V] = {
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = nextNode; p
      })) == null) throw new NoSuchElementException
      val k: K = p.key
      val v: V = p.value
      lastReturned = p
      advance
      return new ConcurrentSHMap.MapEntry[K, V](k, v, map)
    }
  }

  /**
   * Exported Entry for EntryIterator
   */
  private[concurrent] final class MapEntry[K, V](final val key: K, var value: V, final val map: ConcurrentSHMap[K, V]) extends Map.Entry[K, V] {

    def getKey: K = {
      return key
    }

    def getValue: V = {
      return value
    }

    override def hashCode: Int = {
      return key.hashCode ^ value.hashCode
    }

    override def toString: String = {
      return key + "=" + value
    }

    override def equals(o: Any): Boolean = {
      var k: K = null.asInstanceOf[K]
      var v: V = null.asInstanceOf[V]
      var e: Map.Entry[K, V] = null
      return ((o.isInstanceOf[Map.Entry[_, _]]) && (({
        k = (({
          e = o.asInstanceOf[Map.Entry[K, V]]; e
        })).getKey; k
      })) != null && (({
        v = e.getValue; v
      })) != null && (refEquals(k, key) || (k == key)) && (refEquals(v, value) || (v == value)))
    }

    /**
     * Sets our entry's value and writes through to the map. The
     * value to return is somewhat arbitrary here. Since we do not
     * necessarily track asynchronous changes, the most recent
     * "previous" value could be different from what we return (or
     * could even have been removed, in which case the put will
     * re-establish). We do not and cannot guarantee more.
     */
    def setValue(newValue: V): V = {
      if (newValue == null) throw new NullPointerException
      val v: V = value
      value = newValue
      map.put(key, newValue)
      v
    }
  }

  private[concurrent] final class KeySpliterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, var est: Long) extends Traverser[K, V](t, s, i, l) with Spliterator[K] {

    def trySplit: Spliterator[K] = {
      var i: Int = 0
      var f: Int = 0
      var h: Int = 0
      return if ((({
        h = ((({
          i = baseIndex; i
        })) + (({
          f = baseLimit; f
        }))) >>> 1; h
      })) <= i) null
      else new ConcurrentSHMap.KeySpliterator[K, V](tab, baseSize, {baseLimit = h; baseLimit}, f, {est >>>= 1; est})
    }

    override def forEachRemaining(action: Consumer[_ >: K]) {
      if (action == null) throw new NullPointerException
      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = advance; p
        })) != null) action.accept(p.key)
      }
    }

    def tryAdvance(action: Consumer[_ >: K]): Boolean = {
      if (action == null) throw new NullPointerException
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = advance; p
      })) == null) return false
      action.accept(p.key)
      return true
    }

    def estimateSize: Long = {
      return est
    }

    def characteristics: Int = {
      return Spliterator.DISTINCT | Spliterator.CONCURRENT | Spliterator.NONNULL
    }
  }

  private[concurrent] final class ValueSpliterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, var est: Long) extends Traverser[K, V](t, s, i, l) with Spliterator[V] {

    def trySplit: Spliterator[V] = {
      var i: Int = 0
      var f: Int = 0
      var h: Int = 0
      return if ((({
        h = ((({
          i = baseIndex; i
        })) + (({
          f = baseLimit; f
        }))) >>> 1; h
      })) <= i) null
      else new ConcurrentSHMap.ValueSpliterator[K, V](tab, baseSize, {baseLimit = h; baseLimit}, f, {est >>>= 1; est})
    }

    override def forEachRemaining(action: Consumer[_ >: V]) {
      if (action == null) throw new NullPointerException
      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = advance; p
        })) != null) action.accept(p.value)
      }
    }

    def tryAdvance(action: Consumer[_ >: V]): Boolean = {
      if (action == null) throw new NullPointerException
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = advance; p
      })) == null) return false
      action.accept(p.value)
      return true
    }

    def estimateSize: Long = {
      return est
    }

    def characteristics: Int = {
      return Spliterator.CONCURRENT | Spliterator.NONNULL
    }
  }

  private[concurrent] final class EntrySpliterator[K, V](t: Array[ConcurrentSHMap.Node[K, V]], s: Int, i: Int, l: Int, var est: Long, val map: ConcurrentSHMap[K, V]) extends Traverser[K, V](t, s, i, l) with Spliterator[Map.Entry[K, V]] {

    def trySplit: Spliterator[Map.Entry[K, V]] = {
      var i: Int = 0
      var f: Int = 0
      var h: Int = 0
      return if ((({
        h = ((({
          i = baseIndex; i
        })) + (({
          f = baseLimit; f
        }))) >>> 1; h
      })) <= i) null
      else new ConcurrentSHMap.EntrySpliterator[K, V](tab, baseSize, {baseLimit = h; baseLimit}, f, {est >>>= 1; est}, map)
    }

    override def forEachRemaining(action: Consumer[_ >: Map.Entry[K, V]]) {
      if (action == null) throw new NullPointerException
      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = advance; p
        })) != null) action.accept(new ConcurrentSHMap.MapEntry[K, V](p.key, p.value, map))
      }
    }

    def tryAdvance(action: Consumer[_ >: Map.Entry[K, V]]): Boolean = {
      if (action == null) throw new NullPointerException
      var p: ConcurrentSHMap.Node[K, V] = null
      if ((({
        p = advance; p
      })) == null) return false
      action.accept(new ConcurrentSHMap.MapEntry[K, V](p.key, p.value, map))
      return true
    }

    def estimateSize: Long = {
      return est
    }

    def characteristics: Int = {
      return Spliterator.DISTINCT | Spliterator.CONCURRENT | Spliterator.NONNULL
    }
  }

  /**
   * Base class for views.
   */
  @SerialVersionUID(7249069246763182397L)
  private[concurrent] object CollectionView {
    private val oomeMsg: String = "Required array size too large"
  }

  @SerialVersionUID(7249069246763182397L)
  private[concurrent] abstract class CollectionView[K, V, E](val map: ConcurrentSHMap[K, V]) extends Collection[E] with java.io.Serializable {

    /**
     * Returns the map backing this view.
     *
     * @return the map backing this view
     */
    def getMap: ConcurrentSHMap[K, V] = {
      return map
    }

    /**
     * Removes all of the elements from this view, by removing all
     * the mappings from the map backing this view.
     */
    final def clear {
      map.clear
    }

    final def size: Int = {
      return map.size
    }

    final def isEmpty: Boolean = {
      return map.isEmpty
    }

    /**
     * Returns an iterator over the elements in this collection.
     *
     * <p>The returned iterator is
     * <a href="package-summary.html#Weakly"><i>weakly consistent</i></a>.
     *
     * @return an iterator over the elements in this collection
     */
    def iterator: Iterator[E]

    def contains(o: Any): Boolean

    def remove(o: Any): Boolean

    final override def toArray: Array[AnyRef] = {
//      val sz: Long = map.mappingCount
//      if (sz > MAX_ARRAY_SIZE) throw new OutOfMemoryError(CollectionView.oomeMsg)
//      var n: Int = sz.toInt
//      var r: Array[Any] = new Array[Any](n)
//      var i: Int = 0
//      import scala.collection.JavaConversions._
//      for (e <- this) {
//        if (i == n) {
//          if (n >= MAX_ARRAY_SIZE) throw new OutOfMemoryError(CollectionView.oomeMsg)
//          if (n >= MAX_ARRAY_SIZE - (MAX_ARRAY_SIZE >>> 1) - 1) n = MAX_ARRAY_SIZE
//          else n += (n >>> 1) + 1
//          r = Arrays.copyOf(r, n)
//        }
//        r(({
//          i += 1; i - 1
//        })) = e
//      }
//      return if ((i == n)) r else Arrays.copyOf(r, i)
      null
    }

    @SuppressWarnings(Array("unchecked")) final override def toArray[T](a: Array[T with Object]): Array[T with Object] = {
      //      val sz: Long = map.mappingCount
      //      if (sz > MAX_ARRAY_SIZE) throw new OutOfMemoryError(CollectionView.oomeMsg)
      //      val m: Int = sz.toInt
      //      var r: Array[T] = if ((a.length >= m)) a else java.lang.reflect.Array.newInstance(a.getClass.getComponentType, m).asInstanceOf[Array[T]]
      //      var n: Int = r.length
      //      var i: Int = 0
      //      import scala.collection.JavaConversions._
      //      for (e <- this) {
      //        if (i == n) {
      //          if (n >= MAX_ARRAY_SIZE) throw new OutOfMemoryError(CollectionView.oomeMsg)
      //          if (n >= MAX_ARRAY_SIZE - (MAX_ARRAY_SIZE >>> 1) - 1) n = MAX_ARRAY_SIZE
      //          else n += (n >>> 1) + 1
      //          r = Arrays.copyOf[T](r, n)
      //        }
      //        r(({
      //          i += 1; i - 1
      //        })) = e.asInstanceOf[T]
      //      }
      //      if ((a eq r) && (i < n)) {
      //        r(i) = null.asInstanceOf[T]
      //        return r
      //      }
      //      return if ((i == n)) r else Arrays.copyOf(r, i)
      null
    }

    /**
     * Returns a string representation of this collection.
     * The string representation consists of the string representations
     * of the collection's elements in the order they are returned by
     * its iterator, enclosed in square brackets ({@code "[]"}).
     * Adjacent elements are separated by the characters {@code ", "}
     * (comma and space).  Elements are converted to strings as by
     * {@link String#valueOf(Object)}.
     *
     * @return a string representation of this collection
     */
    final override def toString: String = {
      val sb: StringBuilder = new StringBuilder
      sb.append('[')
      val it: Iterator[E] = iterator
      if (it.hasNext) {
        var break = false
        while (!break) {
          val e = it.next
          sb.append(if (refEquals(e, this)) "(this Collection)" else e)
          if (!it.hasNext) break = true //todo: break is not supported
          if(!break) sb.append(',').append(' ')
        }
      }
      return sb.append(']').toString
    }

    final override def containsAll(c: Collection[_]): Boolean = {
      if (c ne this) {
        import scala.collection.JavaConversions._
        for (e <- c) {
          if (e == null || !contains(e)) return false
        }
      }
      return true
    }

    final def removeAll(c: Collection[_]): Boolean = {
      if (c == null) throw new NullPointerException
      var modified: Boolean = false

      {
        val it: Iterator[E] = iterator
        while (it.hasNext) {
          if (c.contains(it.next)) {
            it.remove
            modified = true
          }
        }
      }
      return modified
    }

    final def retainAll(c: Collection[_]): Boolean = {
      if (c == null) throw new NullPointerException
      var modified: Boolean = false

      {
        val it: Iterator[E] = iterator
        while (it.hasNext) {
          if (!c.contains(it.next)) {
            it.remove
            modified = true
          }
        }
      }
      return modified
    }
  }

  /**
   * A view of a ConcurrentSHMap as a {@link Set} of keys, in
   * which additions may optionally be enabled by mapping to a
   * common value.  This class cannot be directly instantiated.
   * See {@link #keySet() keySet()},
   * {@link #keySet(Object) keySet(V)},
   * {@link #newKeySet() newKeySet()},
   * {@link #newKeySet(int) newKeySet(int)}.
   *
   * @since 1.8
   */
  @SerialVersionUID(7249069246763182397L)
  class KeySetView[K, V](override val map: ConcurrentSHMap[K, V], final val value: V) extends CollectionView[K, V, K](map) with Set[K] with java.io.Serializable {

    /**
     * Returns the default mapped value for additions,
     * or {@code null} if additions are not supported.
     *
     * @return the default mapped value for additions, or { @code null}
     *                                                            if not supported
     */
    def getMappedValue: V = {
      return value
    }

    /**
     * {@inheritDoc}
     * @throws NullPointerException if the specified key is null
     */
    def contains(o: Any): Boolean = {
      return map.containsKey(o)
    }

    /**
     * Removes the key from this map view, by removing the key (and its
     * corresponding value) from the backing map.  This method does
     * nothing if the key is not in the map.
     *
     * @param  o the key to be removed from the backing map
     * @return { @code true} if the backing map contained the specified key
     * @throws NullPointerException if the specified key is null
     */
    def remove(o: Any): Boolean = {
      return map.remove(o) != null
    }

    /**
     * @return an iterator over the keys of the backing map
     */
    def iterator: Iterator[K] = {
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val m: ConcurrentSHMap[K, V] = map
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.KeyIterator[K, V](t, f, 0, f, m)
    }

    /**
     * Adds the specified key to this set view by mapping the key to
     * the default mapped value in the backing map, if defined.
     *
     * @param e key to be added
     * @return { @code true} if this set changed as a result of the call
     * @throws NullPointerException if the specified key is null
     * @throws UnsupportedOperationException if no default mapped value
     *                                       for additions was provided
     */
    def add(e: K): Boolean = {
      var v: V = null.asInstanceOf[V]
      if ((({
        v = value; v
      })) == null) throw new UnsupportedOperationException
      return map.putVal(e, v, true) == null
    }

    /**
     * Adds all of the elements in the specified collection to this set,
     * as if by calling {@link #add} on each one.
     *
     * @param c the elements to be inserted into this set
     * @return { @code true} if this set changed as a result of the call
     * @throws NullPointerException if the collection or any of its
     *                              elements are { @code null}
     * @throws UnsupportedOperationException if no default mapped value
     *                                       for additions was provided
     */
    def addAll(c: Collection[_ <: K]): Boolean = {
      var added: Boolean = false
      var v: V = null.asInstanceOf[V]
      if ((({
        v = value; v
      })) == null) throw new UnsupportedOperationException
      import scala.collection.JavaConversions._
      for (e <- c) {
        if (map.putVal(e, v, true) == null) added = true
      }
      return added
    }

    override def hashCode: Int = {
      var h: Int = 0
      import scala.collection.JavaConversions._
      for (e <- this) h += e.hashCode
      return h
    }

    override def equals(o: Any): Boolean = {
      var c: Set[_] = null
      return ((o.isInstanceOf[Set[_]]) && (((({
        c = o.asInstanceOf[Set[_]]; c
      })) eq this) || (containsAll(c) && c.containsAll(this))))
    }

    override def spliterator: Spliterator[K] = {
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val m: ConcurrentSHMap[K, V] = map
      val n: Long = m.sumCount
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.KeySpliterator[K, V](t, f, 0, f, if (n < 0L) 0L else n)
    }

    override def forEach(action: Consumer[_ >: K]) {
      if (action == null) throw new NullPointerException
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      if ((({
        t = map.table; t
      })) != null) {
        val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = it.advance; p
          })) != null) action.accept(p.key)
        }
      }
    }
  }

  /**
   * A view of a ConcurrentSHMap as a {@link Collection} of
   * values, in which additions are disabled. This class cannot be
   * directly instantiated. See {@link #values()}.
   */
  @SerialVersionUID(2249069246763182397L)
  private[concurrent] final class ValuesView[K, V](override val map: ConcurrentSHMap[K, V]) extends CollectionView[K, V, V](map) with Collection[V] with java.io.Serializable {

    final def contains(o: Any): Boolean = {
      return map.containsValue(o)
    }

    final def remove(o: Any): Boolean = {
      if (o != null) {
        {
          val it: Iterator[V] = iterator
          while (it.hasNext) {
            if (o == it.next) {
              it.remove
              return true
            }
          }
        }
      }
      return false
    }

    final def iterator: Iterator[V] = {
      val m: ConcurrentSHMap[K, V] = map
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.ValueIterator[K, V](t, f, 0, f, m)
    }

    final def add(e: V): Boolean = {
      throw new UnsupportedOperationException
    }

    final def addAll(c: Collection[_ <: V]): Boolean = {
      throw new UnsupportedOperationException
    }

    override def spliterator: Spliterator[V] = {
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val m: ConcurrentSHMap[K, V] = map
      val n: Long = m.sumCount
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.ValueSpliterator[K, V](t, f, 0, f, if (n < 0L) 0L else n)
    }

    override def forEach(action: Consumer[_ >: V]) {
      if (action == null) throw new NullPointerException
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      if ((({
        t = map.table; t
      })) != null) {
        val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = it.advance; p
          })) != null) action.accept(p.value)
        }
      }
    }
  }

  /**
   * A view of a ConcurrentSHMap as a {@link Set} of (key, value)
   * entries.  This class cannot be directly instantiated. See
   * {@link #entrySet()}.
   */
  @SerialVersionUID(2249069246763182397L)
  private[concurrent] final class EntrySetView[K, V](override val map: ConcurrentSHMap[K, V]) extends CollectionView[K, V, Map.Entry[K, V]](map) with Set[Map.Entry[K, V]] with java.io.Serializable {

    def contains(o: Any): Boolean = {
      var k: K = null.asInstanceOf[K]
      var v: V = null.asInstanceOf[V]
      var r: V = null.asInstanceOf[V]
      var e: Map.Entry[K, V] = null
      return ((o.isInstanceOf[Map.Entry[_, _]]) && (({
        k = (({
          e = o.asInstanceOf[Map.Entry[K, V]]; e
        })).getKey; k
      })) != null && (({
        r = map.get(k); r
      })) != null && (({
        v = e.getValue; v
      })) != null && (refEquals(v, r) || (v == r)))
    }

    def remove(o: Any): Boolean = {
      var k: K = null.asInstanceOf[K]
      var v: V = null.asInstanceOf[V]
      var e: Map.Entry[K, V] = null
      return ((o.isInstanceOf[Map.Entry[_, _]]) && (({
        k = (({
          e = o.asInstanceOf[Map.Entry[K, V]]; e
        })).getKey; k
      })) != null && (({
        v = e.getValue; v
      })) != null && map.remove(k, v))
    }

    /**
     * @return an iterator over the entries of the backing map
     */
    def iterator: Iterator[Map.Entry[K, V]] = {
      val m: ConcurrentSHMap[K, V] = map
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.EntryIterator[K, V](t, f, 0, f, m)
    }

    def add(e: Map.Entry[K, V]): Boolean = {
      return map.putVal(e.getKey, e.getValue, false) == null
    }

    def addAll(c: Collection[_ <: Map.Entry[K, V]]): Boolean = {
      var added: Boolean = false
      import scala.collection.JavaConversions._
      for (e <- c) {
        if (add(e)) added = true
      }
      return added
    }

    final override def hashCode: Int = {
      var h: Int = 0
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      if ((({
        t = map.table; t
      })) != null) {
        val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = it.advance; p
          })) != null) {
            h += p.hashCode
          }
        }
      }
      return h
    }

    final override def equals(o: Any): Boolean = {
      var c: Set[_] = null
      return ((o.isInstanceOf[Set[_]]) && (((({
        c = o.asInstanceOf[Set[_]]; c
      })) eq this) || (containsAll(c) && c.containsAll(this))))
    }

    override def spliterator: Spliterator[Map.Entry[K, V]] = {
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val m: ConcurrentSHMap[K, V] = map
      val n: Long = m.sumCount
      val f: Int = if ((({
        t = m.table; t
      })) == null) 0
      else t.length
      return new ConcurrentSHMap.EntrySpliterator[K, V](t, f, 0, f, if (n < 0L) 0L else n, m)
    }

    override def forEach(action: Consumer[_ >: Map.Entry[K, V]]) {
      if (action == null) throw new NullPointerException
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      if ((({
        t = map.table; t
      })) != null) {
        val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = it.advance; p
          })) != null) action.accept(new ConcurrentSHMap.MapEntry[K, V](p.key, p.value, map))
        }
      }
    }
  }

  /**
   * Base class for bulk tasks. Repeats some fields and code from
   * class Traverser, because we need to subclass CountedCompleter.
   */
  @SuppressWarnings(Array("serial")) private[concurrent] abstract class BulkTask[K, V, R](val par: ConcurrentSHMap.BulkTask[K, V, _], var batch: Int, var baseIndex: Int, f: Int, var tab: Array[ConcurrentSHMap.Node[K, V]]) extends CountedCompleter[R](par) {

    private[concurrent] var next: ConcurrentSHMap.Node[K, V] = null
    private[concurrent] var stack: ConcurrentSHMap.TableStack[K, V] = null
    private[concurrent] var spare: ConcurrentSHMap.TableStack[K, V] = null
    private[concurrent] var index: Int = baseIndex
    private[concurrent] var baseLimit: Int = if (tab == null) 0
    else if (par == null) tab.length
    else f
    private[concurrent] final val baseSize: Int = if (tab == null || par == null) this.baseLimit
    else par.baseSize

    /**
     * Same as Traverser version
     */
    private[concurrent] final def advance: ConcurrentSHMap.Node[K, V] = {
      var e: ConcurrentSHMap.Node[K, V] = null
      if ((({
        e = next; e
      })) != null) e = e.next
      while (true) {
        var continue = false
        var t: Array[ConcurrentSHMap.Node[K, V]] = null
        var i: Int = 0
        var n: Int = 0
        if (e != null) return { next = e; next }
        if (baseIndex >= baseLimit || (({
          t = tab; t
        })) == null || (({
          n = t.length; n
        })) <= (({
          i = index; i
        })) || i < 0) return { next = null; next }
        if ((({
          e = tabAt(t, i); e
        })) != null && e.hash < 0) {
          if (e.isInstanceOf[ConcurrentSHMap.ForwardingNode[_, _]]) {
            tab = (e.asInstanceOf[ConcurrentSHMap.ForwardingNode[K, V]]).nextTable
            e = null
            pushState(t, i, n)
            continue = true //todo: continue is not supported
          }
          else if (e.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) e = (e.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]).first
          else e = null
        }
        if(!continue) {
          if (stack != null) recoverState(n)
          else if ((({
            index = i + baseSize;
            index
          })) >= n) index = ({
            baseIndex += 1;
            baseIndex
          })
        }
      }
      null
    }

    private def pushState(t: Array[ConcurrentSHMap.Node[K, V]], i: Int, n: Int) {
      var s: ConcurrentSHMap.TableStack[K, V] = spare
      if (s != null) spare = s.next
      else s = new ConcurrentSHMap.TableStack[K, V]
      s.tab = t
      s.length = n
      s.index = i
      s.next = stack
      stack = s
    }

    private def recoverState(nIn: Int) {
      var n: Int = nIn
      var s: ConcurrentSHMap.TableStack[K, V] = null
      var len: Int = 0
      while ((({
        s = stack; s
      })) != null && (({
        index += (({
          len = s.length; len
        })); index
      })) >= n) {
        n = len
        index = s.index
        tab = s.tab
        s.tab = null
        val next: ConcurrentSHMap.TableStack[K, V] = s.next
        s.next = spare
        stack = next
        spare = s
      }
      if (s == null && (({
        index += baseSize; index
      })) >= n) index = ({
        baseIndex += 1; baseIndex
      })
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachKeyTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val action: Consumer[_ >: K]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val action: Consumer[_ >: K] = this.action
      if ((({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachKeyTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) action.accept(p.key)
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachValueTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val action: Consumer[_ >: V]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val action: Consumer[_ >: V] = this.action
      if ((({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachValueTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) action.accept(p.value)
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachEntryTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val action: Consumer[_ >: Map.Entry[K, V]]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val action: Consumer[_ >: Map.Entry[K, V]] = this.action
      if ((({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachEntryTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) action.accept(p)
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachMappingTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val action: BiConsumer[_ >: K, _ >: V]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val action: BiConsumer[_ >: K, _ >: V] = this.action
      if ((({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachMappingTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) action.accept(p.key, p.value)
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachTransformedKeyTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val transformer: Function[_ >: K, _ <: U], final val action: Consumer[_ >: U]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val transformer: Function[_ >: K, _ <: U] = this.transformer
      val action: Consumer[_ >: U] = this.action
      if ((({
        transformer
      })) != null && (({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachTransformedKeyTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, transformer, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.key); u
            })) != null) action.accept(u)
          }
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachTransformedValueTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val transformer: Function[_ >: V, _ <: U], final val action: Consumer[_ >: U]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val transformer: Function[_ >: V, _ <: U] = this.transformer
      val action: Consumer[_ >: U] = this.action
      if ((({
        transformer
      })) != null && (({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachTransformedValueTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, transformer, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.value); u
            })) != null) action.accept(u)
          }
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachTransformedEntryTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val transformer: Function[Map.Entry[K, V], _ <: U], final val action: Consumer[_ >: U]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val transformer: Function[Map.Entry[K, V], _ <: U] = this.transformer
      val action: Consumer[_ >: U] = this.action
      if ((({
        transformer
      })) != null && (({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachTransformedEntryTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, transformer, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p); u
            })) != null) action.accept(u)
          }
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ForEachTransformedMappingTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val transformer: BiFunction[_ >: K, _ >: V, _ <: U], final val action: Consumer[_ >: U]) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val transformer: BiFunction[_ >: K, _ >: V, _ <: U] = this.transformer
      val action: Consumer[_ >: U] = this.action
      if ((({
        transformer
      })) != null && (({
        action
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            new ConcurrentSHMap.ForEachTransformedMappingTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, transformer, action).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.key, p.value); u
            })) != null) action.accept(u)
          }
        }
        propagateCompletion
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class SearchKeysTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val searchFunction: Function[_ >: K, _ <: U], final val result: AtomicReference[U]) extends BulkTask[K, V, U](p,b,i,f,t) {

    final override def getRawResult: U = {
      return result.get
    }

    final def compute {
      val searchFunction: Function[_ >: K, _ <: U] = this.searchFunction
      val result: AtomicReference[U] = this.result
      if ((({
        searchFunction
      })) != null && (({
        result
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            if (result.get != null) return
            addToPendingCount(1)
            new ConcurrentSHMap.SearchKeysTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, searchFunction, result).fork
          }
        }
        var break = false
        while (!break && (result.get == null)) {
          var u: U = null.asInstanceOf[U]
          var p: ConcurrentSHMap.Node[K, V] = null
          if ((({
            p = advance; p
          })) == null) {
            propagateCompletion
            break = true //todo: break is not supported
          }
          if(!break) {
            if ((({
              u = searchFunction.apply(p.key);
              u
            })) != null) {
              if (result.compareAndSet(null.asInstanceOf[U], u)) quietlyCompleteRoot
              break = true //todo: break is not supported
            }
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class SearchValuesTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val searchFunction: Function[_ >: V, _ <: U], final val result: AtomicReference[U]) extends BulkTask[K, V, U](p,b,i,f,t) {

    final override def getRawResult: U = {
      return result.get
    }

    final def compute {
      val searchFunction: Function[_ >: V, _ <: U] = this.searchFunction
      val result: AtomicReference[U] = this.result
      if ((({
        searchFunction
      })) != null && (({
        result
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            if (result.get != null) return
            addToPendingCount(1)
            new ConcurrentSHMap.SearchValuesTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, searchFunction, result).fork
          }
        }
        var break = false
        while (!break && (result.get == null)) {
          var u: U = null.asInstanceOf[U]
          var p: ConcurrentSHMap.Node[K, V] = null
          if ((({
            p = advance; p
          })) == null) {
            propagateCompletion
            break = true //todo: break is not supported
          }
          if(!break) {
            if ((({
              u = searchFunction.apply(p.value);
              u
            })) != null) {
              if (result.compareAndSet(null.asInstanceOf[U], u)) quietlyCompleteRoot
              break = true //todo: break is not supported
            }
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class SearchEntriesTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val searchFunction: Function[Map.Entry[K, V], _ <: U], final val result: AtomicReference[U]) extends BulkTask[K, V, U](p,b,i,f,t) {

    final override def getRawResult: U = {
      return result.get
    }

    final def compute {
      val searchFunction: Function[Map.Entry[K, V], _ <: U] = this.searchFunction
      val result: AtomicReference[U] = this.result
      if ((({
        searchFunction
      })) != null && (({
        result
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            if (result.get != null) return
            addToPendingCount(1)
            new ConcurrentSHMap.SearchEntriesTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, searchFunction, result).fork
          }
        }
        var break = false
        while (!break && (result.get == null)) {
          var u: U = null.asInstanceOf[U]
          var p: ConcurrentSHMap.Node[K, V] = null
          if ((({
            p = advance; p
          })) == null) {
            propagateCompletion
            break = true //todo: break is not supported
          }
          if(!break) {
            if ((({
              u = searchFunction.apply(p);
              u
            })) != null) {
              if (result.compareAndSet(null.asInstanceOf[U], u)) quietlyCompleteRoot
              break = true // return
            }
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class SearchMappingsTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val searchFunction: BiFunction[_ >: K, _ >: V, _ <: U], final val result: AtomicReference[U]) extends BulkTask[K, V, U](p,b,i,f,t) {

    final override def getRawResult: U = {
      return result.get
    }

    final def compute {
      val searchFunction: BiFunction[_ >: K, _ >: V, _ <: U] = this.searchFunction
      val result: AtomicReference[U] = this.result
      if ((({
        searchFunction
      })) != null && (({
        result
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            if (result.get != null) return
            addToPendingCount(1)
            new ConcurrentSHMap.SearchMappingsTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, searchFunction, result).fork
          }
        }
        var break = false
        while (!break && (result.get == null)) {
          var u: U = null.asInstanceOf[U]
          var p: ConcurrentSHMap.Node[K, V] = null
          if ((({
            p = advance; p
          })) == null) {
            propagateCompletion
            break = true //todo: break is not supported
          }
          if(!break) {
            if ((({
              u = searchFunction.apply(p.key, p.value);
              u
            })) != null) {
              if (result.compareAndSet(null.asInstanceOf[U], u)) quietlyCompleteRoot
              break = true //todo: break is not supported
            }
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ReduceKeysTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.ReduceKeysTask[K, V], final val reducer: BiFunction[_ >: K, _ >: K, _ <: K]) extends BulkTask[K, V, K](p,b,i,f,t) {
    private[concurrent] var result: K = null.asInstanceOf[K]
    private[concurrent] var rights: ConcurrentSHMap.ReduceKeysTask[K, V] = null

    final override def getRawResult: K = {
      return result
    }

    final def compute {
      val reducer: BiFunction[_ >: K, _ >: K, _ <: K] = this.reducer
      if ((({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.ReduceKeysTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, reducer); rights
            })).fork
          }
        }
        var r: K = null.asInstanceOf[K]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            val u: K = p.key
            r = if ((r == null)) u else if (u == null) r else reducer.apply(r, u)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.ReduceKeysTask[K, V] = c.asInstanceOf[ConcurrentSHMap.ReduceKeysTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.ReduceKeysTask[K, V] = t.rights
              while (s != null) {
                var tr: K = null.asInstanceOf[K]
                var sr: K = null.asInstanceOf[K]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ReduceValuesTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.ReduceValuesTask[K, V], final val reducer: BiFunction[_ >: V, _ >: V, _ <: V]) extends BulkTask[K, V, V](p,b,i,f,t) {
    private[concurrent] var result: V = null.asInstanceOf[V]
    private[concurrent] var rights: ConcurrentSHMap.ReduceValuesTask[K, V] = null

    final override def getRawResult: V = {
      return result
    }

    final def compute {
      val reducer: BiFunction[_ >: V, _ >: V, _ <: V] = this.reducer
      if ((({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.ReduceValuesTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, reducer); rights
            })).fork
          }
        }
        var r: V = null.asInstanceOf[V]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            val v: V = p.value
            r = if ((r == null)) v else reducer.apply(r, v)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.ReduceValuesTask[K, V] = c.asInstanceOf[ConcurrentSHMap.ReduceValuesTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.ReduceValuesTask[K, V] = t.rights
              while (s != null) {
                var tr: V = null.asInstanceOf[V]
                var sr: V = null.asInstanceOf[V]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class ReduceEntriesTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.ReduceEntriesTask[K, V], final val reducer: BiFunction[Map.Entry[K, V], Map.Entry[K, V], _ <: Map.Entry[K, V]]) extends BulkTask[K, V, Map.Entry[K, V]](p,b,i,f,t) {
    private[concurrent] var result: Map.Entry[K, V] = null
    private[concurrent] var rights: ConcurrentSHMap.ReduceEntriesTask[K, V] = null

    final override def getRawResult: Map.Entry[K, V] = {
      return result
    }

    final def compute {
      val reducer: BiFunction[Map.Entry[K, V], Map.Entry[K, V], _ <: Map.Entry[K, V]] = this.reducer
      if ((({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.ReduceEntriesTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, reducer); rights
            })).fork
          }
        }
        var r: Map.Entry[K, V] = null.asInstanceOf[Map.Entry[K, V]]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = if ((r == null)) p else reducer.apply(r, p)
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.ReduceEntriesTask[K, V] = c.asInstanceOf[ConcurrentSHMap.ReduceEntriesTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.ReduceEntriesTask[K, V] = t.rights
              while (s != null) {
                var tr: Map.Entry[K, V] = null.asInstanceOf[Map.Entry[K, V]]
                var sr: Map.Entry[K, V] = null.asInstanceOf[Map.Entry[K, V]]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceKeysTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceKeysTask[K, V, U], final val transformer: Function[_ >: K, _ <: U], final val reducer: BiFunction[_ >: U, _ >: U, _ <: U]) extends BulkTask[K, V, U](p,b,i,f,t) {
    private[concurrent] var result: U = null.asInstanceOf[U]
    private[concurrent] var rights: ConcurrentSHMap.MapReduceKeysTask[K, V, U] = null

    final override def getRawResult: U = {
      return result
    }

    final def compute {
      val transformer: Function[_ >: K, _ <: U] = this.transformer
      val reducer: BiFunction[_ >: U, _ >: U, _ <: U] = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceKeysTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, reducer); rights
            })).fork
          }
        }
        var r: U = null.asInstanceOf[U]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.key); u
            })) != null) r = if ((r == null)) u else reducer.apply(r, u)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceKeysTask[K, V, U] = c.asInstanceOf[ConcurrentSHMap.MapReduceKeysTask[K, V, U]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceKeysTask[K, V, U] = t.rights
              while (s != null) {
                var tr: U = null.asInstanceOf[U]
                var sr: U = null.asInstanceOf[U]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceValuesTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceValuesTask[K, V, U], final val transformer: Function[_ >: V, _ <: U], final val reducer: BiFunction[_ >: U, _ >: U, _ <: U]) extends BulkTask[K, V, U](p,b,i,f,t) {
    private[concurrent] var result: U = null.asInstanceOf[U]
    private[concurrent] var rights: ConcurrentSHMap.MapReduceValuesTask[K, V, U] = null

    final override def getRawResult: U = {
      return result
    }

    final def compute {
      val transformer: Function[_ >: V, _ <: U] = this.transformer
      val reducer: BiFunction[_ >: U, _ >: U, _ <: U] = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceValuesTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, reducer); rights
            })).fork
          }
        }
        var r: U = null.asInstanceOf[U]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.value); u
            })) != null) r = if ((r == null)) u else reducer.apply(r, u)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceValuesTask[K, V, U] = c.asInstanceOf[ConcurrentSHMap.MapReduceValuesTask[K, V, U]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceValuesTask[K, V, U] = t.rights
              while (s != null) {
                var tr: U = null.asInstanceOf[U]
                var sr: U = null.asInstanceOf[U]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceEntriesTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceEntriesTask[K, V, U], final val transformer: Function[Map.Entry[K, V], _ <: U], final val reducer: BiFunction[_ >: U, _ >: U, _ <: U]) extends BulkTask[K, V, U](p,b,i,f,t) {
    private[concurrent] var result: U = null.asInstanceOf[U]
    private[concurrent] var rights: ConcurrentSHMap.MapReduceEntriesTask[K, V, U] = null

    final override def getRawResult: U = {
      return result
    }

    final def compute {
      val transformer: Function[Map.Entry[K, V], _ <: U] = this.transformer
      val reducer: BiFunction[_ >: U, _ >: U, _ <: U] = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceEntriesTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, reducer); rights
            })).fork
          }
        }
        var r: U = null.asInstanceOf[U]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p); u
            })) != null) r = if ((r == null)) u else reducer.apply(r, u)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceEntriesTask[K, V, U] = c.asInstanceOf[ConcurrentSHMap.MapReduceEntriesTask[K, V, U]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceEntriesTask[K, V, U] = t.rights
              while (s != null) {
                var tr: U = null.asInstanceOf[U]
                var sr: U = null.asInstanceOf[U]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceMappingsTask[K, V, U](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceMappingsTask[K, V, U], final val transformer: BiFunction[_ >: K, _ >: V, _ <: U], final val reducer: BiFunction[_ >: U, _ >: U, _ <: U]) extends BulkTask[K, V, U](p,b,i,f,t) {
    private[concurrent] var result: U = null.asInstanceOf[U]
    private[concurrent] var rights: ConcurrentSHMap.MapReduceMappingsTask[K, V, U] = null

    final override def getRawResult: U = {
      return result
    }

    final def compute {
      val transformer: BiFunction[_ >: K, _ >: V, _ <: U] = this.transformer
      val reducer: BiFunction[_ >: U, _ >: U, _ <: U] = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceMappingsTask[K, V, U](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, reducer); rights
            })).fork
          }
        }
        var r: U = null.asInstanceOf[U]

        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) {
            var u: U = null.asInstanceOf[U]
            if ((({
              u = transformer.apply(p.key, p.value); u
            })) != null) r = if ((r == null)) u else reducer.apply(r, u)
          }
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceMappingsTask[K, V, U] = c.asInstanceOf[ConcurrentSHMap.MapReduceMappingsTask[K, V, U]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceMappingsTask[K, V, U] = t.rights
              while (s != null) {
                var tr: U = null.asInstanceOf[U]
                var sr: U = null.asInstanceOf[U]
                if ((({
                  sr = s.result; sr
                })) != null) t.result = (if (((({
                  tr = t.result; tr
                })) == null)) sr
                else reducer.apply(tr, sr))
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceKeysToDoubleTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V], final val transformer: ToDoubleFunction[_ >: K], final val basis: Double, final val reducer: DoubleBinaryOperator) extends BulkTask[K, V, Double](p,b,i,f,t) {
    private[concurrent] var result: Double = .0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V] = null

    final override def getRawResult: Double = {
      return result
    }

    final def compute {
      val transformer: ToDoubleFunction[_ >: K] = this.transformer
      val reducer: DoubleBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Double = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsDouble(r, transformer.applyAsDouble(p.key))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsDouble(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceValuesToDoubleTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V], final val transformer: ToDoubleFunction[_ >: V], final val basis: Double, final val reducer: DoubleBinaryOperator) extends BulkTask[K, V, Double](p,b,i,f,t) {
    private[concurrent] var result: Double = .0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V] = null

    final override def getRawResult: Double = {
      return result
    }

    final def compute {
      val transformer: ToDoubleFunction[_ >: V] = this.transformer
      val reducer: DoubleBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Double = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsDouble(r, transformer.applyAsDouble(p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsDouble(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceEntriesToDoubleTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V], final val transformer: ToDoubleFunction[Map.Entry[K, V]], final val basis: Double, final val reducer: DoubleBinaryOperator) extends BulkTask[K, V, Double](p,b,i,f,t) {
    private[concurrent] var result: Double = .0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V] = null

    final override def getRawResult: Double = {
      return result
    }

    final def compute {
      val transformer: ToDoubleFunction[Map.Entry[K, V]] = this.transformer
      val reducer: DoubleBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Double = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsDouble(r, transformer.applyAsDouble(p))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsDouble(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceMappingsToDoubleTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V], final val transformer: ToDoubleBiFunction[_ >: K, _ >: V], final val basis: Double, final val reducer: DoubleBinaryOperator) extends BulkTask[K, V, Double](p,b,i,f,t) {
    private[concurrent] var result: Double = .0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V] = null

    final override def getRawResult: Double = {
      return result
    }

    final def compute {
      val transformer: ToDoubleBiFunction[_ >: K, _ >: V] = this.transformer
      val reducer: DoubleBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Double = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsDouble(r, transformer.applyAsDouble(p.key, p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsDouble(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceKeysToLongTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceKeysToLongTask[K, V], final val transformer: ToLongFunction[_ >: K], final val basis: Long, final val reducer: LongBinaryOperator) extends BulkTask[K, V, Long](p,b,i,f,t) {
    private[concurrent] var result: Long = 0L
    private[concurrent] var rights: ConcurrentSHMap.MapReduceKeysToLongTask[K, V] = null

    final override def getRawResult: Long = {
      return result
    }

    final def compute {
      val transformer: ToLongFunction[_ >: K] = this.transformer
      val reducer: LongBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Long = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceKeysToLongTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsLong(r, transformer.applyAsLong(p.key))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceKeysToLongTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceKeysToLongTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceKeysToLongTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsLong(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceValuesToLongTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceValuesToLongTask[K, V], final val transformer: ToLongFunction[_ >: V], final val basis: Long, final val reducer: LongBinaryOperator) extends BulkTask[K, V, Long](p,b,i,f,t) {
    private[concurrent] var result: Long = 0L
    private[concurrent] var rights: ConcurrentSHMap.MapReduceValuesToLongTask[K, V] = null

    final override def getRawResult: Long = {
      return result
    }

    final def compute {
      val transformer: ToLongFunction[_ >: V] = this.transformer
      val reducer: LongBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Long = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceValuesToLongTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsLong(r, transformer.applyAsLong(p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceValuesToLongTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceValuesToLongTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceValuesToLongTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsLong(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceEntriesToLongTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceEntriesToLongTask[K, V], final val transformer: ToLongFunction[Map.Entry[K, V]], final val basis: Long, final val reducer: LongBinaryOperator) extends BulkTask[K, V, Long](p,b,i,f,t) {
    private[concurrent] var result: Long = 0L
    private[concurrent] var rights: ConcurrentSHMap.MapReduceEntriesToLongTask[K, V] = null

    final override def getRawResult: Long = {
      return result
    }

    final def compute {
      val transformer: ToLongFunction[Map.Entry[K, V]] = this.transformer
      val reducer: LongBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Long = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceEntriesToLongTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsLong(r, transformer.applyAsLong(p))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceEntriesToLongTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceEntriesToLongTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceEntriesToLongTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsLong(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceMappingsToLongTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceMappingsToLongTask[K, V], final val transformer: ToLongBiFunction[_ >: K, _ >: V], final val basis: Long, final val reducer: LongBinaryOperator) extends BulkTask[K, V, Long](p,b,i,f,t) {
    private[concurrent] var result: Long = 0L
    private[concurrent] var rights: ConcurrentSHMap.MapReduceMappingsToLongTask[K, V] = null

    final override def getRawResult: Long = {
      return result
    }

    final def compute {
      val transformer: ToLongBiFunction[_ >: K, _ >: V] = this.transformer
      val reducer: LongBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Long = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceMappingsToLongTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsLong(r, transformer.applyAsLong(p.key, p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceMappingsToLongTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceMappingsToLongTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceMappingsToLongTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsLong(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceKeysToIntTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceKeysToIntTask[K, V], final val transformer: ToIntFunction[_ >: K], final val basis: Int, final val reducer: IntBinaryOperator) extends BulkTask[K, V, Integer](p,b,i,f,t) {
    private[concurrent] var result: Int = 0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceKeysToIntTask[K, V] = null

    final override def getRawResult: Integer = {
      return result
    }

    final def compute {
      val transformer: ToIntFunction[_ >: K] = this.transformer
      val reducer: IntBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Int = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceKeysToIntTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsInt(r, transformer.applyAsInt(p.key))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceKeysToIntTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceKeysToIntTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceKeysToIntTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsInt(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceValuesToIntTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceValuesToIntTask[K, V], final val transformer: ToIntFunction[_ >: V], final val basis: Int, final val reducer: IntBinaryOperator) extends BulkTask[K, V, Integer](p,b,i,f,t) {
    private[concurrent] var result: Int = 0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceValuesToIntTask[K, V] = null

    final override def getRawResult: Integer = {
      return result
    }

    final def compute {
      val transformer: ToIntFunction[_ >: V] = this.transformer
      val reducer: IntBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Int = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceValuesToIntTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsInt(r, transformer.applyAsInt(p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceValuesToIntTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceValuesToIntTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceValuesToIntTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsInt(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceEntriesToIntTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceEntriesToIntTask[K, V], final val transformer: ToIntFunction[Map.Entry[K, V]], final val basis: Int, final val reducer: IntBinaryOperator) extends BulkTask[K, V, Integer](p,b,i,f,t) {
    private[concurrent] var result: Int = 0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceEntriesToIntTask[K, V] = null

    final override def getRawResult: Integer = {
      return result
    }

    final def compute {
      val transformer: ToIntFunction[Map.Entry[K, V]] = this.transformer
      val reducer: IntBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Int = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceEntriesToIntTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsInt(r, transformer.applyAsInt(p))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceEntriesToIntTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceEntriesToIntTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceEntriesToIntTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsInt(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  @SuppressWarnings(Array("serial")) private[concurrent] final class MapReduceMappingsToIntTask[K, V](p: ConcurrentSHMap.BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[ConcurrentSHMap.Node[K, V]], final val nextRight: ConcurrentSHMap.MapReduceMappingsToIntTask[K, V], final val transformer: ToIntBiFunction[_ >: K, _ >: V], final val basis: Int, final val reducer: IntBinaryOperator) extends BulkTask[K, V, Integer](p,b,i,f,t) {
    private[concurrent] var result: Int = 0
    private[concurrent] var rights: ConcurrentSHMap.MapReduceMappingsToIntTask[K, V] = null

    final override def getRawResult: Integer = {
      return result
    }

    final def compute {
      val transformer: ToIntBiFunction[_ >: K, _ >: V] = this.transformer
      val reducer: IntBinaryOperator = this.reducer
      if ((({
        transformer
      })) != null && (({
        reducer
      })) != null) {
        var r: Int = this.basis

        {
          val i: Int = baseIndex
          var f: Int = 0
          var h: Int = 0
          while (batch > 0 && (({
            h = ((({
              f = baseLimit; f
            })) + i) >>> 1; h
          })) > i) {
            addToPendingCount(1)
            (({
              rights = new ConcurrentSHMap.MapReduceMappingsToIntTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, rights, transformer, r, reducer); rights
            })).fork
          }
        }
        {
          var p: ConcurrentSHMap.Node[K, V] = null
          while ((({
            p = advance; p
          })) != null) r = reducer.applyAsInt(r, transformer.applyAsInt(p.key, p.value))
        }
        result = r
        var c: CountedCompleter[_] = null

        {
          c = firstComplete
          while (c != null) {
            {
              @SuppressWarnings(Array("unchecked")) val t: ConcurrentSHMap.MapReduceMappingsToIntTask[K, V] = c.asInstanceOf[ConcurrentSHMap.MapReduceMappingsToIntTask[K, V]]
              @SuppressWarnings(Array("unchecked")) var s: ConcurrentSHMap.MapReduceMappingsToIntTask[K, V] = t.rights
              while (s != null) {
                t.result = reducer.applyAsInt(t.result, s.result)
                s = ({
                  t.rights = s.nextRight; t.rights
                })
              }
            }
            c = c.nextComplete
          }
        }
      }
    }
  }

  private val U: Unsafe = MyThreadLocalRandom.getUnsafe
  val k: Class[_] = classOf[ConcurrentSHMap[_, _]]
  private val SIZECTL: Long = U.objectFieldOffset(k.getDeclaredField("sizeCtl"))
  private val TRANSFERINDEX: Long = U.objectFieldOffset(k.getDeclaredField("transferIndex"))
  private val BASECOUNT: Long = U.objectFieldOffset(k.getDeclaredField("baseCount"))
  private val CELLSBUSY: Long = U.objectFieldOffset(k.getDeclaredField("cellsBusy"))
  val ck: Class[_] = classOf[ConcurrentSHMap.CounterCell]
  private val CELLVALUE: Long = U.objectFieldOffset(ck.getDeclaredField("value"))
  val ak: Class[_] = classOf[Array[ConcurrentSHMap.Node[_, _]]]
  private val ABASE: Long = U.arrayBaseOffset(ak)
  val scale: Int = U.arrayIndexScale(ak)
  if ((scale & (scale - 1)) != 0) throw new Error("data type scale not a power of two")
  private val ASHIFT: Int = 31 - Integer.numberOfLeadingZeros(scale)
}

@SerialVersionUID(7249069246763182397L)
/**
 * Creates a new, empty map with the default initial table size (16).
 */
class ConcurrentSHMap[K, V] extends AbstractMap[K, V] with ConcurrentMap[K, V] with Serializable {
  /**
   * The array of bins. Lazily initialized upon first insertion.
   * Size is always a power of two. Accessed directly by iterators.
   */
  @volatile
  @transient
  private[concurrent] var table: Array[ConcurrentSHMap.Node[K, V]] = null
  /**
   * The next table to use; non-null only while resizing.
   */
  @volatile
  @transient
  private var nextTable: Array[ConcurrentSHMap.Node[K, V]] = null
  /**
   * Base counter value, used mainly when there is no contention,
   * but also as a fallback during table initialization
   * races. Updated via CAS.
   */
  @volatile
  @transient
  private var baseCount: Long = 0L
  /**
   * Table initialization and resizing control.  When negative, the
   * table is being initialized or resized: -1 for initialization,
   * else -(1 + the number of active resizing threads).  Otherwise,
   * when table is null, holds the initial table size to use upon
   * creation, or 0 for default. After initialization, holds the
   * next element count value upon which to resize the table.
   */
  @volatile
  @transient
  private var sizeCtl: Int = 0
  /**
   * The next table index (plus one) to split while resizing.
   */
  @volatile
  @transient
  private var transferIndex: Int = 0
  /**
   * Spinlock (locked via CAS) used when resizing and/or creating CounterCells.
   */
  @volatile
  @transient
  private var cellsBusy: Int = 0
  /**
   * Table of counter cells. When non-null, size is a power of 2.
   */
  @volatile
  @transient
  private var counterCells: Array[ConcurrentSHMap.CounterCell] = null
  @transient
  private var keySetVar: ConcurrentSHMap.KeySetView[K, V] = null
  @transient
  private var valuesVar: ConcurrentSHMap.ValuesView[K, V] = null
  @transient
  private var entrySetVar: ConcurrentSHMap.EntrySetView[K, V] = null

  /**
   * Creates a new, empty map with an initial table size
   * accommodating the specified number of elements without the need
   * to dynamically resize.
   *
   * @param initialCapacity The implementation performs internal
   *                        sizing to accommodate this many elements.
   * @throws IllegalArgumentException if the initial capacity of
   *                                  elements is negative
   */
  def this(initialCapacity: Int) {
    this()
    if (initialCapacity < 0) throw new IllegalArgumentException
    val cap: Int = (if ((initialCapacity >= (ConcurrentSHMap.MAXIMUM_CAPACITY >>> 1))) ConcurrentSHMap.MAXIMUM_CAPACITY else ConcurrentSHMap.tableSizeFor(initialCapacity + (initialCapacity >>> 1) + 1))
    this.sizeCtl = cap
  }

  /**
   * Creates a new map with the same mappings as the given map.
   *
   * @param m the map
   */
  def this(m: Map[_ <: K, _ <: V]) {
    this()
    this.sizeCtl = ConcurrentSHMap.DEFAULT_CAPACITY
    putAll(m)
  }

  /**
   * Creates a new, empty map with an initial table size based on
   * the given number of elements ({@code initialCapacity}), table
   * density ({@code loadFactor}), and number of concurrently
   * updating threads ({@code concurrencyLevel}).
   *
   * @param initialCapacity the initial capacity. The implementation
   *                        performs internal sizing to accommodate this many elements,
   *                        given the specified load factor.
   * @param loadFactor the load factor (table density) for
   *                   establishing the initial table size
   * @param concurrencyLevel the estimated number of concurrently
   *                         updating threads. The implementation may use this value as
   *                         a sizing hint.
   * @throws IllegalArgumentException if the initial capacity is
   *                                  negative or the load factor or concurrencyLevel are
   *                                  nonpositive
   */
  def this(inInitialCapacity: Int, loadFactor: Float, concurrencyLevel: Int = 1) {
    this()
    var initialCapacity: Int = inInitialCapacity
    if (!(loadFactor > 0.0f) || initialCapacity < 0 || concurrencyLevel <= 0) throw new IllegalArgumentException
    if (initialCapacity < concurrencyLevel) initialCapacity = concurrencyLevel
    val size: Long = (1.0 + initialCapacity.toLong / loadFactor).toLong
    val cap: Int = if ((size >= ConcurrentSHMap.MAXIMUM_CAPACITY.toLong)) ConcurrentSHMap.MAXIMUM_CAPACITY else ConcurrentSHMap.tableSizeFor(size.toInt)
    this.sizeCtl = cap
  }

  /**
   * {@inheritDoc}
   */
  override def size: Int = {
    val n: Long = sumCount
    return (if ((n < 0L)) 0 else if ((n > Integer.MAX_VALUE.toLong)) Integer.MAX_VALUE else n.toInt)
  }

  /**
   * {@inheritDoc}
   */
  override def isEmpty: Boolean = {
    return sumCount <= 0L
  }

  /**
   * Returns the value to which the specified key is mapped,
   * or {@code null} if this map contains no mapping for the key.
   *
   * <p>More formally, if this map contains a mapping from a key
   * {@code k} to a value {@code v} such that {@code key.equals(k)},
   * then this method returns {@code v}; otherwise it returns
   * {@code null}.  (There can be at most one such mapping.)
   *
   * @throws NullPointerException if the specified key is null
   */
  override def get(key: Any): V = {
    var tab: Array[ConcurrentSHMap.Node[K, V]] = null
    var e: ConcurrentSHMap.Node[K, V] = null
    var p: ConcurrentSHMap.Node[K, V] = null
    var n: Int = 0
    var eh: Int = 0
    var ek: K = null.asInstanceOf[K]
    val h: Int = ConcurrentSHMap.spread(key.hashCode)
    if ((({
      tab = table; tab
    })) != null && (({
      n = tab.length; n
    })) > 0 && (({
      e = ConcurrentSHMap.tabAt(tab, (n - 1) & h); e
    })) != null) {
      if ((({
        eh = e.hash; eh
      })) == h) {
        if (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek))) return e.value
      }
      else if (eh < 0) return if ((({
        p = e.find(h, key); p
      })) != null) p.value
      else null.asInstanceOf[V]
      while ((({
        e = e.next; e
      })) != null) {
        if (e.hash == h && (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek)))) return e.value
      }
    }
    null.asInstanceOf[V]
  }

  /**
   * Tests if the specified object is a key in this table.
   *
   * @param  key possible key
   * @return { @code true} if and only if the specified object
   *                 is a key in this table, as determined by the
   *                 { @code equals} method; { @code false} otherwise
   * @throws NullPointerException if the specified key is null
   */
  override def containsKey(key: Any): Boolean = {
    return get(key) != null
  }

  /**
   * Returns {@code true} if this map maps one or more keys to the
   * specified value. Note: This method may require a full traversal
   * of the map, and is much slower than method {@code containsKey}.
   *
   * @param value value whose presence in this map is to be tested
   * @return { @code true} if this map maps one or more keys to the
   *                 specified value
   * @throws NullPointerException if the specified value is null
   */
  override def containsValue(value: Any): Boolean = {
    if (value == null) throw new NullPointerException
    val t = table
    if (t != null) {
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

      val p: ConcurrentSHMap.Node[K, V] = it.advance
      while (p != null) {
        val v: V = p.value
        if (refEquals(v, value) || ((v != null) && (value == v))) {
          return true
        }
      }
    }
    false
  }

  /**
   * Maps the specified key to the specified value in this table.
   * Neither the key nor the value can be null.
   *
   * <p>The value can be retrieved by calling the {@code get} method
   * with a key that is equal to the original key.
   *
   * @param key key with which the specified value is to be associated
   * @param value value to be associated with the specified key
   * @return the previous value associated with { @code key}, or
   *                                                    { @code null} if there was no mapping for { @code key}
   * @throws NullPointerException if the specified key or value is null
   */
  override def put(key: K, value: V): V = {
    return putVal(key, value, false)
  }

  /** Implementation for put and putIfAbsent */
  private[concurrent] final def putVal(key: K, value: V, onlyIfAbsent: Boolean): V = {
    if (key == null || value == null) throw new NullPointerException
    val hash: Int = ConcurrentSHMap.spread(key.hashCode)
    var binCount: Int = 0

    var tab: Array[ConcurrentSHMap.Node[K, V]] = table
    var break = false
    while (!break) {
      var f: ConcurrentSHMap.Node[K, V] = null
      var n: Int = 0
      var i: Int = 0
      var fh: Int = 0
      if ((tab == null) || ({n = tab.length; n } == 0)) {
        tab = initTable
      } else if ({f = ConcurrentSHMap.tabAt(tab, {i = ((n - 1) & hash); i}); f} == null) {
        if (ConcurrentSHMap.casTabAt(tab, i, null, new ConcurrentSHMap.Node[K, V](hash, key, value, null))) {
          break = true //todo: break is not supported
        }
      } else if ({fh = f.hash; fh} == ConcurrentSHMap.MOVED) {
        tab = helpTransfer(tab, f)
      } else {
        var oldVal: V = null.asInstanceOf[V]
        f synchronized {
          if (ConcurrentSHMap.tabAt(tab, i) eq f) {
            if (fh >= 0) {
              binCount = 1

              var e: ConcurrentSHMap.Node[K, V] = f
              break = false
              while (!break) {
                var ek: K = null.asInstanceOf[K]
                if (e.hash == hash && (refEquals({ek = e.key; ek}, key) || ((ek != null) && (key == ek)))) {
                  oldVal = e.value
                  if (!onlyIfAbsent) {
                    e.value = value
                  }
                  break = true //todo: break is not supported
                }
                if(!break) {
                  val pred: ConcurrentSHMap.Node[K, V] = e
                  if ({e = e.next; e} == null) {
                    pred.next = new ConcurrentSHMap.Node[K, V](hash, key, value, null)
                    break = true //todo: break is not supported
                  }
                  binCount += 1
                }
              }
              break = false
            } else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
              var p: ConcurrentSHMap.Node[K, V] = null
              binCount = 2
              if ({p = (f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]).putTreeVal(hash, key, value); p} != null) {
                oldVal = p.value
                if (!onlyIfAbsent) {
                  p.value = value
                }
              }
            }
          }
        }
        if (binCount != 0) {
          if (binCount >= ConcurrentSHMap.TREEIFY_THRESHOLD) {
            treeifyBin(tab, i)
          }
          if (oldVal != null) {
            return oldVal
          }
          break = true //todo: break is not supported
        }
      }
    }
    
    addCount(1L, binCount)
    return null.asInstanceOf[V]
  }

  /**
   * Copies all of the mappings from the specified map to this one.
   * These mappings replace any mappings that this map had for any of the
   * keys currently in the specified map.
   *
   * @param m mappings to be stored in this map
   */
  override def putAll(m: Map[_ <: K, _ <: V]) {
    tryPresize(m.size)
    import scala.collection.JavaConversions._
    for (e <- m.entrySet) putVal(e.getKey, e.getValue, false)
  }

  /**
   * Removes the key (and its corresponding value) from this map.
   * This method does nothing if the key is not in the map.
   *
   * @param  key the key that needs to be removed
   * @return the previous value associated with { @code key}, or
   *                                                    { @code null} if there was no mapping for { @code key}
   * @throws NullPointerException if the specified key is null
   */
  override def remove(key: Any): V = {
    return replaceNode(key, null.asInstanceOf[V], null)
  }

  /**
   * Implementation for the four public remove/replace methods:
   * Replaces node value with v, conditional upon match of cv if
   * non-null.  If resulting value is null, delete.
   */
  private[concurrent] final def replaceNode(key: Any, value: V, cv: Any): V = {
    val hash: Int = ConcurrentSHMap.spread(key.hashCode)

    {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var break = false
      while (!break) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var n: Int = 0
        var i: Int = 0
        var fh: Int = 0
        if (tab == null || (({
          n = tab.length; n
        })) == 0 || (({
          f = ConcurrentSHMap.tabAt(tab, {i = (n - 1) & hash; i}); f
        })) == null) break = true //todo: break is not supported
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) tab = helpTransfer(tab, f)
        else {
          var oldVal: V = null.asInstanceOf[V]
          var validated: Boolean = false
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              if (fh >= 0) {
                validated = true

                {
                  var e: ConcurrentSHMap.Node[K, V] = f
                  var pred: ConcurrentSHMap.Node[K, V] = null
                  break = false;
                  while (!break) {
                    var ek: K = null.asInstanceOf[K]
                    if (e.hash == hash && (refEquals((({
                      ek = e.key; ek
                    })), key) || (ek != null && (key == ek)))) {
                      val ev: V = e.value
                      if ((cv == null) || refEquals(cv, ev) || (ev != null && (cv == ev))) {
                        oldVal = ev
                        if (value != null) e.value = value
                        else if (pred != null) pred.next = e.next
                        else ConcurrentSHMap.setTabAt(tab, i, e.next)
                      }
                      break = true //todo: break is not supported
                    }
                    if(!break) {
                      pred = e
                      if ((({
                        e = e.next;
                        e
                      })) == null) break = true //todo: break is not supported
                    }
                  }
                  break = false
                }
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                validated = true
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                var r: ConcurrentSHMap.TreeNode[K, V] = null
                var p: ConcurrentSHMap.TreeNode[K, V] = null
                if ((({
                  r = t.root; r
                })) != null && (({
                  p = r.findTreeNode(hash, key, null); p
                })) != null) {
                  val pv: V = p.value
                  if ((cv == null) || refEquals(cv, pv) || ((pv != null) && (cv == pv))) {
                    oldVal = pv
                    if (value != null) p.value = value
                    else if (t.removeTreeNode(p)) ConcurrentSHMap.setTabAt(tab, i, ConcurrentSHMap.untreeify(t.first))
                  }
                }
              }
            }
          }
          if (validated) {
            if (oldVal != null) {
              if (value == null) addCount(-1L, -1)
              return oldVal
            }
            break = true //todo: break is not supported
          }
        }
      }
    }
    return null.asInstanceOf[V]
  }

  /**
   * Removes all of the mappings from this map.
   */
  override def clear {
    var delta: Long = 0L
    var i: Int = 0
    var tab: Array[ConcurrentSHMap.Node[K, V]] = table
    while (tab != null && i < tab.length) {
      var fh: Int = 0
      val f: ConcurrentSHMap.Node[K, V] = ConcurrentSHMap.tabAt(tab, i)
      if (f == null) ({
        i += 1; i
      })
      else if ((({
        fh = f.hash; fh
      })) == ConcurrentSHMap.MOVED) {
        tab = helpTransfer(tab, f)
        i = 0
      }
      else {
        f synchronized {
          if (ConcurrentSHMap.tabAt(tab, i) eq f) {
            var p: ConcurrentSHMap.Node[K, V] = (if (fh >= 0) f else if ((f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]])) (f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]).first else null)
            while (p != null) {
              delta -= 1
              p = p.next
            }
            ConcurrentSHMap.setTabAt(tab, ({
              i += 1; i - 1
            }), null)
          }
        }
      }
    }
    if (delta != 0L) addCount(delta, -1)
  }

  /**
   * Returns a {@link Set} view of the keys contained in this map.
   * The set is backed by the map, so changes to the map are
   * reflected in the set, and vice-versa. The set supports element
   * removal, which removes the corresponding mapping from this map,
   * via the {@code Iterator.remove}, {@code Set.remove},
   * {@code removeAll}, {@code retainAll}, and {@code clear}
   * operations.  It does not support the {@code add} or
   * {@code addAll} operations.
   *
   * <p>The view's iterators and spliterators are
   * <a href="package-summary.html#Weakly"><i>weakly consistent</i></a>.
   *
   * <p>The view's {@code spliterator} reports {@link Spliterator#CONCURRENT},
   * {@link Spliterator#DISTINCT}, and {@link Spliterator#NONNULL}.
   *
   * @return the set view
   */
  override def keySet: ConcurrentSHMap.KeySetView[K, V] = {
    var ks: ConcurrentSHMap.KeySetView[K, V] = null
    return if ((({
      ks = keySetVar; ks
    })) != null) ks
    else (({
      keySetVar = new ConcurrentSHMap.KeySetView[K, V](this, null.asInstanceOf[V]); keySetVar
    }))
  }

  /**
   * Returns a {@link Collection} view of the values contained in this map.
   * The collection is backed by the map, so changes to the map are
   * reflected in the collection, and vice-versa.  The collection
   * supports element removal, which removes the corresponding
   * mapping from this map, via the {@code Iterator.remove},
   * {@code Collection.remove}, {@code removeAll},
   * {@code retainAll}, and {@code clear} operations.  It does not
   * support the {@code add} or {@code addAll} operations.
   *
   * <p>The view's iterators and spliterators are
   * <a href="package-summary.html#Weakly"><i>weakly consistent</i></a>.
   *
   * <p>The view's {@code spliterator} reports {@link Spliterator#CONCURRENT}
   * and {@link Spliterator#NONNULL}.
   *
   * @return the collection view
   */
  override def values: Collection[V] = {
    var vs: ConcurrentSHMap.ValuesView[K, V] = null
    return if ((({
      vs = valuesVar; vs
    })) != null) vs
    else (({
      valuesVar = new ConcurrentSHMap.ValuesView[K, V](this); valuesVar
    }))
  }

  /**
   * Returns a {@link Set} view of the mappings contained in this map.
   * The set is backed by the map, so changes to the map are
   * reflected in the set, and vice-versa.  The set supports element
   * removal, which removes the corresponding mapping from the map,
   * via the {@code Iterator.remove}, {@code Set.remove},
   * {@code removeAll}, {@code retainAll}, and {@code clear}
   * operations.
   *
   * <p>The view's iterators and spliterators are
   * <a href="package-summary.html#Weakly"><i>weakly consistent</i></a>.
   *
   * <p>The view's {@code spliterator} reports {@link Spliterator#CONCURRENT},
   * {@link Spliterator#DISTINCT}, and {@link Spliterator#NONNULL}.
   *
   * @return the set view
   */
  def entrySet: Set[Map.Entry[K, V]] = {
    var es: ConcurrentSHMap.EntrySetView[K, V] = null
    return if ((({
      es = entrySetVar; es
    })) != null) es
    else (({
      entrySetVar = new ConcurrentSHMap.EntrySetView[K, V](this); entrySetVar
    }))
  }

  /**
   * Returns the hash code value for this {@link Map}, i.e.,
   * the sum of, for each key-value pair in the map,
   * {@code key.hashCode() ^ value.hashCode()}.
   *
   * @return the hash code value for this map
   */
  override def hashCode: Int = {
    var h: Int = 0
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    if ((({
      t = table; t
    })) != null) {
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = it.advance; p
        })) != null) h += p.key.hashCode ^ p.value.hashCode
      }
    }
    return h
  }

  /**
   * Returns a string representation of this map.  The string
   * representation consists of a list of key-value mappings (in no
   * particular order) enclosed in braces ("{@code {}}").  Adjacent
   * mappings are separated by the characters {@code ", "} (comma
   * and space).  Each key-value mapping is rendered as the key
   * followed by an equals sign ("{@code =}") followed by the
   * associated value.
   *
   * @return a string representation of this map
   */
  override def toString: String = {
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    val f: Int = if ((({
      t = table; t
    })) == null) 0
    else t.length
    val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, f, 0, f)
    val sb: StringBuilder = new StringBuilder
    sb.append('{')
    var p: ConcurrentSHMap.Node[K, V] = null
    if ((({
      p = it.advance; p
    })) != null) {
      var break = false
      while (!break) {
        val k: K = p.key
        val v: V = p.value
        sb.append(if (refEquals(k, this)) "(this Map)" else k)
        sb.append('=')
        sb.append(if (refEquals(v, this)) "(this Map)" else v)
        if ((({
          p = it.advance; p
        })) == null) break = true //todo: break is not supported
        if(!break) sb.append(',').append(' ')
      }
    }
    return sb.append('}').toString
  }

  /**
   * Compares the specified object with this map for equality.
   * Returns {@code true} if the given object is a map with the same
   * mappings as this map.  This operation may return misleading
   * results if either map is concurrently modified during execution
   * of this method.
   *
   * @param o object to be compared for equality with this map
   * @return { @code true} if the specified object is equal to this map
   */
  override def equals(o: Any): Boolean = {
    if (!refEquals(o, this)) {
      if (!(o.isInstanceOf[Map[_, _]])) return false
      val m = o.asInstanceOf[Map[K,V]]
      var t: Array[ConcurrentSHMap.Node[K, V]] = null
      val f: Int = if ((({
        t = table; t
      })) == null) 0
      else t.length
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, f, 0, f)

      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = it.advance; p
        })) != null) {
          val value: V = p.value
          val v = m.get(p.key)
          if ((v == null) || (refEquals(v, value) && !(v == value))) return false
        }
      }
      import scala.collection.JavaConversions._
      for (e <- m.entrySet) {
        var mk: K = null.asInstanceOf[K]
        var mv: V = null.asInstanceOf[V]
        var v: V = null.asInstanceOf[V]
        if ((({
          mk = e.getKey; mk
        })) == null || (({
          mv = e.getValue; mv
        })) == null || (({
          v = get(mk); v
        })) == null || (!refEquals(mv, v) && !(mv == v))) return false
      }
    }
    true
  }

  /**
   * Saves the state of the {@code ConcurrentSHMap} instance to a
   * stream (i.e., serializes it).
   * @param s the stream
   * @throws java.io.IOException if an I/O error occurs
   * @serialData
     * the key (Object) and value (Object)
   *   for each key-value mapping, followed by a null pair.
   *   The key-value mappings are emitted in no particular order.
   */
  @throws(classOf[java.io.IOException])
  private def writeObject(s: ObjectOutputStream) {
    var sshift: Int = 0
    var ssize: Int = 1
    while (ssize < ConcurrentSHMap.DEFAULT_CONCURRENCY_LEVEL) {
      sshift += 1
      ssize <<= 1
    }
    val segmentShift: Int = 32 - sshift
    val segmentMask: Int = ssize - 1
    @SuppressWarnings(Array("unchecked")) var segments: Array[ConcurrentSHMap.Segment[K, V]] = new Array[ConcurrentSHMap.Segment[_, _]](ConcurrentSHMap.DEFAULT_CONCURRENCY_LEVEL).asInstanceOf[Array[ConcurrentSHMap.Segment[K, V]]]

    {
      var i: Int = 0
      while (i < segments.length) {
        segments(i) = new ConcurrentSHMap.Segment[K, V](ConcurrentSHMap.LOAD_FACTOR)
        ({
          i += 1; i
        })
      }
    }
    s.putFields.put("segments", segments)
    s.putFields.put("segmentShift", segmentShift)
    s.putFields.put("segmentMask", segmentMask)
    s.writeFields
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    if ((({
      t = table; t
    })) != null) {
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = it.advance; p
        })) != null) {
          s.writeObject(p.key)
          s.writeObject(p.value)
        }
      }
    }
    s.writeObject(null)
    s.writeObject(null)
    segments = null
  }

  /**
   * Reconstitutes the instance from a stream (that is, deserializes it).
   * @param s the stream
   * @throws ClassNotFoundException if the class of a serialized object
   *                                could not be found
   * @throws java.io.IOException if an I/O error occurs
   */
  @throws(classOf[java.io.IOException])
  @throws(classOf[ClassNotFoundException])
  private def readObject(s: ObjectInputStream) {
    sizeCtl = -1
    s.defaultReadObject
    var size: Long = 0L
    var p: ConcurrentSHMap.Node[K, V] = null
    var break = false
    while (!break) {
      @SuppressWarnings(Array("unchecked")) val k: K = s.readObject.asInstanceOf[K]
      @SuppressWarnings(Array("unchecked")) val v: V = s.readObject.asInstanceOf[V]
      if (k != null && v != null) {
        p = new ConcurrentSHMap.Node[K, V](ConcurrentSHMap.spread(k.hashCode), k, v, p)
        size += 1
      }
      else break = true //todo: break is not supported
    }
    if (size == 0L) sizeCtl = 0
    else {
      var n: Int = 0
      if (size >= (ConcurrentSHMap.MAXIMUM_CAPACITY >>> 1).toLong) n = ConcurrentSHMap.MAXIMUM_CAPACITY
      else {
        val sz: Int = size.toInt
        n = ConcurrentSHMap.tableSizeFor(sz + (sz >>> 1) + 1)
      }
      @SuppressWarnings(Array("unchecked")) val tab: Array[ConcurrentSHMap.Node[K, V]] = new Array[ConcurrentSHMap.Node[_, _]](n).asInstanceOf[Array[ConcurrentSHMap.Node[K, V]]]
      val mask: Int = n - 1
      var added: Long = 0L
      while (p != null) {
        var insertAtFront: Boolean = false
        val next: ConcurrentSHMap.Node[K, V] = p.next
        var first: ConcurrentSHMap.Node[K, V] = null
        val h: Int = p.hash
        val j: Int = h & mask
        if ((({
          first = ConcurrentSHMap.tabAt(tab, j); first
        })) == null) insertAtFront = true
        else {
          val k: K = p.key
          if (first.hash < 0) {
            val t: ConcurrentSHMap.TreeBin[K, V] = first.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
            if (t.putTreeVal(h, k, p.value) == null) ({
              added += 1; added
            })
            insertAtFront = false
          }
          else {
            var binCount: Int = 0
            insertAtFront = true
            var q: ConcurrentSHMap.Node[K, V] = null
            var qk: K = null.asInstanceOf[K]

            {
              q = first
              var break = false
              while (!break && (q != null)) {
                if (q.hash == h && (refEquals((({
                  qk = q.key; qk
                })), k) || (qk != null && (k == qk)))) {
                  insertAtFront = false
                  break = true //todo: break is not supported
                }
                if(!break) {
                  binCount += 1
                  q = q.next
                }
              }
            }
            if (insertAtFront && binCount >= ConcurrentSHMap.TREEIFY_THRESHOLD) {
              insertAtFront = false
              added += 1
              p.next = first
              var hd: ConcurrentSHMap.TreeNode[K, V] = null
              var tl: ConcurrentSHMap.TreeNode[K, V] = null

              {
                q = p
                while (q != null) {
                  {
                    val t: ConcurrentSHMap.TreeNode[K, V] = new ConcurrentSHMap.TreeNode[K, V](q.hash, q.key, q.value, null, null)
                    if ((({
                      t.prev = tl; t.prev
                    })) == null) hd = t
                    else tl.next = t
                    tl = t
                  }
                  q = q.next
                }
              }
              ConcurrentSHMap.setTabAt(tab, j, new ConcurrentSHMap.TreeBin[K, V](hd))
            }
          }
        }
        if (insertAtFront) {
          added += 1
          p.next = first
          ConcurrentSHMap.setTabAt(tab, j, p)
        }
        p = next
      }
      table = tab
      sizeCtl = n - (n >>> 2)
      baseCount = added
    }
  }

  /**
   * {@inheritDoc}
   *
   * @return the previous value associated with the specified key,
   *         or { @code null} if there was no mapping for the key
   * @throws NullPointerException if the specified key or value is null
   */
  override def putIfAbsent(key: K, value: V): V = {
    return putVal(key, value, true)
  }

  /**
   * {@inheritDoc}
   *
   * @throws NullPointerException if the specified key is null
   */
  override def remove(key: Any, value: Any): Boolean = {
    if (key == null) throw new NullPointerException
    return value != null && replaceNode(key, null.asInstanceOf[V], value) != null
  }

  /**
   * {@inheritDoc}
   *
   * @throws NullPointerException if any of the arguments are null
   */
  override def replace(key: K, oldValue: V, newValue: V): Boolean = {
    if (key == null || oldValue == null || newValue == null) throw new NullPointerException
    return replaceNode(key, newValue, oldValue) != null
  }

  /**
   * {@inheritDoc}
   *
   * @return the previous value associated with the specified key,
   *         or { @code null} if there was no mapping for the key
   * @throws NullPointerException if the specified key or value is null
   */
  override def replace(key: K, value: V): V = {
    if (key == null || value == null) throw new NullPointerException
    return replaceNode(key, value, null)
  }

  /**
   * Returns the value to which the specified key is mapped, or the
   * given default value if this map contains no mapping for the
   * key.
   *
   * @param key the key whose associated value is to be returned
   * @param defaultValue the value to return if this map contains
   *                     no mapping for the given key
   * @return the mapping for the key, if present; else the default value
   * @throws NullPointerException if the specified key is null
   */
  override def getOrDefault(key: Any, defaultValue: V): V = {
    var v: V = null.asInstanceOf[V]
    return if ((({
      v = get(key); v
    })) == null) defaultValue
    else v
  }

  override def forEach(action: BiConsumer[_ >: K, _ >: V]) {
    if (action == null) throw new NullPointerException
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    if ((({
      t = table; t
    })) != null) {
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = it.advance; p
        })) != null) {
          action.accept(p.key, p.value)
        }
      }
    }
  }

  override def replaceAll(function: BiFunction[_ >: K, _ >: V, _ <: V]) {
    if (function == null) throw new NullPointerException
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    if ((({
      t = table; t
    })) != null) {
      val it: ConcurrentSHMap.Traverser[K, V] = new ConcurrentSHMap.Traverser[K, V](t, t.length, 0, t.length)

      {
        var p: ConcurrentSHMap.Node[K, V] = null
        while ((({
          p = it.advance; p
        })) != null) {
          var oldValue: V = p.value

          {
            val key: K = p.key
            var break = false
            while (!break) {
              val newValue: V = function.apply(key, oldValue)
              if (newValue == null) throw new NullPointerException
              if (replaceNode(key, newValue, oldValue) != null || (({
                oldValue = get(key); oldValue
              })) == null) break = true //todo: break is not supported
            }
          }
        }
      }
    }
  }

  /**
   * If the specified key is not already associated with a value,
   * attempts to compute its value using the given mapping function
   * and enters it into this map unless {@code null}.  The entire
   * method invocation is performed atomically, so the function is
   * applied at most once per key.  Some attempted update operations
   * on this map by other threads may be blocked while computation
   * is in progress, so the computation should be short and simple,
   * and must not attempt to update any other mappings of this map.
   *
   * @param key key with which the specified value is to be associated
   * @param mappingFunction the function to compute a value
   * @return the current (existing or computed) value associated with
   *         the specified key, or null if the computed value is null
   * @throws NullPointerException if the specified key or mappingFunction
   *                              is null
   * @throws IllegalStateException if the computation detectably
   *                               attempts a recursive update to this map that would
   *                               otherwise never complete
   * @throws RuntimeException or Error if the mappingFunction does so,
   *                          in which case the mapping is left unestablished
   */
  override def computeIfAbsent(key: K, mappingFunction: Function[_ >: K, _ <: V]): V = {
    if (key == null || mappingFunction == null) throw new NullPointerException
    val h: Int = ConcurrentSHMap.spread(key.hashCode)
    var value: V = null.asInstanceOf[V]
    var binCount: Int = 0

    {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var break = false
      while (!break) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var n: Int = 0
        var i: Int = 0
        var fh: Int = 0
        if (tab == null || (({
          n = tab.length; n
        })) == 0) tab = initTable
        else if ((({
          f = ConcurrentSHMap.tabAt(tab, {i = (n - 1) & h; i}); f
        })) == null) {
          val r: ConcurrentSHMap.Node[K, V] = new ConcurrentSHMap.ReservationNode[K, V]
          r synchronized {
            if (ConcurrentSHMap.casTabAt(tab, i, null, r)) {
              binCount = 1
              var node: ConcurrentSHMap.Node[K, V] = null
              try {
                if ((({
                  value = mappingFunction.apply(key); value
                })) != null) node = new ConcurrentSHMap.Node[K, V](h, key, value, null)
              } finally {
                ConcurrentSHMap.setTabAt(tab, i, node)
              }
            }
          }
          if (binCount != 0) break = true //todo: break is not supported
        }
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) tab = helpTransfer(tab, f)
        else {
          var added: Boolean = false
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              if (fh >= 0) {
                binCount = 1

                {
                  var e: ConcurrentSHMap.Node[K, V] = f
                  break = false
                  while (!break) {
                    {
                      var ek: K = null.asInstanceOf[K]
                      val ev: V = null.asInstanceOf[V]
                      if (e.hash == h && (refEquals((({
                        ek = e.key; ek
                      })), key) || ((ek != null) && (key == ek)))) {
                        value = e.value
                        break = true //todo: break is not supported
                      }
                      if(!break) {
                        val pred: ConcurrentSHMap.Node[K, V] = e
                        if ((({
                          e = e.next;
                          e
                        })) == null) {
                          if ((({
                            value = mappingFunction.apply(key);
                            value
                          })) != null) {
                            added = true
                            pred.next = new ConcurrentSHMap.Node[K, V](h, key, value, null)
                          }
                          break = true //todo: break is not supported
                        }
                      }
                    }
                    if(!break) binCount += 1
                  }
                  break = false
                }
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                binCount = 2
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                var r: ConcurrentSHMap.TreeNode[K, V] = null
                var p: ConcurrentSHMap.TreeNode[K, V] = null
                if ((({
                  r = t.root; r
                })) != null && (({
                  p = r.findTreeNode(h, key, null); p
                })) != null) value = p.value
                else if ((({
                  value = mappingFunction.apply(key); value
                })) != null) {
                  added = true
                  t.putTreeVal(h, key, value)
                }
              }
            }
          }
          if (binCount != 0) {
            if (binCount >= ConcurrentSHMap.TREEIFY_THRESHOLD) treeifyBin(tab, i)
            if (!added) return value
            break = true //todo: break is not supported
          }
        }
      }
    }
    if (value != null) addCount(1L, binCount)
    return value
  }

  /**
   * If the value for the specified key is present, attempts to
   * compute a new mapping given the key and its current mapped
   * value.  The entire method invocation is performed atomically.
   * Some attempted update operations on this map by other threads
   * may be blocked while computation is in progress, so the
   * computation should be short and simple, and must not attempt to
   * update any other mappings of this map.
   *
   * @param key key with which a value may be associated
   * @param remappingFunction the function to compute a value
   * @return the new value associated with the specified key, or null if none
   * @throws NullPointerException if the specified key or remappingFunction
   *                              is null
   * @throws IllegalStateException if the computation detectably
   *                               attempts a recursive update to this map that would
   *                               otherwise never complete
   * @throws RuntimeException or Error if the remappingFunction does so,
   *                          in which case the mapping is unchanged
   */
  override def computeIfPresent(key: K, remappingFunction: BiFunction[_ >: K, _ >: V, _ <: V]): V = {
    if (key == null || remappingFunction == null) throw new NullPointerException
    val h: Int = ConcurrentSHMap.spread(key.hashCode)
    var value: V = null.asInstanceOf[V]
    var delta: Int = 0
    var binCount: Int = 0

    {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var break = false
      while (!break) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var n: Int = 0
        var i: Int = 0
        var fh: Int = 0
        if (tab == null || (({
          n = tab.length; n
        })) == 0) tab = initTable
        else if ((({
          f = ConcurrentSHMap.tabAt(tab, {i = (n - 1) & h; i}); f
        })) == null) break = true //todo: break is not supported
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) tab = helpTransfer(tab, f)
        else {
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              if (fh >= 0) {
                binCount = 1

                {
                  var e: ConcurrentSHMap.Node[K, V] = f
                  var pred: ConcurrentSHMap.Node[K, V] = null
                  break = false
                  while (!break) {
                    {
                      var ek: K = null.asInstanceOf[K]
                      if (e.hash == h && (refEquals((({
                        ek = e.key; ek
                      })), key) || ((ek != null) && (key == ek)))) {
                        value = remappingFunction.apply(key, e.value)
                        if (value != null) e.value = value
                        else {
                          delta = -1
                          val en: ConcurrentSHMap.Node[K, V] = e.next
                          if (pred != null) pred.next = en
                          else ConcurrentSHMap.setTabAt(tab, i, en)
                        }
                        break = true //todo: break is not supported
                      }
                      if(!break) {
                        pred = e
                        if ((({
                          e = e.next;
                          e
                        })) == null) break = true //todo: break is not supported
                      }
                    }
                    if(!break) binCount += 1
                  }
                  break = false
                }
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                binCount = 2
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                var r: ConcurrentSHMap.TreeNode[K, V] = null
                var p: ConcurrentSHMap.TreeNode[K, V] = null
                if ((({
                  r = t.root; r
                })) != null && (({
                  p = r.findTreeNode(h, key, null); p
                })) != null) {
                  value = remappingFunction.apply(key, p.value)
                  if (value != null) p.value = value
                  else {
                    delta = -1
                    if (t.removeTreeNode(p)) ConcurrentSHMap.setTabAt(tab, i, ConcurrentSHMap.untreeify(t.first))
                  }
                }
              }
            }
          }
          if (binCount != 0) break = true //todo: break is not supported
        }
      }
    }
    if (delta != 0) addCount(delta.toLong, binCount)
    return value
  }

  /**
   * Attempts to compute a mapping for the specified key and its
   * current mapped value (or {@code null} if there is no current
   * mapping). The entire method invocation is performed atomically.
   * Some attempted update operations on this map by other threads
   * may be blocked while computation is in progress, so the
   * computation should be short and simple, and must not attempt to
   * update any other mappings of this Map.
   *
   * @param key key with which the specified value is to be associated
   * @param remappingFunction the function to compute a value
   * @return the new value associated with the specified key, or null if none
   * @throws NullPointerException if the specified key or remappingFunction
   *                              is null
   * @throws IllegalStateException if the computation detectably
   *                               attempts a recursive update to this map that would
   *                               otherwise never complete
   * @throws RuntimeException or Error if the remappingFunction does so,
   *                          in which case the mapping is unchanged
   */
  override def compute(key: K, remappingFunction: BiFunction[_ >: K, _ >: V, _ <: V]): V = {
    if (key == null || remappingFunction == null) throw new NullPointerException
    val h: Int = ConcurrentSHMap.spread(key.hashCode)
    var value: V = null.asInstanceOf[V]
    var delta: Int = 0
    var binCount: Int = 0

    {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var break = false
      while (!break) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var n: Int = 0
        var i: Int = 0
        var fh: Int = 0
        if (tab == null || (({
          n = tab.length; n
        })) == 0) tab = initTable
        else if ((({
          f = ConcurrentSHMap.tabAt(tab, {i = (n - 1) & h; i}); f
        })) == null) {
          val r: ConcurrentSHMap.Node[K, V] = new ConcurrentSHMap.ReservationNode[K, V]
          r synchronized {
            if (ConcurrentSHMap.casTabAt(tab, i, null, r)) {
              binCount = 1
              var node: ConcurrentSHMap.Node[K, V] = null
              try {
                if ((({
                  value = remappingFunction.apply(key, null.asInstanceOf[V]); value
                })) != null) {
                  delta = 1
                  node = new ConcurrentSHMap.Node[K, V](h, key, value, null)
                }
              } finally {
                ConcurrentSHMap.setTabAt(tab, i, node)
              }
            }
          }
          if (binCount != 0) break = true //todo: break is not supported
        }
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) tab = helpTransfer(tab, f)
        else {
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              if (fh >= 0) {
                binCount = 1

                {
                  var e: ConcurrentSHMap.Node[K, V] = f
                  var pred: ConcurrentSHMap.Node[K, V] = null
                  break = false
                  while (!break) {
                    {
                      var ek: K = null.asInstanceOf[K]
                      if (e.hash == h && (refEquals((({
                        ek = e.key; ek
                      })), key) || (ek != null && (key == ek)))) {
                        value = remappingFunction.apply(key, e.value)
                        if (value != null) e.value = value
                        else {
                          delta = -1
                          val en: ConcurrentSHMap.Node[K, V] = e.next
                          if (pred != null) pred.next = en
                          else ConcurrentSHMap.setTabAt(tab, i, en)
                        }
                        break = true //todo: break is not supported
                      }
                      if(!break) {
                        pred = e
                        if ((({
                          e = e.next;
                          e
                        })) == null) {
                          value = remappingFunction.apply(key, null.asInstanceOf[V])
                          if (value != null) {
                            delta = 1
                            pred.next = new ConcurrentSHMap.Node[K, V](h, key, value, null)
                          }
                          break = true //todo: break is not supported
                        }
                      }
                    }
                    if(!break) binCount += 1
                  }
                  break = false
                }
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                binCount = 1
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                var r: ConcurrentSHMap.TreeNode[K, V] = null
                var p: ConcurrentSHMap.TreeNode[K, V] = null
                if ((({
                  r = t.root; r
                })) != null) p = r.findTreeNode(h, key, null)
                else p = null
                val pv: V = if ((p == null)) null.asInstanceOf[V] else p.value
                value = remappingFunction.apply(key, pv)
                if (value != null) {
                  if (p != null) p.value = value
                  else {
                    delta = 1
                    t.putTreeVal(h, key, value)
                  }
                }
                else if (p != null) {
                  delta = -1
                  if (t.removeTreeNode(p)) ConcurrentSHMap.setTabAt(tab, i, ConcurrentSHMap.untreeify(t.first))
                }
              }
            }
          }
          if (binCount != 0) {
            if (binCount >= ConcurrentSHMap.TREEIFY_THRESHOLD) treeifyBin(tab, i)
            break = true //todo: break is not supported
          }
        }
      }
    }
    if (delta != 0) addCount(delta.toLong, binCount)
    return value
  }

  /**
   * If the specified key is not already associated with a
   * (non-null) value, associates it with the given value.
   * Otherwise, replaces the value with the results of the given
   * remapping function, or removes if {@code null}. The entire
   * method invocation is performed atomically.  Some attempted
   * update operations on this map by other threads may be blocked
   * while computation is in progress, so the computation should be
   * short and simple, and must not attempt to update any other
   * mappings of this Map.
   *
   * @param key key with which the specified value is to be associated
   * @param value the value to use if absent
   * @param remappingFunction the function to recompute a value if present
   * @return the new value associated with the specified key, or null if none
   * @throws NullPointerException if the specified key or the
   *                              remappingFunction is null
   * @throws RuntimeException or Error if the remappingFunction does so,
   *                          in which case the mapping is unchanged
   */
  override def merge(key: K, value: V, remappingFunction: BiFunction[_ >: V, _ >: V, _ <: V]): V = {
    if (key == null || value == null || remappingFunction == null) throw new NullPointerException
    val h: Int = ConcurrentSHMap.spread(key.hashCode)
    var theVal: V = null.asInstanceOf[V]
    var delta: Int = 0
    var binCount: Int = 0

    {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var break = false
      while (!break) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var n: Int = 0
        var i: Int = 0
        var fh: Int = 0
        if (tab == null || (({
          n = tab.length; n
        })) == 0) tab = initTable
        else if ((({
          f = ConcurrentSHMap.tabAt(tab, {i = (n - 1) & h; i}); f
        })) == null) {
          if (ConcurrentSHMap.casTabAt(tab, i, null, new ConcurrentSHMap.Node[K, V](h, key, value, null))) {
            delta = 1
            theVal = value
            break = true //todo: break is not supported
          }
        }
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) tab = helpTransfer(tab, f)
        else {
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              if (fh >= 0) {
                binCount = 1

                {
                  var e: ConcurrentSHMap.Node[K, V] = f
                  var pred: ConcurrentSHMap.Node[K, V] = null
                  break = false
                  while (!break) {
                    {
                      var ek: K = null.asInstanceOf[K]
                      if (e.hash == h && (refEquals((({
                        ek = e.key; ek
                      })), key) || ((ek != null) && (key == ek)))) {
                        theVal = remappingFunction.apply(e.value, value)
                        if (theVal != null) e.value = theVal
                        else {
                          delta = -1
                          val en: ConcurrentSHMap.Node[K, V] = e.next
                          if (pred != null) pred.next = en
                          else ConcurrentSHMap.setTabAt(tab, i, en)
                        }
                        break = true //todo: break is not supported
                      }
                      if(!break) {
                        pred = e
                        if ((({
                          e = e.next;
                          e
                        })) == null) {
                          delta = 1
                          theVal = value
                          pred.next = new ConcurrentSHMap.Node[K, V](h, key, theVal, null)
                          break = true //todo: break is not supported
                        }
                      }
                    }
                    if(!break) binCount += 1
                  }
                  break = false
                }
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                binCount = 2
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                val r: ConcurrentSHMap.TreeNode[K, V] = t.root
                val p: ConcurrentSHMap.TreeNode[K, V] = if ((r == null)) null else r.findTreeNode(h, key, null)
                theVal = if ((p == null)) value else remappingFunction.apply(p.value, value)
                if (theVal != null) {
                  if (p != null) p.value = theVal
                  else {
                    delta = 1
                    t.putTreeVal(h, key, theVal)
                  }
                }
                else if (p != null) {
                  delta = -1
                  if (t.removeTreeNode(p)) ConcurrentSHMap.setTabAt(tab, i, ConcurrentSHMap.untreeify(t.first))
                }
              }
            }
          }
          if (binCount != 0) {
            if (binCount >= ConcurrentSHMap.TREEIFY_THRESHOLD) treeifyBin(tab, i)
            break = true //todo: break is not supported
          }
        }
      }
    }
    if (delta != 0) addCount(delta.toLong, binCount)
    theVal
  }

  /**
   * Legacy method testing if some key maps into the specified value
   * in this table.  This method is identical in functionality to
   * {@link #containsValue(Object)}, and exists solely to ensure
   * full compatibility with class {@link java.util.Hashtable},
   * which supported this method prior to introduction of the
   * Java Collections framework.
   *
   * @param  value a value to search for
   * @return { @code true} if and only if some key maps to the
   *                 { @code value} argument in this table as
   *                         determined by the { @code equals} method;
   *                                                   { @code false} otherwise
   * @throws NullPointerException if the specified value is null
   */
  def contains(value: Any): Boolean = {
    containsValue(value)
  }

  /**
   * Returns an enumeration of the keys in this table.
   *
   * @return an enumeration of the keys in this table
   * @see #keySet()
   */
  def keys: Enumeration[K] = {
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    val f: Int = if ((({
      t = table; t
    })) == null) 0
    else t.length
    new ConcurrentSHMap.KeyIterator[K, V](t, f, 0, f, this)
  }

  /**
   * Returns an enumeration of the values in this table.
   *
   * @return an enumeration of the values in this table
   * @see #values()
   */
  def elements: Enumeration[V] = {
    var t: Array[ConcurrentSHMap.Node[K, V]] = null
    val f: Int = if ((({
      t = table; t
    })) == null) 0
    else t.length
    new ConcurrentSHMap.ValueIterator[K, V](t, f, 0, f, this)
  }

  /**
   * Returns the number of mappings. This method should be used
   * instead of {@link #size} because a ConcurrentSHMap may
   * contain more mappings than can be represented as an int. The
   * value returned is an estimate; the actual count may differ if
   * there are concurrent insertions or removals.
   *
   * @return the number of mappings
   * @since 1.8
   */
  def mappingCount: Long = {
    val n: Long = sumCount
    if ((n < 0L)) 0L else n
  }

  /**
   * Returns a {@link Set} view of the keys in this map, using the
   * given common mapped value for any additions (i.e., {@link
   * Collection#add} and {@link Collection#addAll(Collection)}).
   * This is of course only appropriate if it is acceptable to use
   * the same value for all additions from this view.
   *
   * @param mappedValue the mapped value to use for any additions
   * @return the set view
   * @throws NullPointerException if the mappedValue is null
   */
  def keySet(mappedValue: V): ConcurrentSHMap.KeySetView[K, V] = {
    if (mappedValue == null) throw new NullPointerException
    new ConcurrentSHMap.KeySetView[K, V](this, mappedValue)
  }

  /**
   * Initializes table, using the size recorded in sizeCtl.
   */
  private final def initTable: Array[ConcurrentSHMap.Node[K, V]] = {
    var tab: Array[ConcurrentSHMap.Node[K, V]] = null
    var sc: Int = 0
    var break = false
    while (!break && ((({
      tab = table; tab
    })) == null || tab.length == 0)) {
      if ((({
        sc = sizeCtl; sc
      })) < 0) Thread.`yield`
      else if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, -1)) {
        try {
          if ((({
            tab = table; tab
          })) == null || tab.length == 0) {
            val n: Int = if ((sc > 0)) sc else ConcurrentSHMap.DEFAULT_CAPACITY
            @SuppressWarnings(Array("unchecked")) val nt: Array[ConcurrentSHMap.Node[K, V]] = new Array[ConcurrentSHMap.Node[_, _]](n).asInstanceOf[Array[ConcurrentSHMap.Node[K, V]]]
            table = ({
              tab = nt; tab
            })
            sc = n - (n >>> 2)
          }
        } finally {
          sizeCtl = sc
        }
        break = true //todo: break is not supported
      }
    }
    tab
  }

  /**
   * Adds to count, and if table is too small and not already
   * resizing, initiates transfer. If already resizing, helps
   * perform transfer if work is available.  Rechecks occupancy
   * after a transfer to see if another resize is already needed
   * because resizings are lagging additions.
   *
   * @param x the count to add
   * @param check if <0, don't check resize, if <= 1 only check if uncontended
   */
  private final def addCount(x: Long, check: Int) {
    var as: Array[ConcurrentSHMap.CounterCell] = null
    var b: Long = 0L
    var s: Long = 0L
    if ((({
      as = counterCells; as
    })) != null || !ConcurrentSHMap.U.compareAndSwapLong(this, ConcurrentSHMap.BASECOUNT, {b = baseCount; b}, {s = b + x; s})) {
      var a: ConcurrentSHMap.CounterCell = null
      var v: Long = 0L
      var m: Int = 0
      var uncontended: Boolean = true
      if (as == null || (({
        m = as.length - 1; m
      })) < 0 || (({
        a = as(MyThreadLocalRandom.getProbe & m); a
      })) == null || !(({
        uncontended = ConcurrentSHMap.U.compareAndSwapLong(a, ConcurrentSHMap.CELLVALUE, {v = a.value; v}, v + x); uncontended
      }))) {
        fullAddCount(x, uncontended)
        return
      }
      if (check <= 1) return
      s = sumCount
    }
    if (check >= 0) {
      var tab: Array[ConcurrentSHMap.Node[K, V]] = null
      var nt: Array[ConcurrentSHMap.Node[K, V]] = null
      var n: Int = 0
      var sc: Int = 0
      var break = false
      while (!break && (s >= (({
        sc = sizeCtl; sc
      })).toLong && (({
        tab = table; tab
      })) != null && (({
        n = tab.length; n
      })) < ConcurrentSHMap.MAXIMUM_CAPACITY)) {
        val rs: Int = ConcurrentSHMap.resizeStamp(n)
        if (sc < 0) {
          if ((sc >>> ConcurrentSHMap.RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 || sc == rs + ConcurrentSHMap.MAX_RESIZERS || (({
            nt = nextTable; nt
          })) == null || transferIndex <= 0) break = true //todo: break is not supported
          if (!break && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, sc + 1)) transfer(tab, nt)
        }
        else if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, (rs << ConcurrentSHMap.RESIZE_STAMP_SHIFT) + 2)) transfer(tab, null)
        if(!break) s = sumCount
      }
    }
  }

  /**
   * Helps transfer if a resize is in progress.
   */
  private[concurrent] final def helpTransfer(tab: Array[ConcurrentSHMap.Node[K, V]], f: ConcurrentSHMap.Node[K, V]): Array[ConcurrentSHMap.Node[K, V]] = {
    var nextTab: Array[ConcurrentSHMap.Node[K, V]] = null
    var sc: Int = 0
    if (tab != null && (f.isInstanceOf[ConcurrentSHMap.ForwardingNode[_, _]]) && (({
      nextTab = (f.asInstanceOf[ConcurrentSHMap.ForwardingNode[K, V]]).nextTable; nextTab
    })) != null) {
      val rs: Int = ConcurrentSHMap.resizeStamp(tab.length)
      var break = false
      while (!break && ((nextTab eq nextTable) && (table eq tab) && (({
        sc = sizeCtl; sc
      })) < 0)) {
        if ((sc >>> ConcurrentSHMap.RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 || sc == rs + ConcurrentSHMap.MAX_RESIZERS || transferIndex <= 0) break = true //todo: break is not supported
        if (!break && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, sc + 1)) {
          transfer(tab, nextTab)
          break = true //todo: break is not supported
        }
      }
      return nextTab
    }
    table
  }

  /**
   * Tries to presize table to accommodate the given number of elements.
   *
   * @param size number of elements (doesn't need to be perfectly accurate)
   */
  private final def tryPresize(size: Int) {
    val c: Int = if ((size >= (ConcurrentSHMap.MAXIMUM_CAPACITY >>> 1))) ConcurrentSHMap.MAXIMUM_CAPACITY else ConcurrentSHMap.tableSizeFor(size + (size >>> 1) + 1)
    var sc: Int = 0
    var break = false
    while (!break && ((({
      sc = sizeCtl; sc
    })) >= 0)) {
      val tab: Array[ConcurrentSHMap.Node[K, V]] = table
      var n: Int = 0
      if (tab == null || (({
        n = tab.length; n
      })) == 0) {
        n = if ((sc > c)) sc else c
        if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, -1)) {
          try {
            if (table eq tab) {
              @SuppressWarnings(Array("unchecked")) val nt: Array[ConcurrentSHMap.Node[K, V]] = new Array[ConcurrentSHMap.Node[_, _]](n).asInstanceOf[Array[ConcurrentSHMap.Node[K, V]]]
              table = nt
              sc = n - (n >>> 2)
            }
          } finally {
            sizeCtl = sc
          }
        }
      }
      else if (c <= sc || n >= ConcurrentSHMap.MAXIMUM_CAPACITY) break = true //todo: break is not supported
      else if (tab eq table) {
        val rs: Int = ConcurrentSHMap.resizeStamp(n)
        if (sc < 0) {
          var nt: Array[ConcurrentSHMap.Node[K, V]] = null
          if ((sc >>> ConcurrentSHMap.RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 || sc == rs + ConcurrentSHMap.MAX_RESIZERS || (({
            nt = nextTable; nt
          })) == null || transferIndex <= 0) break = true //todo: break is not supported
          if (!break && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, sc + 1)) transfer(tab, nt)
        }
        else if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, sc, (rs << ConcurrentSHMap.RESIZE_STAMP_SHIFT) + 2)) transfer(tab, null)
      }
    }
  }

  /**
   * Moves and/or copies the nodes in each bin to new table. See
   * above for explanation.
   */
  private final def transfer(tab: Array[ConcurrentSHMap.Node[K, V]], inNextTab: Array[ConcurrentSHMap.Node[K, V]]) {
    var nextTab: Array[ConcurrentSHMap.Node[K, V]] = inNextTab
    val n: Int = tab.length
    var stride: Int = 0
    if ((({
      stride = if ((ConcurrentSHMap.NCPU > 1)) (n >>> 3) / ConcurrentSHMap.NCPU else n; stride
    })) < ConcurrentSHMap.MIN_TRANSFER_STRIDE) stride = ConcurrentSHMap.MIN_TRANSFER_STRIDE
    if (nextTab == null) {
      try {
        @SuppressWarnings(Array("unchecked")) val nt: Array[ConcurrentSHMap.Node[K, V]] = new Array[ConcurrentSHMap.Node[_, _]](n << 1).asInstanceOf[Array[ConcurrentSHMap.Node[K, V]]]
        nextTab = nt
      }
      catch {
        case ex: Throwable => {
          sizeCtl = Integer.MAX_VALUE
          return
        }
      }
      nextTable = nextTab
      transferIndex = n
    }
    val nextn: Int = nextTab.length
    val fwd: ConcurrentSHMap.ForwardingNode[K, V] = new ConcurrentSHMap.ForwardingNode[K, V](nextTab)
    var advance: Boolean = true
    var finishing: Boolean = false

    {
      var i: Int = 0
      var bound: Int = 0
      while (true) {
        var f: ConcurrentSHMap.Node[K, V] = null
        var fh: Int = 0
        while (advance) {
          var nextIndex: Int = 0
          var nextBound: Int = 0
          if (({
            i -= 1; i
          }) >= bound || finishing) advance = false
          else if ((({
            nextIndex = transferIndex; nextIndex
          })) <= 0) {
            i = -1
            advance = false
          }
          else if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.TRANSFERINDEX, nextIndex, { nextBound = (if (nextIndex > stride) nextIndex - stride else 0); nextBound })) {
            bound = nextBound
            i = nextIndex - 1
            advance = false
          }
        }
        if (i < 0 || i >= n || i + n >= nextn) {
          var sc: Int = 0
          if (finishing) {
            nextTable = null
            table = nextTab
            sizeCtl = (n << 1) - (n >>> 1)
            return
          }
          if (ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.SIZECTL, {sc = sizeCtl; sc}, sc - 1)) {
            if ((sc - 2) != (ConcurrentSHMap.resizeStamp(n) << ConcurrentSHMap.RESIZE_STAMP_SHIFT)) return
            finishing = ({
              advance = true; advance
            })
            i = n
          }
        }
        else if ((({
          f = ConcurrentSHMap.tabAt(tab, i); f
        })) == null) advance = ConcurrentSHMap.casTabAt(tab, i, null, fwd)
        else if ((({
          fh = f.hash; fh
        })) == ConcurrentSHMap.MOVED) advance = true
        else {
          f synchronized {
            if (ConcurrentSHMap.tabAt(tab, i) eq f) {
              var ln: ConcurrentSHMap.Node[K, V] = null
              var hn: ConcurrentSHMap.Node[K, V] = null
              if (fh >= 0) {
                var runBit: Int = fh & n
                var lastRun: ConcurrentSHMap.Node[K, V] = f

                {
                  var p: ConcurrentSHMap.Node[K, V] = f.next
                  while (p != null) {
                    {
                      val b: Int = p.hash & n
                      if (b != runBit) {
                        runBit = b
                        lastRun = p
                      }
                    }
                    p = p.next
                  }
                }
                if (runBit == 0) {
                  ln = lastRun
                  hn = null
                }
                else {
                  hn = lastRun
                  ln = null
                }
                {
                  var p: ConcurrentSHMap.Node[K, V] = f
                  while (p ne lastRun) {
                    {
                      val ph: Int = p.hash
                      val pk: K = p.key
                      val pv: V = p.value
                      if ((ph & n) == 0) ln = new ConcurrentSHMap.Node[K, V](ph, pk, pv, ln)
                      else hn = new ConcurrentSHMap.Node[K, V](ph, pk, pv, hn)
                    }
                    p = p.next
                  }
                }
                ConcurrentSHMap.setTabAt(nextTab, i, ln)
                ConcurrentSHMap.setTabAt(nextTab, i + n, hn)
                ConcurrentSHMap.setTabAt(tab, i, fwd)
                advance = true
              }
              else if (f.isInstanceOf[ConcurrentSHMap.TreeBin[_, _]]) {
                val t: ConcurrentSHMap.TreeBin[K, V] = f.asInstanceOf[ConcurrentSHMap.TreeBin[K, V]]
                var lo: ConcurrentSHMap.TreeNode[K, V] = null
                var loTail: ConcurrentSHMap.TreeNode[K, V] = null
                var hi: ConcurrentSHMap.TreeNode[K, V] = null
                var hiTail: ConcurrentSHMap.TreeNode[K, V] = null
                var lc: Int = 0
                var hc: Int = 0

                {
                  var e: ConcurrentSHMap.Node[K, V] = t.first
                  while (e != null) {
                    {
                      val h: Int = e.hash
                      val p: ConcurrentSHMap.TreeNode[K, V] = new ConcurrentSHMap.TreeNode[K, V](h, e.key, e.value, null, null)
                      if ((h & n) == 0) {
                        if ((({
                          p.prev = loTail; p.prev
                        })) == null) lo = p
                        else loTail.next = p
                        loTail = p
                        lc += 1
                      }
                      else {
                        if ((({
                          p.prev = hiTail; p.prev
                        })) == null) hi = p
                        else hiTail.next = p
                        hiTail = p
                        hc += 1
                      }
                    }
                    e = e.next
                  }
                }
                ln = if ((lc <= ConcurrentSHMap.UNTREEIFY_THRESHOLD)) ConcurrentSHMap.untreeify(lo) else if ((hc != 0)) new ConcurrentSHMap.TreeBin[K, V](lo) else t
                hn = if ((hc <= ConcurrentSHMap.UNTREEIFY_THRESHOLD)) ConcurrentSHMap.untreeify(hi) else if ((lc != 0)) new ConcurrentSHMap.TreeBin[K, V](hi) else t
                ConcurrentSHMap.setTabAt(nextTab, i, ln)
                ConcurrentSHMap.setTabAt(nextTab, i + n, hn)
                ConcurrentSHMap.setTabAt(tab, i, fwd)
                advance = true
              }
            }
          }
        }
      }
    }
  }

  private[concurrent] final def sumCount: Long = {
    val as: Array[ConcurrentSHMap.CounterCell] = counterCells
    var a: ConcurrentSHMap.CounterCell = null
    var sum: Long = baseCount
    if (as != null) {
      {
        var i: Int = 0
        while (i < as.length) {
          {
            if ((({
              a = as(i); a
            })) != null) sum += a.value
          }
          ({
            i += 1; i
          })
        }
      }
    }
    return sum
  }

  private final def fullAddCount(x: Long, inWasUncontended: Boolean) {
    var wasUncontended: Boolean = inWasUncontended
    var h: Int = 0
    if ((({
      h = MyThreadLocalRandom.getProbe; h
    })) == 0) {
      MyThreadLocalRandom.localInit
      h = MyThreadLocalRandom.getProbe
      wasUncontended = true
    }
    var collide: Boolean = false
    var break = false
    while (!break) {
      var continue = false
      var as: Array[ConcurrentSHMap.CounterCell] = null
      var a: ConcurrentSHMap.CounterCell = null
      var n: Int = 0
      var v: Long = 0L
      if ((({
        as = counterCells; as
      })) != null && (({
        n = as.length; n
      })) > 0) {
        if ((({
          a = as((n - 1) & h); a
        })) == null) {
          if (cellsBusy == 0) {
            val r: ConcurrentSHMap.CounterCell = new ConcurrentSHMap.CounterCell(x)
            if (cellsBusy == 0 && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.CELLSBUSY, 0, 1)) {
              var created: Boolean = false
              try {
                var rs: Array[ConcurrentSHMap.CounterCell] = null
                var m: Int = 0
                var j: Int = 0
                if ((({
                  rs = counterCells; rs
                })) != null && (({
                  m = rs.length; m
                })) > 0 && rs(({
                  j = (m - 1) & h; j
                })) == null) {
                  rs(j) = r
                  created = true
                }
              } finally {
                cellsBusy = 0
              }
              if (created) break = true //todo: break is not supported
              if(!break) continue = true //todo: continue is not supported
            }
          }
          if(!break && !continue) collide = false
        }
        else if (!wasUncontended) wasUncontended = true
        else if (ConcurrentSHMap.U.compareAndSwapLong(a, ConcurrentSHMap.CELLVALUE, {v = a.value; v}, v + x)) break = true //todo: break is not supported
        else if ((counterCells ne as) || (n >= ConcurrentSHMap.NCPU)) collide = false
        else if (!collide) collide = true
        else if (cellsBusy == 0 && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.CELLSBUSY, 0, 1)) {
          try {
            if (counterCells eq as) {
              val rs: Array[ConcurrentSHMap.CounterCell] = new Array[ConcurrentSHMap.CounterCell](n << 1)

              {
                var i: Int = 0
                while (i < n) {
                  rs(i) = as(i)
                  ({
                    i += 1; i
                  })
                }
              }
              counterCells = rs
            }
          } finally {
            cellsBusy = 0
          }
          collide = false
          continue = true //todo: continue is not supported
        }
        if(!break && !continue) h = MyThreadLocalRandom.advanceProbe(h)
      }
      else if ((cellsBusy == 0) && (counterCells eq as) && ConcurrentSHMap.U.compareAndSwapInt(this, ConcurrentSHMap.CELLSBUSY, 0, 1)) {
        var init: Boolean = false
        try {
          if (counterCells eq as) {
            val rs: Array[ConcurrentSHMap.CounterCell] = new Array[ConcurrentSHMap.CounterCell](2)
            rs(h & 1) = new ConcurrentSHMap.CounterCell(x)
            counterCells = rs
            init = true
          }
        } finally {
          cellsBusy = 0
        }
        if (init) break = true //todo: break is not supported
      }
      else if (ConcurrentSHMap.U.compareAndSwapLong(this, ConcurrentSHMap.BASECOUNT, {v = baseCount; v}, v + x)) break = true //todo: break is not supported
    }
  }

  /**
   * Replaces all linked nodes in bin at given index unless table is
   * too small, in which case resizes instead.
   */
  private final def treeifyBin(tab: Array[ConcurrentSHMap.Node[K, V]], index: Int) {
    var b: ConcurrentSHMap.Node[K, V] = null
    var n: Int = 0
    val sc: Int = 0
    if (tab != null) {
      if ((({
        n = tab.length; n
      })) < ConcurrentSHMap.MIN_TREEIFY_CAPACITY) tryPresize(n << 1)
      else if ((({
        b = ConcurrentSHMap.tabAt(tab, index); b
      })) != null && b.hash >= 0) {
        b synchronized {
          if (ConcurrentSHMap.tabAt(tab, index) eq b) {
            var hd: ConcurrentSHMap.TreeNode[K, V] = null
            var tl: ConcurrentSHMap.TreeNode[K, V] = null

            {
              var e: ConcurrentSHMap.Node[K, V] = b
              while (e != null) {
                {
                  val p: ConcurrentSHMap.TreeNode[K, V] = new ConcurrentSHMap.TreeNode[K, V](e.hash, e.key, e.value, null, null)
                  if ((({
                    p.prev = tl; p.prev
                  })) == null) hd = p
                  else tl.next = p
                  tl = p
                }
                e = e.next
              }
            }
            ConcurrentSHMap.setTabAt(tab, index, new ConcurrentSHMap.TreeBin[K, V](hd))
          }
        }
      }
    }
  }

  /**
   * Computes initial batch value for bulk tasks. The returned value
   * is approximately exp2 of the number of times (minus one) to
   * split task by two before executing leaf action. This value is
   * faster to compute and more convenient to use as a guide to
   * splitting than is the depth, since it is used while dividing by
   * two anyway.
   */
  private[concurrent] final def batchFor(b: Long): Int = {
    var n: Long = 0L
    if (b == Long.MaxValue || (({
      n = sumCount; n
    })) <= 1L || n < b) return 0
    val sp: Int = ForkJoinPool.getCommonPoolParallelism << 2
    return if ((b <= 0L || (({
      n /= b; n
    })) >= sp)) sp
    else n.toInt
  }

  /**
   * Performs the given action for each (key, value).
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  def forEach(parallelismThreshold: Long, action: BiConsumer[_ >: K, _ >: V]) {
    if (action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachMappingTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, action).invoke
  }

  /**
   * Performs the given action for each non-null transformation
   * of each (key, value).
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case the action is not applied)
   * @param action the action
   * @param <U> the return type of the transformer
   * @since 1.8
   */
  def forEach[U](parallelismThreshold: Long, transformer: BiFunction[_ >: K, _ >: V, _ <: U], action: Consumer[_ >: U]) {
    if (transformer == null || action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachTransformedMappingTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, transformer, action).invoke
  }

  /**
   * Returns a non-null result from applying the given search
   * function on each (key, value), or null if none.  Upon
   * success, further element processing is suppressed and the
   * results of any other parallel invocations of the search
   * function are ignored.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param searchFunction a function returning a non-null
   *                       result on success, else null
   * @param <U> the return type of the search function
   * @return a non-null result from applying the given search
   *         function on each (key, value), or null if none
   * @since 1.8
   */
  def search[U](parallelismThreshold: Long, searchFunction: BiFunction[_ >: K, _ >: V, _ <: U]): U = {
    if (searchFunction == null) throw new NullPointerException
    new ConcurrentSHMap.SearchMappingsTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, searchFunction, new AtomicReference[U]).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all (key, value) pairs using the given reducer to
   * combine values, or null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case it is not combined)
   * @param reducer a commutative associative combining function
   * @param <U> the return type of the transformer
   * @return the result of accumulating the given transformation
   *         of all (key, value) pairs
   * @since 1.8
   */
  def reduce[U](parallelismThreshold: Long, transformer: BiFunction[_ >: K, _ >: V, _ <: U], reducer: BiFunction[_ >: U, _ >: U, _ <: U]): U = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceMappingsTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all (key, value) pairs using the given reducer to
   * combine values, and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all (key, value) pairs
   * @since 1.8
   */
  def reduceToDouble(parallelismThreshold: Long, transformer: ToDoubleBiFunction[_ >: K, _ >: V], basis: Double, reducer: DoubleBinaryOperator): Double = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceMappingsToDoubleTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all (key, value) pairs using the given reducer to
   * combine values, and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all (key, value) pairs
   * @since 1.8
   */
  def reduceToLong(parallelismThreshold: Long, transformer: ToLongBiFunction[_ >: K, _ >: V], basis: Long, reducer: LongBinaryOperator): Long = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceMappingsToLongTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all (key, value) pairs using the given reducer to
   * combine values, and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all (key, value) pairs
   * @since 1.8
   */
  def reduceToInt(parallelismThreshold: Long, transformer: ToIntBiFunction[_ >: K, _ >: V], basis: Int, reducer: IntBinaryOperator): Int = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceMappingsToIntTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Performs the given action for each key.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  def forEachKey(parallelismThreshold: Long, action: Consumer[_ >: K]) {
    if (action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachKeyTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, action).invoke
  }

  /**
   * Performs the given action for each non-null transformation
   * of each key.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case the action is not applied)
   * @param action the action
   * @param <U> the return type of the transformer
   * @since 1.8
   */
  def forEachKey[U](parallelismThreshold: Long, transformer: Function[_ >: K, _ <: U], action: Consumer[_ >: U]) {
    if (transformer == null || action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachTransformedKeyTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, transformer, action).invoke
  }

  /**
   * Returns a non-null result from applying the given search
   * function on each key, or null if none. Upon success,
   * further element processing is suppressed and the results of
   * any other parallel invocations of the search function are
   * ignored.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param searchFunction a function returning a non-null
   *                       result on success, else null
   * @param <U> the return type of the search function
   * @return a non-null result from applying the given search
   *         function on each key, or null if none
   * @since 1.8
   */
  def searchKeys[U](parallelismThreshold: Long, searchFunction: Function[_ >: K, _ <: U]): U = {
    if (searchFunction == null) throw new NullPointerException
    return new ConcurrentSHMap.SearchKeysTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, searchFunction, new AtomicReference[U]).invoke
  }

  /**
   * Returns the result of accumulating all keys using the given
   * reducer to combine values, or null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param reducer a commutative associative combining function
   * @return the result of accumulating all keys using the given
   *         reducer to combine values, or null if none
   * @since 1.8
   */
  def reduceKeys(parallelismThreshold: Long, reducer: BiFunction[_ >: K, _ >: K, _ <: K]): K = {
    if (reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.ReduceKeysTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all keys using the given reducer to combine values, or
   * null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case it is not combined)
   * @param reducer a commutative associative combining function
   * @param <U> the return type of the transformer
   * @return the result of accumulating the given transformation
   *         of all keys
   * @since 1.8
   */
  def reduceKeys[U](parallelismThreshold: Long, transformer: Function[_ >: K, _ <: U], reducer: BiFunction[_ >: U, _ >: U, _ <: U]): U = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceKeysTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all keys using the given reducer to combine values, and
   * the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all keys
   * @since 1.8
   */
  def reduceKeysToDouble(parallelismThreshold: Long, transformer: ToDoubleFunction[_ >: K], basis: Double, reducer: DoubleBinaryOperator): Double = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceKeysToDoubleTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all keys using the given reducer to combine values, and
   * the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all keys
   * @since 1.8
   */
  def reduceKeysToLong(parallelismThreshold: Long, transformer: ToLongFunction[_ >: K], basis: Long, reducer: LongBinaryOperator): Long = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceKeysToLongTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all keys using the given reducer to combine values, and
   * the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all keys
   * @since 1.8
   */
  def reduceKeysToInt(parallelismThreshold: Long, transformer: ToIntFunction[_ >: K], basis: Int, reducer: IntBinaryOperator): Int = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceKeysToIntTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Performs the given action for each value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  def forEachValue(parallelismThreshold: Long, action: Consumer[_ >: V]) {
    if (action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachValueTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, action).invoke
  }

  /**
   * Performs the given action for each non-null transformation
   * of each value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case the action is not applied)
   * @param action the action
   * @param <U> the return type of the transformer
   * @since 1.8
   */
  def forEachValue[U](parallelismThreshold: Long, transformer: Function[_ >: V, _ <: U], action: Consumer[_ >: U]) {
    if (transformer == null || action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachTransformedValueTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, transformer, action).invoke
  }

  /**
   * Returns a non-null result from applying the given search
   * function on each value, or null if none.  Upon success,
   * further element processing is suppressed and the results of
   * any other parallel invocations of the search function are
   * ignored.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param searchFunction a function returning a non-null
   *                       result on success, else null
   * @param <U> the return type of the search function
   * @return a non-null result from applying the given search
   *         function on each value, or null if none
   * @since 1.8
   */
  def searchValues[U](parallelismThreshold: Long, searchFunction: Function[_ >: V, _ <: U]): U = {
    if (searchFunction == null) throw new NullPointerException
    return new ConcurrentSHMap.SearchValuesTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, searchFunction, new AtomicReference[U]).invoke
  }

  /**
   * Returns the result of accumulating all values using the
   * given reducer to combine values, or null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param reducer a commutative associative combining function
   * @return the result of accumulating all values
   * @since 1.8
   */
  def reduceValues(parallelismThreshold: Long, reducer: BiFunction[_ >: V, _ >: V, _ <: V]): V = {
    if (reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.ReduceValuesTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all values using the given reducer to combine values, or
   * null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case it is not combined)
   * @param reducer a commutative associative combining function
   * @param <U> the return type of the transformer
   * @return the result of accumulating the given transformation
   *         of all values
   * @since 1.8
   */
  def reduceValues[U](parallelismThreshold: Long, transformer: Function[_ >: V, _ <: U], reducer: BiFunction[_ >: U, _ >: U, _ <: U]): U = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceValuesTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all values using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all values
   * @since 1.8
   */
  def reduceValuesToDouble(parallelismThreshold: Long, transformer: ToDoubleFunction[_ >: V], basis: Double, reducer: DoubleBinaryOperator): Double = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceValuesToDoubleTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all values using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all values
   * @since 1.8
   */
  def reduceValuesToLong(parallelismThreshold: Long, transformer: ToLongFunction[_ >: V], basis: Long, reducer: LongBinaryOperator): Long = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceValuesToLongTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all values using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all values
   * @since 1.8
   */
  def reduceValuesToInt(parallelismThreshold: Long, transformer: ToIntFunction[_ >: V], basis: Int, reducer: IntBinaryOperator): Int = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceValuesToIntTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Performs the given action for each entry.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  def forEachEntry(parallelismThreshold: Long, action: Consumer[_ >: Map.Entry[K, V]]) {
    if (action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachEntryTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, action).invoke
  }

  /**
   * Performs the given action for each non-null transformation
   * of each entry.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case the action is not applied)
   * @param action the action
   * @param <U> the return type of the transformer
   * @since 1.8
   */
  def forEachEntry[U](parallelismThreshold: Long, transformer: Function[Map.Entry[K, V], _ <: U], action: Consumer[_ >: U]) {
    if (transformer == null || action == null) throw new NullPointerException
    new ConcurrentSHMap.ForEachTransformedEntryTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, transformer, action).invoke
  }

  /**
   * Returns a non-null result from applying the given search
   * function on each entry, or null if none.  Upon success,
   * further element processing is suppressed and the results of
   * any other parallel invocations of the search function are
   * ignored.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param searchFunction a function returning a non-null
   *                       result on success, else null
   * @param <U> the return type of the search function
   * @return a non-null result from applying the given search
   *         function on each entry, or null if none
   * @since 1.8
   */
  def searchEntries[U](parallelismThreshold: Long, searchFunction: Function[Map.Entry[K, V], _ <: U]): U = {
    if (searchFunction == null) throw new NullPointerException
    return new ConcurrentSHMap.SearchEntriesTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, searchFunction, new AtomicReference[U]).invoke
  }

  /**
   * Returns the result of accumulating all entries using the
   * given reducer to combine values, or null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param reducer a commutative associative combining function
   * @return the result of accumulating all entries
   * @since 1.8
   */
  def reduceEntries(parallelismThreshold: Long, reducer: BiFunction[Map.Entry[K, V], Map.Entry[K, V], _ <: Map.Entry[K, V]]): Map.Entry[K, V] = {
    if (reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.ReduceEntriesTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all entries using the given reducer to combine values,
   * or null if none.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element, or null if there is no transformation (in
   *                    which case it is not combined)
   * @param reducer a commutative associative combining function
   * @param <U> the return type of the transformer
   * @return the result of accumulating the given transformation
   *         of all entries
   * @since 1.8
   */
  def reduceEntries[U](parallelismThreshold: Long, transformer: Function[Map.Entry[K, V], _ <: U], reducer: BiFunction[_ >: U, _ >: U, _ <: U]): U = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceEntriesTask[K, V, U](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all entries using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all entries
   * @since 1.8
   */
  def reduceEntriesToDouble(parallelismThreshold: Long, transformer: ToDoubleFunction[Map.Entry[K, V]], basis: Double, reducer: DoubleBinaryOperator): Double = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceEntriesToDoubleTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all entries using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all entries
   * @since 1.8
   */
  def reduceEntriesToLong(parallelismThreshold: Long, transformer: ToLongFunction[Map.Entry[K, V]], basis: Long, reducer: LongBinaryOperator): Long = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceEntriesToLongTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }

  /**
   * Returns the result of accumulating the given transformation
   * of all entries using the given reducer to combine values,
   * and the given basis as an identity value.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param transformer a function returning the transformation
   *                    for an element
   * @param basis the identity (initial default value) for the reduction
   * @param reducer a commutative associative combining function
   * @return the result of accumulating the given transformation
   *         of all entries
   * @since 1.8
   */
  def reduceEntriesToInt(parallelismThreshold: Long, transformer: ToIntFunction[Map.Entry[K, V]], basis: Int, reducer: IntBinaryOperator): Int = {
    if (transformer == null || reducer == null) throw new NullPointerException
    return new ConcurrentSHMap.MapReduceEntriesToIntTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, null, transformer, basis, reducer).invoke
  }
}
