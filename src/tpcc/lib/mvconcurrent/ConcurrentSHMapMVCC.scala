package ddbt.tpcc.lib.mvconcurrent

import java.util.Map
import java.util.NoSuchElementException
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.locks.LockSupport
import java.util.concurrent.locks.ReentrantLock
import java.util.concurrent.CountedCompleter
import java.util.concurrent.ForkJoinPool
import ddbt.tpcc.lib.util.Comp._
import sun.misc.Unsafe
import ddbt.tpcc.lib.concurrent.MyThreadLocalRandom
import ConcurrentSHMapMVCC._
import ddbt.tpcc.tx._
import MVCCTpccTableV3._

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
 * <p>A {@link Set} projection of a ConcurrentSHMapMVCC may be created
 * (using {@link #newKeySet()} or {@link #newKeySet(int)}), or viewed
 * (using {@link #keySet(Object)} when only keys are of interest, and the
 * mapped values are (perhaps transiently) not used or all take the
 * same mapping value.
 *
 * <p>A ConcurrentSHMapMVCC can be used as scalable frequency map (a
 * form of histogram or multiset) by using {@link
 * java.util.concurrent.atomic.LongAdder} values and initializing via
 * {@link #computeIfAbsent computeIfAbsent}. For example, to add a count
 * to a {@code ConcurrentSHMapMVCC<String,LongAdder> freqs}, you can use
 * {@code freqs.computeIfAbsent(k -> new LongAdder()).increment();}
 *
 * <p>This class and its views and iterators implement all of the
 * <em>optional</em> methods of the {@link Map} and {@link Iterator}
 * interfaces.
 *
 * <p>Like {@link Hashtable} but unlike {@link HashMap}, this class
 * does <em>not</em> allow {@code null} to be used as a key or value.
 *
 * <p>ConcurrentSHMapMVCCs support a set of sequential and parallel bulk
 * operations that, unlike most {@link Stream} methods, are designed
 * to be safely, and often sensibly, applied even with maps that are
 * being concurrently updated by other threads; for example, when
 * computing a snapshot summary of the values in a shared registry.
 * There are three kinds of operation, each with four forms, accepting
 * functions with Keys, Values, Entries, and (Key, Value) arguments
 * and/or return values. Because the elements of a ConcurrentSHMapMVCC
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
 * from those of ConcurrentSHMapMVCC: Any non-null result returned
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
 * This implementation differs from MVCC implementation described in
 * "Fast Serializable Multi-Version Concurrency Control for Main-Memory Database Systems"
 * by these points:
 * - detecting conflicts in the level of record (instead of attribute)
 * - keeping only one image version per transaction (instead of a new 
 *   version per operation in the transaction)
 * - keeping the image itself, instead of the before image that needs
 *   applying recursively to reach to the actual committed value
 *
 * @since 1.5
 * @author Doug Lea
 * @param <K> the type of keys maintained by this map
 * @param <V> the type of mapped values
 */
object ConcurrentSHMapMVCC {
  type SEntryMVCC[K,V <: Product] = Node[K,V]
  
  type Operation = Int
  val INSERT_OP:Operation = 1
  val DELETE_OP:Operation = 2
  val UPDATE_OP:Operation = 3

  val NO_TRANSACTION_AVAILABLE = null

  val ONE_THREAD_NO_PARALLELISM = Long.MaxValue
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
  private val MAX_ARRAY_SIZE: Int = Integer.MAX_VALUE - 8
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
  val TREEIFY_THRESHOLD: Int = 8
  /**
   * The bin count threshold for untreeifying a (split) bin during a
   * resize operation. Should be less than TREEIFY_THRESHOLD, and at
   * most 6 to mesh with shrinkage detection under removal.
   */
  val UNTREEIFY_THRESHOLD: Int = 6
  /**
   * The smallest table capacity for which bins may be treeified.
   * (Otherwise the table is resized if too many nodes in a bin.)
   * The value should be at least 4 * TREEIFY_THRESHOLD to avoid
   * conflicts between resizing and treeification thresholds.
   */
  private val MIN_TREEIFY_CAPACITY: Int = 64
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
  private val MOVED: Int = -1
  private val TREEBIN: Int = -2
  private val RESERVED: Int = -3
  private val HASH_BITS: Int = 0x7fffffff
  /** Number of CPUS, to place bounds on some sizings */
  private val NCPU: Int = Runtime.getRuntime.availableProcessors

  def debug(msg: => String)(implicit xact:Transaction) = MVCCTpccTableV3.debug(msg)

  final class DeltaVersion[K,V <: Product](val vXact:Transaction, @volatile var entry:SEntryMVCC[K,V], @volatile var img:V, @volatile var cols:List[Int]=Nil /*all columns*/, @volatile var op: Operation=INSERT_OP, @volatile var next: DeltaVersion[K,V]=null, @volatile var prev: DeltaVersion[K,V]=null) {
    vXact.undoBuffer += this
    if(next != null) next.prev = this
    if(prev != null) prev.next = this

    @inline
    final def getImage: V = /*if(op == DELETE_OP) null.asInstanceOf[V] else*/ img
    @inline
    final def getKey: K = entry.key
    @inline
    final def getMap = entry.map
    @inline
    final def getTable = getMap.tbl
    @inline
    final def project(part: Int) = getMap.projs(part).apply(getKey, getImage)

    // @inline //inlining is disabled during development
    final def setEntryValue(newValue: V)(implicit xact:Transaction): Unit = {
      var insertOrUpdate = false
      if(op == DELETE_OP) {
        entry.setTheValue(newValue, INSERT_OP)
        insertOrUpdate = true
      } else if(newValue == null) {
        entry.setTheValue(newValue, DELETE_OP)
      } else {
        entry.setTheValue(newValue, UPDATE_OP, getModifiedColIds(newValue, img))
        insertOrUpdate = true
      }

      if(insertOrUpdate) {
        val map = getMap
        if (map.idxs!=Nil) map.idxs.foreach(_.set(entry.value))
      }
    }

    // @inline //inlining is disabled during development
    final def isVisible(implicit xact:Transaction): Boolean = {
      /*val res = */(!isDeleted) && ((vXact eq xact)|| 
          (vXact.isCommitted && vXact.xactId < xact.startTS 
              && ((prev == null) || prev.vXact.commitTS > xact.startTS)
          )
        )
      // if(res) {
      //   debug("Is it visible => " + this)
      //   debug("\tisDeleted = " + isDeleted)
      //   debug("\tvXact eq xact = " + (vXact eq xact))
      //   if(!(vXact eq xact)) {
      //     debug("\tvXact.isCommitted = " + vXact.isCommitted)
      //     debug("\tvXact.xactId < xact.startTS  = " + (vXact.xactId < xact.startTS ))
      //     debug("\tprev = " + prev)
      //     if(prev != null) {
      //       debug("\tprev.vXact.commitTS > xact.startTS = " + (prev.vXact.commitTS > xact.startTS))
      //     }
      //   }
      // }
      // val res = entry.getTheValue eq this
      // res
    }

    @inline
    final def isDeleted = (op == DELETE_OP)

    @inline
    def opStr = op match {
      case INSERT_OP => "INSERT"
      case DELETE_OP => "DELETE"
      case UPDATE_OP => "UPDATE"
      case _ => "UNKNOWN"
    }

    // @inline //inlining is disabled during development
    def gcRemove(implicit xact:Transaction) {
      if(next != null) throw new RuntimeException("There are more than one versions pending for a single garbage collection.")
      remove
    }

    // @inline //inlining is disabled during development
    def remove(implicit xact:Transaction) {
      val map = getMap
      if(/*isDeleted &&*/ prev == null && next == null) {
        // debug("removing node " + getKey + " from " + getTable)
        map.replaceNode(getKey)
      }
      if(prev != null){
        prev.next = next
        prev = null
      } else {
        entry.synchronized {
          if(entry.value ne this) throw new RuntimeException("Only the head element in version list can have a null previous pointer.")
          if(next != null) entry.value = next //this entry might still be in use
        }
      }
      if(next != null) {
        next.prev = prev
        next = null
      }

      if (map.idxs!=Nil) map.idxs.foreach(_.del(this))
    }

    final override def toString = "<"+img+" with op="+opStr+(if(op == UPDATE_OP) " on cols="+cols else "") + " for " + getTable +" written by " + vXact + ">"

    // final override def hashCode: Int = {
    //   entry.hashCode ^ img.hashCode
    // }

    final override def equals(o: Any): Boolean = {
      this eq o.asInstanceOf[AnyRef]
      // val e = o.asInstanceOf[DeltaVersion[K, V]]
      // (refEquals(e.entry, entry) || (e.entry == entry)) &&
      // (refEquals(e.img, img) || (e.img == img))
    }
  }

  /**
   * Key-value entry.  This class is never exported out as a
   * user-mutable Map.Entry (i.e., one supporting setValue; see
   * MapEntry below), but can be used for read-only traversals used
   * in bulk tasks.  Subclasses of Node with a negative hash field
   * are special, and contain null keys and values (but are never
   * exported).  Otherwise, keys and vals are never null.
   */
  class Node[K, V <: Product](val map:ConcurrentSHMapMVCC[K,V], val key: K, val hash: Int, @volatile var value: DeltaVersion[K,V], @volatile var next: Node[K, V]) /*extends Map.Entry[K, V]*/ {
    
    def this(m:ConcurrentSHMapMVCC[K,V], h: Int, k: K, v: V, n: Node[K, V]=null)(implicit xact:Transaction) {
      this(m,k,h,null,n)
      setTheValue(v, INSERT_OP)(xact)
    }
    def this(m:ConcurrentSHMapMVCC[K,V], h: Int, k: K, v: DeltaVersion[K,V], n: Node[K, V]) {
      this(m,k,h,v,n)

      //fixing the changed reference to the entry from delta versions
      if(value != null) {
        var dv = value
        while(dv != null) {
          dv.entry = this
          dv = dv.next
        }
        dv = value.prev
        while(dv != null) {
          dv.entry = this
          dv = dv.prev
        }
      }
    }

    final def getKey: K = {
      key
    }

    // @inline //inlining is disabled during development
    final def getValueImage(implicit xact:Transaction) = this.synchronized { val v = getTheValue; if(v == null) null.asInstanceOf[V] else v.getImage }

    // final def getValue: V = throw new UnsupportedOperationException("SEntryMVCC.getValue without passing the xact is not supported.")
    // final def setValue(newValue: V): V = throw new UnsupportedOperationException("SEntryMVCC.setValue without passing the xact is not supported.")

    final def getTheValue(implicit xact:Transaction) = this.synchronized {
      // debug("getting the value for " + key + " in " + Integer.toHexString(System.identityHashCode(this)))
      var res: DeltaVersion[K,V] = value
      var found = false
      do {
        if(res == null) {
          debug("getting the value for " + key + " in " + Integer.toHexString(System.identityHashCode(this)))
          debug("\tchecking >> " + res)
        }
        val resXact = res.vXact
        if((resXact eq xact) || 
           (resXact.isCommitted && resXact.xactId < xact.startTS)) {
          found = true
        }
        if(!found) {
          res = res.next
        }
      } while(!found && res != null);
      if(found) res else null
    }

    // @inline //inlining is disabled during development
    final def setTheValue(newValue: V, op: Operation, cols:List[Int]=Nil)(implicit xact:Transaction): Unit = this.synchronized {
      // val oldValue: V = NULL_VALUE
      if(value == null) {
        // debug("setting the value for " + key + " in " + Integer.toHexString(System.identityHashCode(this)))
        value = new DeltaVersion(xact,this,newValue,cols,op)
        // debug("\t case 1 => " + value)
      } else {
        // debug("setting the value for " + key + " in " + Integer.toHexString(System.identityHashCode(this)))
        if(value.vXact eq xact) {
          value.img = newValue
          if((value.op == UPDATE_OP) && (op == UPDATE_OP)) {
            value.cols = value.cols.union(cols).sorted
            value.op = op
            debug("\t case 2 => " + value)
          } else {
            value.cols = cols
            value.op = op
            debug("\t case 3 => " + value)
          }
        } else {
          if(!value.vXact.isCommitted) throw new MVCCConcurrentWriteException("%s has already written on this object (%s<%s>), so %s should get aborted.".format(value.vXact, map.tbl, key, xact))
          value = new DeltaVersion(xact,this,newValue,cols,op,value)
          debug("\t case 4 => " + value)
        }
      }
      // oldValue
    }

    final override def hashCode: Int = {
      key.hashCode //^ value.hashCode
    }

    final override def toString: String = {
      throw new UnsupportedOperationException("The toString method is not supported in SEntryMVCC. You should use the overloaded method accepting the Transaction as an extra parameter.")
    }
    // final def toString(implicit xact:Transaction): String = {
    //   key + "=" + getValueImage
    // }

    final override def equals(o: Any): Boolean = {
      //TODO add safety checks (checking for null or isInstanceOf)?
      if(o == null) false else {
        val k: K = o.asInstanceOf[SEntryMVCC[K, V]].key
        (k != null) && (refEquals(k, key) || (k == key))
      }
    }

    // final def equals(o: Any)(implicit xact:Transaction): Boolean = {
    //   var k: K = null.asInstanceOf[K]
    //   var v: V = NULL_VALUE
    //   var u: V = NULL_VALUE
    //   var e: SEntryMVCC[K, V] = null
    //   (o.isInstanceOf[SEntryMVCC[_, _]] && {
    //     k = {
    //       e = o.asInstanceOf[SEntryMVCC[K, V]]; e
    //     }.getKey; k
    //   } != null && (({
    //     v = e.getValueImage; v
    //   })) != null && (refEquals(k, key) || (k == key)) && (refEquals(v, ({
    //     u = getValueImage; u
    //   })) || (v == u)))
    // }

    /**
     * Virtualized support for map.get(); overridden in subclasses.
     */
    def find(h: Int, k: K)(implicit ord: math.Ordering[K]): Node[K, V] = {
      var e: Node[K, V] = this
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
      null
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
  // @inline //inlining is disabled during development
  final def spread(h: Int): Int = (h ^ (h >>> 16)) & HASH_BITS

  /**
   * Returns a power of two table size for the given desired capacity.
   * See Hackers Delight, sec 3.2
   */
  // @inline //inlining is disabled during development
  final private def tableSizeFor(c: Int): Int = {
    var n: Int = c - 1
    n |= n >>> 1
    n |= n >>> 2
    n |= n >>> 4
    n |= n >>> 8
    n |= n >>> 16
    if ((n < 0)) 1 else if ((n >= MAXIMUM_CAPACITY)) MAXIMUM_CAPACITY else n + 1
  }

  /**
   * Returns k.compareTo(x) if x matches kc (k's screened comparable
   * class), else 0.
   */
  @SuppressWarnings(Array("rawtypes", "unchecked")) // @inline //inlining is disabled during development
  final def compareComparables[K](k: K, x: K)(implicit ord: math.Ordering[K]): Int = {
    ord.compare(k, x)
  }

  @SuppressWarnings(Array("unchecked")) // @inline //inlining is disabled during development
  final def tabAt[K, V <: Product](tab: Array[Node[K, V]], i: Int): Node[K, V] = {
    U.getObjectVolatile(tab, (i.toLong << ASHIFT) + ABASE).asInstanceOf[Node[K, V]]
  }

  // @inline //inlining is disabled during development
  final def casTabAt[K, V <: Product](tab: Array[Node[K, V]], i: Int, c: Node[K, V], v: Node[K, V]): Boolean = {
    U.compareAndSwapObject(tab, (i.toLong << ASHIFT) + ABASE, c, v)
  }

  // @inline //inlining is disabled during development
  final def setTabAt[K, V <: Product](tab: Array[Node[K, V]], i: Int, v: Node[K, V]) {
    U.putObjectVolatile(tab, (i.toLong << ASHIFT) + ABASE, v)
  }

  /**
   * A node inserted at head of bins during transfer operations.
   */
  final class ForwardingNode[K, V <: Product](val nextTable: Array[Node[K, V]]) extends Node[K, V](null,null.asInstanceOf[K],MOVED, null, null) {

    override def find(h: Int, k: K)(implicit ord: math.Ordering[K]): Node[K, V] = {
      {
        var tab: Array[Node[K, V]] = nextTable
        while (true) {
          var continue = false
          var e: Node[K, V] = null
          var n: Int = 0
          if (k == null || tab == null || (({
            n = tab.length; n
          })) == 0 || (({
            e = tabAt(tab, (n - 1) & h); e
          })) == null) return null
          while (!continue) {
            var eh: Int = 0
            var ek: K = null.asInstanceOf[K]
            if ((({
              eh = e.hash; eh
            })) == h && (refEquals((({
              ek = e.key; ek
            })), k) || (ek != null && (k == ek)))) return e
            if (eh < 0) {
              if (e.isInstanceOf[ForwardingNode[_, _]]) {
                tab = (e.asInstanceOf[ForwardingNode[K, V]]).nextTable
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
  final class ReservationNode[K, V <: Product] extends Node[K, V](null,null.asInstanceOf[K], RESERVED, null, null) {
    override def find(h: Int, k: K)(implicit ord: math.Ordering[K]): Node[K, V] = null
  }

  /**
   * Returns the stamp bits for resizing a table of size n.
   * Must be negative when shifted left by RESIZE_STAMP_SHIFT.
   */
  def resizeStamp(n: Int): Int = {
    Integer.numberOfLeadingZeros(n) | (1 << (RESIZE_STAMP_BITS - 1))
  }

  /**
   * A padded cell for distributing counts.  Adapted from LongAdder
   * and Striped64.  See their internal docs for explanation.
   */
  @sun.misc.Contended final class CounterCell {
    @volatile
    var value: Long = 0L

    def this(x: Long) {
      this()
      value = x
    }
  }

  /**
   * Returns a list on non-TreeNodes replacing those in given list.
   */
  def untreeify[K, V <: Product](b: Node[K, V]): Node[K, V] = {
    var hd: Node[K, V] = null
    var tl: Node[K, V] = null

    {
      var q: Node[K, V] = b
      while (q != null) {
        {
          val p: Node[K, V] = new Node[K, V](b.map, q.hash, q.key, q.value, null)
          if (tl == null) hd = p
          else tl.next = p
          tl = p
        }
        q = q.next
      }
    }
    hd
  }

  /**
   * Nodes for use in TreeBins
   */
  final class TreeNode[K, V <: Product](m:ConcurrentSHMapMVCC[K,V], h: Int, k: K, v: DeltaVersion[K,V], n: Node[K, V], var parent: TreeNode[K, V]) extends Node[K, V](m, h, k, v, n) {

    def this(m:ConcurrentSHMapMVCC[K,V], h: Int, k: K, v: V, n: Node[K, V], p: TreeNode[K, V])(implicit xact:Transaction) {
      this(m,h,k,null,n,p)
      setTheValue(v,INSERT_OP)(xact)
    }

    var left: TreeNode[K, V] = null
    var right: TreeNode[K, V] = null
    var prev: TreeNode[K, V] = null
    var red: Boolean = false

    override def find(h: Int, k: K)(implicit ord: math.Ordering[K]): Node[K, V] = {
      findTreeNode(h, k)
    }

    /**
     * Returns the TreeNode (or null if not found) for the given key
     * starting at given root.
     */
    final def findTreeNode(h: Int, k: K)(implicit ord: math.Ordering[K]): TreeNode[K, V] = {
      if (k != null) {
        var p: TreeNode[K, V] = this
        do {
          var ph: Int = 0
          var dir: Int = 0
          var pk: K = null.asInstanceOf[K]
          var q: TreeNode[K, V] = null
          val pl: TreeNode[K, V] = p.left
          val pr: TreeNode[K, V] = p.right
          if ((({
            ph = p.hash; ph
          })) > h) p = pl
          else if (ph < h) p = pr
          else if (refEquals((({
            pk = p.key; pk
          })), k) || (pk != null && (k == pk))) return p
          else if (pl == null) p = pr
          else if (pr == null) p = pl
          else if ({
            dir = compareComparables(k, pk); dir
          } != 0) p = if ((dir < 0)) pl else pr
          else if ((({
            q = pr.findTreeNode(h, k); q
          })) != null) return q
          else p = pl
        } while (p != null)
      }
      null
    }
  }

  /**
   * TreeNodes used at the heads of bins. TreeBins do not hold user
   * keys or values, but instead point to list of TreeNodes and
   * their root. They also maintain a parasitic read-write lock
   * forcing writers (who hold bin lock) to wait for readers (who do
   * not) to complete before tree restructuring operations.
   */
  object TreeBin {
    val WRITER: Int = 1
    val WAITER: Int = 2
    val READER: Int = 4

    /**
     * Tie-breaking utility for ordering insertions when equal
     * hashCodes and non-comparable. We don't require a total
     * order, just a consistent insertion rule to maintain
     * equivalence across rebalancings. Tie-breaking further than
     * necessary simplifies testing a bit.
     */
    def tieBreakOrder(a: Any, b: Any): Int = {
      var d: Int = 0
      if (a == null || b == null || (({
        d = a.getClass.getName.compareTo(b.getClass.getName); d
      })) == 0) d = (if (System.identityHashCode(a) <= System.identityHashCode(b)) -1 else 1)
      d
    }

    def rotateLeft[K, V <: Product](rootIn: TreeNode[K, V], p: TreeNode[K, V]): TreeNode[K, V] = {
      var root: TreeNode[K, V] = rootIn
      var r: TreeNode[K, V] = null
      var pp: TreeNode[K, V] = null
      var rl: TreeNode[K, V] = null
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
      root
    }

    def rotateRight[K, V <: Product](rootIn: TreeNode[K, V], p: TreeNode[K, V]): TreeNode[K, V] = {
      var root: TreeNode[K, V] = rootIn
      var l: TreeNode[K, V] = null
      var pp: TreeNode[K, V] = null
      var lr: TreeNode[K, V] = null
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
      root
    }

    def balanceInsertion[K, V <: Product](rootIn: TreeNode[K, V], xIn: TreeNode[K, V]): TreeNode[K, V] = {
      var root: TreeNode[K, V] = rootIn
      var x: TreeNode[K, V] = xIn
      x.red = true

      var xp: TreeNode[K, V] = null
      var xpp: TreeNode[K, V] = null
      var xppl: TreeNode[K, V] = null
      var xppr: TreeNode[K, V] = null
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

    def balanceDeletion[K, V <: Product](rootIn: TreeNode[K, V], xIn: TreeNode[K, V]): TreeNode[K, V] = {
      var root: TreeNode[K, V] = rootIn
      var x: TreeNode[K, V] = xIn

      {
        var xp: TreeNode[K, V] = null
        var xpl: TreeNode[K, V] = null
        var xpr: TreeNode[K, V] = null
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
              val sl: TreeNode[K, V] = xpr.left
              var sr: TreeNode[K, V] = xpr.right
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
              var sl: TreeNode[K, V] = xpl.left
              val sr: TreeNode[K, V] = xpl.right
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
    def checkInvariants[K, V <: Product](t: TreeNode[K, V]): Boolean = {
      val tp: TreeNode[K, V] = t.parent
      val tl: TreeNode[K, V] = t.left
      val tr: TreeNode[K, V] = t.right
      val tb: TreeNode[K, V] = t.prev
      val tn: TreeNode[K, V] = t.next.asInstanceOf[TreeNode[K, V]]
      if ((tb != null) && (tb.next ne t)) false
      else if ((tn != null) && (tn.prev ne t)) false
      else if ((tp != null) && (t ne tp.left) && (t ne tp.right)) false
      else if ((tl != null) && ((tl.parent ne t) || (tl.hash > t.hash))) false
      else if ((tr != null) && ((tr.parent ne t) || (tr.hash < t.hash))) false
      else if (t.red && tl != null && tl.red && tr != null && tr.red) false
      else if (tl != null && !checkInvariants(tl)) false
      else if (tr != null && !checkInvariants(tr)) false
      else true
    }

    private val U: Unsafe = MyThreadLocalRandom.getUnsafe
    val k: Class[_] = classOf[TreeBin[_, _]]
    private val LOCKSTATE: Long = U.objectFieldOffset(k.getDeclaredField("lockState"))
  }

  final class TreeBin[K, V <: Product](m:ConcurrentSHMapMVCC[K,V]) extends Node[K, V](m, null.asInstanceOf[K], TREEBIN, null, null) {
    var root: TreeNode[K, V] = null
    @volatile
    var first: TreeNode[K, V] = null
    @volatile
    var waiter: Thread = null
    @volatile
    var lockState: Int = 0

    /**
     * Creates bin with initial set of nodes headed by b.
     */
    def this(b: TreeNode[K, V],m:ConcurrentSHMapMVCC[K,V])(implicit ord: math.Ordering[K]) {
      this(m)
      this.first = b
      var r: TreeNode[K, V] = null

      {
        var x: TreeNode[K, V] = b
        var next: TreeNode[K, V] = null
        while (x != null) {
          {
            next = x.next.asInstanceOf[TreeNode[K, V]]
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

              {
                var p: TreeNode[K, V] = r
                var break = false
                while (!break) {
                  var dir: Int = 0
                  var ph: Int = 0
                  val pk: K = p.key
                  if ((({
                    ph = p.hash; ph
                  })) > h) dir = -1
                  else if (ph < h) dir = 1
                  else if ({
                    dir = compareComparables(k, pk)(ord); dir
                  } == 0) dir = TreeBin.tieBreakOrder(k, pk)
                  val xp: TreeNode[K, V] = p
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
    final override def find(h: Int, k: K)(implicit ord: math.Ordering[K]): Node[K, V] = {
      if (k != null) {
        {
          var e: Node[K, V] = first
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
              var r: TreeNode[K, V] = null
              var p: TreeNode[K, V] = null
              try {
                p = (if ((({
                  r = root; r
                })) == null) null
                else r.findTreeNode(h, k))
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
      null
    }

    /**
     * Finds or adds a node.
     * @return null if added
     */
    final def putTreeVal(h: Int, k: K, v: V, onInsert:TreeNode[K, V] => Unit)(implicit xact:Transaction, ord: math.Ordering[K]): TreeNode[K, V] = {
      var searched: Boolean = false

      {
        var p: TreeNode[K, V] = root
        var break = false
        while (!break) {
          var dir: Int = 0
          var ph: Int = 0
          var pk: K = null.asInstanceOf[K]
          if (p == null) {
            first = ({
              root = new TreeNode[K, V](this.map, h, k, v, null, null); root
            })
            onInsert(first)
            break = true //todo: break is not supported
          }
          else if ((({
            ph = p.hash; ph
          })) > h) dir = -1
          else if (ph < h) dir = 1
          else if (refEquals((({
            pk = p.key; pk
          })), k) || (pk != null && (k == pk))) return p
          else if ({
            dir = compareComparables(k, pk); dir
          } == 0) {
            if (!searched) {
              var q: TreeNode[K, V] = null
              var ch: TreeNode[K, V] = null
              searched = true
              if (((({
                ch = p.left; ch
              })) != null && (({
                q = ch.findTreeNode(h, k); q
              })) != null) || ((({
                ch = p.right; ch
              })) != null && (({
                q = ch.findTreeNode(h, k); q
              })) != null)) return q
            }
            dir = TreeBin.tieBreakOrder(k, pk)
          }
          if(!break) {
            val xp: TreeNode[K, V] = p
            if ((({
              p = if ((dir <= 0)) p.left else p.right;
              p
            })) == null) {
              var x: TreeNode[K, V] = null
              val f: TreeNode[K, V] = first
              first = ({
                x = new TreeNode[K, V](this.map, h, k, v, f, xp);
                x
              })
              onInsert(first)
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
      null
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
    final def removeTreeNode(p: TreeNode[K, V]): Boolean = {
      val next: TreeNode[K, V] = p.next.asInstanceOf[TreeNode[K, V]]
      val pred: TreeNode[K, V] = p.prev
      var r: TreeNode[K, V] = null
      var rl: TreeNode[K, V] = null
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
        var replacement: TreeNode[K, V] = null
        val pl: TreeNode[K, V] = p.left
        val pr: TreeNode[K, V] = p.right
        if (pl != null && pr != null) {
          var s: TreeNode[K, V] = pr
          var sl: TreeNode[K, V] = null
          while ((({
            sl = s.left; sl
          })) != null) s = sl
          val c: Boolean = s.red
          s.red = p.red
          p.red = c
          val sr: TreeNode[K, V] = s.right
          val pp: TreeNode[K, V] = p.parent
          if (s eq pr) {
            p.parent = s
            s.right = p
          }
          else {
            val sp: TreeNode[K, V] = s.parent
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
          val pp: TreeNode[K, V] = {replacement.parent = p.parent; replacement.parent}
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
          var pp: TreeNode[K, V] = null
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
      false
    }
  }

  /**
   * Records the table, its length, and current traversal index for a
   * traverser that must process a region of a forwarded table before
   * proceeding with current table.
   */
  final class TableStack[K, V <: Product] {
    var length: Int = 0
    var index: Int = 0
    var tab: Array[Node[K, V]] = null
    var next: TableStack[K, V] = null
  }

  /**
   * Base class for bulk tasks. Repeats some fields and code from
   * class Traverser, because we need to subclass CountedCompleter.
   */
  @SuppressWarnings(Array("serial")) abstract class BulkTask[K, V <: Product, R](val par: BulkTask[K, V, _], var batch: Int, var baseIndex: Int, f: Int, var tab: Array[Node[K, V]]) extends CountedCompleter[R](par) {

    var next: Node[K, V] = null
    var stack: TableStack[K, V] = null
    var spare: TableStack[K, V] = null
    var index: Int = baseIndex
    var baseLimit: Int = if (tab == null) 0
    else if (par == null) tab.length
    else f
    final val baseSize: Int = if (tab == null || par == null) this.baseLimit
    else par.baseSize

    /**
     * Same as Traverser version
     */
    final def advance: Node[K, V] = {
      var e: Node[K, V] = null
      if ((({
        e = next; e
      })) != null) e = e.next
      while (true) {
        var continue = false
        var t: Array[Node[K, V]] = null
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
          if (e.isInstanceOf[ForwardingNode[_, _]]) {
            tab = (e.asInstanceOf[ForwardingNode[K, V]]).nextTable
            e = null
            pushState(t, i, n)
            continue = true //todo: continue is not supported
          }
          else if (e.isInstanceOf[TreeBin[_, _]]) e = (e.asInstanceOf[TreeBin[K, V]]).first
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

    private def pushState(t: Array[Node[K, V]], i: Int, n: Int) {
      var s: TableStack[K, V] = spare
      if (s != null) spare = s.next
      else s = new TableStack[K, V]
      s.tab = t
      s.length = n
      s.index = i
      s.next = stack
      stack = s
    }

    private def recoverState(nIn: Int) {
      var n: Int = nIn
      var s: TableStack[K, V] = null
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
        val next: TableStack[K, V] = s.next
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

  @SuppressWarnings(Array("serial")) final class ForEachEntryTask[K, V <: Product](p: BulkTask[K, V, _], b: Int, i: Int, f: Int, t: Array[Node[K, V]], final val action: SEntryMVCC[K, V] => Unit) extends BulkTask[K, V, Void](p,b,i,f,t) {

    final def compute {
      val action = this.action
      if ((({
        action
      })) != null) {
        val i: Int = baseIndex
        var f: Int = 0
        var h: Int = 0
        while (batch > 0 && (({
          h = ((({
            f = baseLimit; f
          })) + i) >>> 1; h
        })) > i) {
          addToPendingCount(1)
          new ForEachEntryTask[K, V](this, {batch >>>= 1; batch}, {baseLimit = h; baseLimit}, f, tab, action).fork
        }
        var p: Node[K, V] = null
        while ((({
          p = advance; p
        })) != null) action(p)
        propagateCompletion
      }
    }
  }

  // @inline //inlining is disabled during development
  final def getModifiedColIds[V <: Product](v1: V, v2: V) = {
    var cols:List[Int]=Nil
    var i = v1.productArity - 1
    while(i >= 0) {
      if(v1.productElement(i) != v2.productElement(i)) {
        cols = i :: cols
      }
      i -= 1
    }
    cols
  }

  private val U: Unsafe = MyThreadLocalRandom.getUnsafe
  val k: Class[_] = classOf[ConcurrentSHMapMVCC[_, _]]
  private val SIZECTL: Long = U.objectFieldOffset(k.getDeclaredField("sizeCtl"))
  private val TRANSFERINDEX: Long = U.objectFieldOffset(k.getDeclaredField("transferIndex"))
  private val BASECOUNT: Long = U.objectFieldOffset(k.getDeclaredField("baseCount"))
  private val CELLSBUSY: Long = U.objectFieldOffset(k.getDeclaredField("cellsBusy"))
  val ck: Class[_] = classOf[CounterCell]
  private val CELLVALUE: Long = U.objectFieldOffset(ck.getDeclaredField("value"))
  val ak: Class[_] = classOf[Array[Node[_, _]]]
  private val ABASE: Long = U.arrayBaseOffset(ak)
  val scale: Int = U.arrayIndexScale(ak)
  if ((scale & (scale - 1)) != 0) throw new Error("data type scale not a power of two")
  private val ASHIFT: Int = 31 - Integer.numberOfLeadingZeros(scale)
}

@SerialVersionUID(7249069246763182397L)
/**
 * Creates a new, empty map with the default initial table size (16).
 */
class ConcurrentSHMapMVCC[K, V <: Product](val tbl:Table, val projs:(K,V)=>_ *)(implicit ord: math.Ordering[K]) /*extends AbstractMap[K, V] with ConcurrentMap[K, V] with Serializable*/ {
  final val NULL_VALUE = null.asInstanceOf[V]
  /**
   * The array of bins. Lazily initialized upon first insertion.
   * Size is always a power of two. Accessed directly by iterators.
   */
  @volatile
  @transient
  var table: Array[Node[K, V]] = null
  /**
   * The next table to use; non-null only while resizing.
   */
  @volatile
  @transient
  private var nextTable: Array[Node[K, V]] = null
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
  private var sizeCtl: Int = DEFAULT_CAPACITY
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
  private var counterCells: Array[CounterCell] = null

  def this(n:String)(implicit ord: math.Ordering[K]) {
    this(n,List():_*)
  }
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
  def this(n:String, initialCapacity: Int)(implicit ord: math.Ordering[K]) {
    this(n)
    if (initialCapacity < 0) throw new IllegalArgumentException
    val cap: Int = (if ((initialCapacity >= (MAXIMUM_CAPACITY >>> 1))) MAXIMUM_CAPACITY else tableSizeFor(initialCapacity + (initialCapacity >>> 1) + 1))
    this.sizeCtl = cap
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
  def this(n:String, inInitialCapacity: Int, loadFactor: Float, concurrencyLevel: Int = 1)(implicit ord: math.Ordering[K]) {
    this(n)
    var initialCapacity: Int = inInitialCapacity
    if (!(loadFactor > 0.0f) || initialCapacity < 0 || concurrencyLevel <= 0) throw new IllegalArgumentException
    if (initialCapacity < concurrencyLevel) initialCapacity = concurrencyLevel
    val size: Long = (1.0 + initialCapacity.toLong / loadFactor).toLong
    val cap: Int = if ((size >= MAXIMUM_CAPACITY.toLong)) MAXIMUM_CAPACITY else tableSizeFor(size.toInt)
    this.sizeCtl = cap
  }

  def this(n:String, loadFactor: Float, inInitialCapacity: Int, projs:(K,V)=>_ *)(implicit ord: math.Ordering[K]) {
    this(n,projs:_*)
    val concurrencyLevel = 1
    var initialCapacity: Int = inInitialCapacity
    if (!(loadFactor > 0.0f) || initialCapacity < 0 || concurrencyLevel <= 0) throw new IllegalArgumentException
    if (initialCapacity < concurrencyLevel) initialCapacity = concurrencyLevel
    val size: Long = (1.0 + initialCapacity.toLong / loadFactor).toLong
    val cap: Int = if ((size >= MAXIMUM_CAPACITY.toLong)) MAXIMUM_CAPACITY else tableSizeFor(size.toInt)
    this.sizeCtl = cap
  }

  /**
   * {@inheritDoc}
   */
  def size: Int = {
    val n: Long = sumCount
    (if ((n < 0L)) 0 else if ((n > Integer.MAX_VALUE.toLong)) Integer.MAX_VALUE else n.toInt)
  }

  /**
   * {@inheritDoc}
   */
  def isEmpty: Boolean = {
    sumCount <= 0L
  }

  // def get(key: Any): V = {
  //   throw new UnsupportedOperationException()
  // }
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
  // @inline //inlining is disabled during development
  final def get(key: K)(implicit xact:Transaction): V = apply(key)
  // @inline //inlining is disabled during development
  final def apply(key: K)(implicit xact:Transaction): V = {
    // debug("finding " + key)
    xact.addPredicate(Predicate(tbl, GetPredicateOp(key)))
    var tab: Array[Node[K, V]] = null
    var e: Node[K, V] = null
    var p: Node[K, V] = null
    var n: Int = 0
    var eh: Int = 0
    var ek: K = null.asInstanceOf[K]
    val h: Int = spread(key.hashCode)
    if ((({
      tab = table; tab
    })) != null && (({
      n = tab.length; n
    })) > 0 && (({
      e = tabAt(tab, (n - 1) & h); e
    })) != null) {
      if ((({
        eh = e.hash; eh
      })) == h) {
        if (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek))) {
          // debug("\t found 1")
          return e.getValueImage
        }
      }
      else if (eh < 0) return if ((({
        p = e.find(h, key); p
      })) != null) {
        if(p == null) {
          // debug("\t found 2")
          NULL_VALUE
        } else {
          // debug("\t found 3")
          p.getValueImage
        }
      } else {
        // debug("\t found 4")
        NULL_VALUE
      }
      while ((({
        e = e.next; e
      })) != null) {
        if (e.hash == h && (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek)))) return if(e == null) {
        // debug("\t found 5")
          NULL_VALUE
        } else {
        // debug("\t found 6")
          e.getValueImage
        }
      }
    }
    // debug("\t not found 7")
    NULL_VALUE
  }
  def getEntry(key: K)(implicit xact:Transaction): DeltaVersion[K,V] = {
    xact.addPredicate(Predicate(tbl, GetPredicateOp(key)))
    // debug("finding entry for " + key)
    var tab: Array[Node[K, V]] = null
    var e: Node[K, V] = null
    var p: Node[K, V] = null
    var n: Int = 0
    var eh: Int = 0
    var ek: K = null.asInstanceOf[K]
    val h: Int = spread(key.hashCode)
    if ((({
      tab = table; tab
    })) != null && (({
      n = tab.length; n
    })) > 0 && (({
      e = tabAt(tab, (n - 1) & h); e
    })) != null) {
      if ((({
        eh = e.hash; eh
      })) == h) {
        if (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek))) {
          // debug("\t found case 1 => " + Integer.toHexString(System.identityHashCode(e)))
          return e.getTheValue
        }
      }
      else if (eh < 0) return if ((({
        p = e.find(h, key); p
      })) != null) {
        // debug("\t found case 2 => " + Integer.toHexString(System.identityHashCode(p)))
        p.getTheValue
      } else {
        // debug("\t not found case 3 ")
        null
      }

      while ((({
        e = e.next; e
      })) != null) {
        if (e.hash == h && (refEquals((({
          ek = e.key; ek
        })), key) || (ek != null && (key == ek)))) {
          // debug("\t found case 4 => " + Integer.toHexString(System.identityHashCode(e)))
          return e.getTheValue
        }
      }
    }
    // debug("\t not found case 5 ")
    null
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
  def put(key: K, value: V)(implicit xact:Transaction): Unit = {
    putVal(key, value, false)
  }
  def +=(key: K, value: V)(implicit xact:Transaction): Unit = {
    putVal(key, value, true)
  }

  def update(key: K, value: V)(implicit xact:Transaction): Unit = {
    putVal(key, value, false)
  }

  def update(key: K, updateFunc:V=>V)(implicit xact:Transaction): Unit = {
    //TODO: FIX IT
    putVal(key, updateFunc(get(key)), false)
  }

  /** Implementation for put and putIfAbsent */
  final def putVal(key: K, value: V, onlyIfAbsent: Boolean)(implicit xact:Transaction): Unit = {
    var isAdded = false
    var oldVal: DeltaVersion[K,V] = null
    var oldValImg: V = NULL_VALUE
    var entry: Node[K, V] = null

    if (key == null || value == null) throw new NullPointerException
    val hash: Int = spread(key.hashCode)
    var binCount: Int = 0

    var tab: Array[Node[K, V]] = table
    var break = false
    while (!break) {
      var f: Node[K, V] = null
      var n: Int = 0
      var i: Int = 0
      var fh: Int = 0
      if ((tab == null) || ({n = tab.length; n } == 0)) {
        tab = initTable
      } else if ({f = tabAt(tab, {i = ((n - 1) & hash); i}); f} == null) {
        entry = new Node[K, V](this, hash, key, value, null)
        if (casTabAt(tab, i, null, entry)) {
          isAdded = true
          break = true //todo: break is not supported
        }
      } else if ({fh = f.hash; fh} == MOVED) {
        tab = helpTransfer(tab, f)
      } else {
        f synchronized {
          if (tabAt(tab, i) eq f) {
            if (fh >= 0) {
              binCount = 1

              var e: Node[K, V] = f
              break = false
              while (!break) {
                var ek: K = null.asInstanceOf[K]
                if (e.hash == hash && (refEquals({ek = e.key; ek}, key) || ((ek != null) && (key == ek)))) {
                  entry = e
                  oldVal = e.getTheValue
                  if(oldVal == null || oldVal.isDeleted) {
                    e.setTheValue(value, INSERT_OP)
                  } else if (!onlyIfAbsent) {
                    oldValImg = oldVal.getImage
                    e.setTheValue(value, UPDATE_OP, getModifiedColIds(value, oldValImg))
                  }
                  break = true //todo: break is not supported
                }
                if(!break) {
                  val pred: Node[K, V] = e
                  if ({e = e.next; e} == null) {
                    entry = new Node[K, V](this, hash, key, value, null)
                    pred.next = entry
                    isAdded = true
                    break = true //todo: break is not supported
                  }
                  if(!break) binCount += 1
                }
              }
              break = false
            } else if (f.isInstanceOf[TreeBin[_, _]]) {
              var p: Node[K, V] = null
              binCount = 2
              p = (f.asInstanceOf[TreeBin[K, V]]).putTreeVal(hash, key, value, { insertedEntry =>
                isAdded = true
                entry = insertedEntry
              })
              if (p != null) {
                entry = p
                oldVal = p.getTheValue
                if(oldVal == null || oldVal.isDeleted) {
                  p.setTheValue(value, INSERT_OP)
                } else if (!onlyIfAbsent) {
                  oldValImg = oldVal.getImage
                  p.setTheValue(value, UPDATE_OP, getModifiedColIds(value, oldValImg))
                }
              }
            }
          }
        }
        if (binCount != 0) {
          if (binCount >= TREEIFY_THRESHOLD) {
            treeifyBin(tab, i)
          }
          // if (oldVal != null)
          //   return oldVal;
          break = true //todo: break is not supported
        }
      }
    }

    if(isAdded) addCount(1L, binCount)

    if(oldVal == null) {
      if (idxs != Nil) idxs.foreach(_.set(entry.getTheValue))
    } else {
      if(onlyIfAbsent && !oldVal.isDeleted) {
        //We ensure the uniqueness of primary keys by aborting a
        //transaction that inserts a primary key that exists either
        // (i) in the snapshot that is visible to the transaction,
        // (ii) in the last committed version of the key’s record, or
        // (iii) uncommitted as an insert in an undo buffer
        throw new MVCCRecordAlreayExistsException("The record (%s -> %s) already exists (written by %s). Could not insert the new value (%s) from %s".format(oldVal.getKey, oldVal.getImage, oldVal.vXact, value, xact))
      }

      if (idxs != Nil) idxs.foreach{ idx => 
        // val pOld = idx.proj(key,oldValImg)
        // val pNew = idx.proj(key,value)
        // if(pNew != pOld) {
          // idx.del(oldVal, oldValImg) // no value image gets deleted from the indices,
                                        // before it becomes invisible to all the transactions
                                        // If an update up- dates only non-indexed attributes,
                                        // updates are performed as usual. If an update updates
                                        // an indexed attribute, the record is deleted and re-inserted
                                        // into the relation and both, the deleted and the re-inserted
                                        // record, are stored in the in- dex. Thus, indexes retain
                                        // references to all records that are visible by any active
                                        // transaction. Just like undo buffers, indexes are cleaned up
                                        // during garbage collection.
          // we need to always index both the old and new value,
          // as the old value will become invisible to the future xacts
          idx.set(entry.getTheValue)
        // }
      }
    }
    // oldVal
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
  // @inline //inlining is disabled during development
  final def remove(key: K)(implicit xact:Transaction): Unit = {
    -=(key)
  }
  // @inline //inlining is disabled during development
  final def -=(key: K)(implicit xact:Transaction): Unit = {
    // debug("- started deleting " + key)
    val dv = getEntry(key)
    if(dv ne null) {
      dv.setEntryValue(NULL_VALUE)
    }
    // debug("- finished deleting " + key)
    // replaceNode(key)
  }

  /**
   * Implementation for the four public remove/replace methods:
   * Replaces node value with v, conditional upon match of cv if
   * non-null.  If resulting value is null, delete.
   */
  final def replaceNode(key: K): Unit = {
    val hash: Int = spread(key.hashCode)

    {
      var tab: Array[Node[K, V]] = table
      var break = false
      while (!break) {
        var f: Node[K, V] = null
        var n, i , fh = 0
        if ((tab == null) ||
            ({ n = tab.length; n } == 0) ||
            ({ f = tabAt(tab, {i = (n - 1) & hash; i}); f } == null)) {
          break = true //todo: break is not supported
        } else if ({ fh = f.hash; fh } == MOVED) {
          tab = helpTransfer(tab, f)
        } else {
          var validated: Boolean = false
          f synchronized {
            if (tabAt(tab, i) eq f) {
              if (fh >= 0) {
                validated = true

                {
                  var e: Node[K, V] = f
                  var pred: Node[K, V] = null
                  break = false;
                  while (!break) {
                    var ek: K = null.asInstanceOf[K]
                    if (e.hash == hash &&
                        (refEquals({ ek = e.key; ek }, key) ||
                          (ek != null && (key == ek)))) {
                      if (pred != null) pred.next = e.next
                      else setTabAt(tab, i, e.next)

                      // if (idxs!=Nil) idxs.foreach(_.del(e.getTheValue)) // Just like undo buffers, indexes are cleaned up
                                                                           // during garbage collection.
                      break = true
                    }
                    if(!break) {
                      pred = e
                      if ({ e = e.next; e } == null) break = true
                    }
                  }
                  break = false
                }
              } else if (f.isInstanceOf[TreeBin[_, _]]) {
                validated = true
                val t: TreeBin[K, V] = f.asInstanceOf[TreeBin[K, V]]
                var r: TreeNode[K, V] = null
                var p: TreeNode[K, V] = null
                if ({ r = t.root; r } != null && { p = r.findTreeNode(hash, key); p } != null) {
                  if (t.removeTreeNode(p)) setTabAt(tab, i, untreeify(t.first))

                  // if (idxs!=Nil) idxs.foreach(_.del(p.getTheValue)) // Just like undo buffers, indexes are cleaned up
                                                                       // during garbage collection.
                }
              }
            }
          }
          if (validated) {
            addCount(-1L, -1) //nothing is removed here, and the removal is postponed to commit time
                                 //TODO: FIX IT this operation should be done in a later point
            break = true //todo: break is not supported
          }
        }
      }
    }
  }

  /**
   * Removes all of the mappings from this map.
   */
  // def clear(implicit xact:Transaction) {
  //   var delta: Long = 0L
  //   var i: Int = 0
  //   var tab: Array[Node[K, V]] = table
  //   while (tab != null && i < tab.length) {
  //     var fh: Int = 0
  //     val f: Node[K, V] = tabAt(tab, i)
  //     if (f == null) ({
  //       i += 1; i
  //     })
  //     else if ((({
  //       fh = f.hash; fh
  //     })) == MOVED) {
  //       tab = helpTransfer(tab, f)
  //       i = 0
  //     }
  //     else {
  //       f synchronized {
  //         if (tabAt(tab, i) eq f) {
  //           var p: Node[K, V] = (if (fh >= 0) f else if ((f.isInstanceOf[TreeBin[_, _]])) (f.asInstanceOf[TreeBin[K, V]]).first else null)
  //           while (p != null) {
  //             delta -= 1
  //             p = p.next
  //           }
  //           setTabAt(tab, ({
  //             i += 1; i - 1
  //           }), null)
  //         }
  //       }
  //     }
  //   }
  //   if (delta != 0L) addCount(delta, -1)
  //   if (idxs!=Nil) idxs.foreach(_.clear)
  // }

  /**
   * Initializes table, using the size recorded in sizeCtl.
   */
  private final def initTable: Array[Node[K, V]] = {
    var tab: Array[Node[K, V]] = null
    var sc: Int = 0
    var break = false
    while (!break && ((({
      tab = table; tab
    })) == null || tab.length == 0)) {
      if ((({
        sc = sizeCtl; sc
      })) < 0) Thread.`yield`
      else if (U.compareAndSwapInt(this, SIZECTL, sc, -1)) {
        try {
          if ((({
            tab = table; tab
          })) == null || tab.length == 0) {
            val n: Int = if ((sc > 0)) sc else DEFAULT_CAPACITY
            @SuppressWarnings(Array("unchecked")) val nt: Array[Node[K, V]] = new Array[Node[_, _]](n).asInstanceOf[Array[Node[K, V]]]
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
    var as: Array[CounterCell] = null
    var b: Long = 0L
    var s: Long = 0L
    if ((({
      as = counterCells; as
    })) != null || !U.compareAndSwapLong(this, BASECOUNT, {b = baseCount; b}, {s = b + x; s})) {
      var a: CounterCell = null
      var v: Long = 0L
      var m: Int = 0
      var uncontended: Boolean = true
      if (as == null || (({
        m = as.length - 1; m
      })) < 0 || (({
        a = as(MyThreadLocalRandom.getProbe & m); a
      })) == null || !(({
        uncontended = U.compareAndSwapLong(a, CELLVALUE, {v = a.value; v}, v + x); uncontended
      }))) {
        fullAddCount(x, uncontended)
        return
      }
      if (check <= 1) return
      s = sumCount
    }
    if (check >= 0) {
      var tab: Array[Node[K, V]] = null
      var nt: Array[Node[K, V]] = null
      var n: Int = 0
      var sc: Int = 0
      var break = false
      while (!break && (s >= (({
        sc = sizeCtl; sc
      })).toLong && (({
        tab = table; tab
      })) != null && (({
        n = tab.length; n
      })) < MAXIMUM_CAPACITY)) {
        val rs: Int = resizeStamp(n)
        if (sc < 0) {
          if (((sc >>> RESIZE_STAMP_SHIFT) != rs) || (sc == rs + 1) || (sc == rs + MAX_RESIZERS) || (({
            nt = nextTable; nt
          })) == null || transferIndex <= 0) break = true //todo: break is not supported
          if (!break && U.compareAndSwapInt(this, SIZECTL, sc, sc + 1)) transfer(tab, nt)
        }
        else if (U.compareAndSwapInt(this, SIZECTL, sc, (rs << RESIZE_STAMP_SHIFT) + 2)) transfer(tab, null)
        if(!break) s = sumCount
      }
    }
  }

  /**
   * Helps transfer if a resize is in progress.
   */
  final def helpTransfer(tab: Array[Node[K, V]], f: Node[K, V]): Array[Node[K, V]] = {
    var nextTab: Array[Node[K, V]] = null
    var sc: Int = 0
    if (tab != null && (f.isInstanceOf[ForwardingNode[_, _]]) && (({
      nextTab = (f.asInstanceOf[ForwardingNode[K, V]]).nextTable; nextTab
    })) != null) {
      val rs: Int = resizeStamp(tab.length)
      var break = false
      while (!break && ((nextTab eq nextTable) && (table eq tab) && (({
        sc = sizeCtl; sc
      })) < 0)) {
        if ((sc >>> RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 || sc == rs + MAX_RESIZERS || transferIndex <= 0) break = true //todo: break is not supported
        if (!break && U.compareAndSwapInt(this, SIZECTL, sc, sc + 1)) {
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
    val c: Int = if ((size >= (MAXIMUM_CAPACITY >>> 1))) MAXIMUM_CAPACITY else tableSizeFor(size + (size >>> 1) + 1)
    var sc: Int = 0
    var break = false
    while (!break && ((({
      sc = sizeCtl; sc
    })) >= 0)) {
      val tab: Array[Node[K, V]] = table
      var n: Int = 0
      if (tab == null || (({
        n = tab.length; n
      })) == 0) {
        n = if ((sc > c)) sc else c
        if (U.compareAndSwapInt(this, SIZECTL, sc, -1)) {
          try {
            if (table eq tab) {
              @SuppressWarnings(Array("unchecked")) val nt: Array[Node[K, V]] = new Array[Node[_, _]](n).asInstanceOf[Array[Node[K, V]]]
              table = nt
              sc = n - (n >>> 2)
            }
          } finally {
            sizeCtl = sc
          }
        }
      }
      else if (c <= sc || n >= MAXIMUM_CAPACITY) break = true //todo: break is not supported
      else if (tab eq table) {
        val rs: Int = resizeStamp(n)
        if (sc < 0) {
          var nt: Array[Node[K, V]] = null
          if ((sc >>> RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 || sc == rs + MAX_RESIZERS || (({
            nt = nextTable; nt
          })) == null || transferIndex <= 0) break = true //todo: break is not supported
          if (!break && U.compareAndSwapInt(this, SIZECTL, sc, sc + 1)) transfer(tab, nt)
        }
        else if (U.compareAndSwapInt(this, SIZECTL, sc, (rs << RESIZE_STAMP_SHIFT) + 2)) transfer(tab, null)
      }
    }
  }

  /**
   * Moves and/or copies the nodes in each bin to new table. See
   * above for explanation.
   */
  private final def transfer(tab: Array[Node[K, V]], inNextTab: Array[Node[K, V]]) {
    var nextTab: Array[Node[K, V]] = inNextTab
    val n: Int = tab.length
    var stride: Int = 0
    if ((({
      stride = if ((NCPU > 1)) (n >>> 3) / NCPU else n; stride
    })) < MIN_TRANSFER_STRIDE) stride = MIN_TRANSFER_STRIDE
    if (nextTab == null) {
      try {
        @SuppressWarnings(Array("unchecked")) val nt: Array[Node[K, V]] = new Array[Node[_, _]](n << 1).asInstanceOf[Array[Node[K, V]]]
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
    val fwd: ForwardingNode[K, V] = new ForwardingNode[K, V](nextTab)
    var advance: Boolean = true
    var finishing: Boolean = false

    {
      var i: Int = 0
      var bound: Int = 0
      while (true) {
        var f: Node[K, V] = null
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
          else if (U.compareAndSwapInt(this, TRANSFERINDEX, nextIndex, { nextBound = (if (nextIndex > stride) nextIndex - stride else 0); nextBound })) {
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
          if (U.compareAndSwapInt(this, SIZECTL, {sc = sizeCtl; sc}, sc - 1)) {
            if ((sc - 2) != (resizeStamp(n) << RESIZE_STAMP_SHIFT)) return
            finishing = ({
              advance = true; advance
            })
            i = n
          }
        }
        else if ((({
          f = tabAt(tab, i); f
        })) == null) advance = casTabAt(tab, i, null, fwd)
        else if ((({
          fh = f.hash; fh
        })) == MOVED) advance = true
        else {
          f synchronized {
            if (tabAt(tab, i) eq f) {
              var ln: Node[K, V] = null
              var hn: Node[K, V] = null
              if (fh >= 0) {
                var runBit: Int = fh & n
                var lastRun: Node[K, V] = f

                {
                  var p: Node[K, V] = f.next
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
                  var p: Node[K, V] = f
                  while (p ne lastRun) {
                    {
                      val ph: Int = p.hash
                      val pk: K = p.key
                      val pv: DeltaVersion[K,V] = p.value
                      if ((ph & n) == 0) ln = new Node[K, V](this, ph, pk, pv, ln)
                      else hn = new Node[K, V](this, ph, pk, pv, hn)
                    }
                    p = p.next
                  }
                }
                setTabAt(nextTab, i, ln)
                setTabAt(nextTab, i + n, hn)
                setTabAt(tab, i, fwd)
                advance = true
              }
              else if (f.isInstanceOf[TreeBin[_, _]]) {
                val t: TreeBin[K, V] = f.asInstanceOf[TreeBin[K, V]]
                var lo: TreeNode[K, V] = null
                var loTail: TreeNode[K, V] = null
                var hi: TreeNode[K, V] = null
                var hiTail: TreeNode[K, V] = null
                var lc: Int = 0
                var hc: Int = 0

                {
                  var e: Node[K, V] = t.first
                  while (e != null) {
                    {
                      val h: Int = e.hash
                      val p: TreeNode[K, V] = new TreeNode[K, V](this, h, e.key, e.value, null, null)
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
                ln = if ((lc <= UNTREEIFY_THRESHOLD)) untreeify(lo) else if ((hc != 0)) new TreeBin[K, V](lo,this) else t
                hn = if ((hc <= UNTREEIFY_THRESHOLD)) untreeify(hi) else if ((lc != 0)) new TreeBin[K, V](hi,this) else t
                setTabAt(nextTab, i, ln)
                setTabAt(nextTab, i + n, hn)
                setTabAt(tab, i, fwd)
                advance = true
              }
            }
          }
        }
      }
    }
  }

  final def sumCount: Long = {
    val as: Array[CounterCell] = counterCells
    var a: CounterCell = null
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
    sum
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
      var as: Array[CounterCell] = null
      var a: CounterCell = null
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
            val r: CounterCell = new CounterCell(x)
            if (cellsBusy == 0 && U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
              var created: Boolean = false
              try {
                var rs: Array[CounterCell] = null
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
        else if (U.compareAndSwapLong(a, CELLVALUE, {v = a.value; v}, v + x)) break = true //todo: break is not supported
        else if ((counterCells ne as) || (n >= NCPU)) collide = false
        else if (!collide) collide = true
        else if (cellsBusy == 0 && U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
          try {
            if (counterCells eq as) {
              val rs: Array[CounterCell] = new Array[CounterCell](n << 1)

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
      else if ((cellsBusy == 0) && (counterCells eq as) && U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
        var init: Boolean = false
        try {
          if (counterCells eq as) {
            val rs: Array[CounterCell] = new Array[CounterCell](2)
            rs(h & 1) = new CounterCell(x)
            counterCells = rs
            init = true
          }
        } finally {
          cellsBusy = 0
        }
        if (init) break = true //todo: break is not supported
      }
      else if (U.compareAndSwapLong(this, BASECOUNT, {v = baseCount; v}, v + x)) break = true //todo: break is not supported
    }
  }

  /**
   * Replaces all linked nodes in bin at given index unless table is
   * too small, in which case resizes instead.
   */
  private final def treeifyBin(tab: Array[Node[K, V]], index: Int) {
    var b: Node[K, V] = null
    var n: Int = 0
    val sc: Int = 0
    if (tab != null) {
      if ((({
        n = tab.length; n
      })) < MIN_TREEIFY_CAPACITY) tryPresize(n << 1)
      else if ((({
        b = tabAt(tab, index); b
      })) != null && b.hash >= 0) {
        b synchronized {
          if (tabAt(tab, index) eq b) {
            var hd: TreeNode[K, V] = null
            var tl: TreeNode[K, V] = null

            {
              var e: Node[K, V] = b
              while (e != null) {
                {
                  val p: TreeNode[K, V] = new TreeNode[K, V](this, e.hash, e.key, e.value, null, null)
                  if ((({
                    p.prev = tl; p.prev
                  })) == null) hd = p
                  else tl.next = p
                  tl = p
                }
                e = e.next
              }
            }
            setTabAt(tab, index, new TreeBin[K, V](hd, this))
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
  // @inline //inlining is disabled during development
  final def batchFor(b: Long): Int = {
    var n: Long = 0L
    if (b == Long.MaxValue || (({
      n = sumCount; n
    })) <= 1L || n < b) return 0
    val sp: Int = ForkJoinPool.getCommonPoolParallelism << 2
    if ((b <= 0L || (({
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
  // @inline //inlining is disabled during development
  final private def forEach(parallelismThreshold: Long, action: (K,V) => Unit)(implicit xact:Transaction) {
    xact.addPredicate(Predicate(tbl, ForeachPredicateOp()))
    if (action == null) throw new NullPointerException
    new ForEachEntryTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, { e => 
      val dv = e.getTheValue
      if(dv ne null) {
        val img = dv.getImage
        if(img != null) action(e.key, dv.getImage)
      }
    }).invoke
  }
  // @inline //inlining is disabled during development
  final def foreach(action: (K,V) => Unit)(implicit xact:Transaction) {
    forEach(ONE_THREAD_NO_PARALLELISM, action)
  }

  /**
   * Performs the given action for each key.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  // @inline //inlining is disabled during development
  final private def forEachKey(parallelismThreshold: Long, action: K => Unit)(implicit xact:Transaction)  {
    xact.addPredicate(Predicate(tbl, ForeachPredicateOp()))
    if (action == null) throw new NullPointerException
    new ForEachEntryTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, { e =>
      val dv = e.getTheValue
      if((dv ne null) && (dv.getImage != null)) action(e.key)
    }).invoke
  }
  // @inline //inlining is disabled during development
  final def foreachKey(action: K => Unit)(implicit xact:Transaction)  {
    forEachKey(ONE_THREAD_NO_PARALLELISM, action)
  }

  /**
   * Performs the given action for each entry.
   *
   * @param parallelismThreshold the (estimated) number of elements
   *                             needed for this operation to be executed in parallel
   * @param action the action
   * @since 1.8
   */
  // @inline //inlining is disabled during development
  final private def forEachEntry(parallelismThreshold: Long, action: SEntryMVCC[K, V] => Unit)(implicit xact:Transaction)  {
    xact.addPredicate(Predicate(tbl, ForeachPredicateOp()))
    if (action == null) throw new NullPointerException
    new ForEachEntryTask[K, V](null, batchFor(parallelismThreshold), 0, 0, table, { e =>
      val dv = e.getTheValue
      if((dv ne null) && (dv.getImage != null)) action(e)
    }).invoke
  }

  // @inline //inlining is disabled during development
  final def foreachEntry(action: SEntryMVCC[K, V] => Unit)(implicit xact:Transaction) {
    forEachEntry(ONE_THREAD_NO_PARALLELISM, action)
  }

  val idxs:List[ConcurrentSHIndexMVCC[_,K,V]] = {
    def idx[P](f:(K,V)=>P, lf:Float, initC:Int) = new ConcurrentSHIndexMVCC[P,K,V](f,lf,initC)(new Ordering[P] {
      def compare(x: P, y: P): Int = if(x == y) 0 else x.hashCode compare y.hashCode
    },ord)
    //TODO: pass the correct capacity and load factor if necessary
    // projs.zip(lfInitIndex).toList.map{case (p, (lf, initC)) => idx(p , lf, initC)}
    projs.toList.map{ p => idx(p , LOAD_FACTOR, DEFAULT_CAPACITY)}
  }

  def slice[P](part:Int, partKey:P)(implicit xact:Transaction):ConcurrentSHIndexMVCCEntry[P,K,V] = {
    xact.addPredicate(Predicate(tbl, SlicePredicateOp(part, partKey)))
    val ix=idxs(part)
    ix.asInstanceOf[ConcurrentSHIndexMVCC[P,K,V]].slice(partKey) // type information P is erased anyway
  }
}
