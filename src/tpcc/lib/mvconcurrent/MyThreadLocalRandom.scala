package ddbt.tpcc.lib.mvconcurrent

import java.lang.reflect.Field
import java.util.concurrent.atomic.{AtomicLong, AtomicInteger}

import sun.misc.Unsafe
import sun.security.action.GetPropertyAction

object MyThreadLocalRandom {
  @SuppressWarnings(Array("restriction")) def getUnsafe: Unsafe = {
    try {
      val singleoneInstanceField: Field = classOf[Unsafe].getDeclaredField("theUnsafe")
      singleoneInstanceField.setAccessible(true)
      return singleoneInstanceField.get(null).asInstanceOf[Unsafe]
    }
    catch {
      case e: IllegalArgumentException => {
        throw new RuntimeException(e)
      }
      case e: SecurityException => {
        throw new RuntimeException(e)
      }
      case e: NoSuchFieldException => {
        throw new RuntimeException(e)
      }
      case e: IllegalAccessException => {
        throw new RuntimeException(e)
      }
    }
  }

  /** Generates per-thread initialization/probe field */
  private val probeGenerator: AtomicInteger = new AtomicInteger
  /**
   * The next seed for default constructors.
   */
  private val seeder: AtomicLong = new AtomicLong(initialSeed)

  private def initialSeed: Long = {
    val pp: String = java.security.AccessController.doPrivileged(new GetPropertyAction("java.util.secureRandomSeed"))
    if (pp != null && pp.equalsIgnoreCase("true")) {
      val seedBytes: Array[Byte] = java.security.SecureRandom.getSeed(8)
      var s: Long = (seedBytes(0)).toLong & 0xffL

      {
        var i: Int = 1
        while (i < 8) {
          s = (s << 8) | ((seedBytes(i)).toLong & 0xffL)
          ({
            i += 1; i
          })
        }
      }
      return s
    }
    return (mix64(System.currentTimeMillis) ^ mix64(System.nanoTime))
  }

  /**
   * Returns the probe value for the current thread without forcing
   * initialization. Note that invoking ThreadLocalRandom.current()
   * can be used to force initialization on zero return.
   */
  def getProbe: Int = {
    return UNSAFE.getInt(Thread.currentThread, PROBE)
  }

  /**
   * Pseudo-randomly advances and records the given probe value for the
   * given thread.
   */
  def advanceProbe(probeIn: Int): Int = {
    var probe = probeIn ^ (probeIn << 13)
    probe ^= probe >>> 17
    probe ^= probe << 5
    UNSAFE.putInt(Thread.currentThread, PROBE, probe)
    return probe
  }

  /**
   * Initialize Thread fields for the current thread.  Called only
   * when Thread.threadLocalRandomProbe is zero, indicating that a
   * thread local seed value needs to be generated. Note that even
   * though the initialization is purely thread-local, we need to
   * rely on (static) atomic generators to initialize the values.
   */
  def localInit {
    val p: Int = probeGenerator.addAndGet(PROBE_INCREMENT)
    val probe: Int = if ((p == 0)) 1 else p
    val seed: Long = mix64(seeder.getAndAdd(SEEDER_INCREMENT))
    val t: Thread = Thread.currentThread
    UNSAFE.putLong(t, SEED, seed)
    UNSAFE.putInt(t, PROBE, probe)
  }

  private def mix64(zIn: Long): Long = {
    var z = (zIn ^ (zIn >>> 33)) * 0xff51afd7ed558ccdL
    z = (z ^ (z >>> 33)) * 0xc4ceb9fe1a85ec53L
    return z ^ (z >>> 33)
  }

  /**
   * The increment for generating probe values
   */
  private val PROBE_INCREMENT: Int = 0x9e3779b9
  /**
   * The increment of seeder per new instance
   */
  private val SEEDER_INCREMENT: Long = 0xbb67ae8584caa73bL
  private val UNSAFE: Unsafe = getUnsafe
  val tk: Class[_] = classOf[Thread]
  private val SEED: Long = UNSAFE.objectFieldOffset(tk.getDeclaredField("threadLocalRandomSeed"))
  private val PROBE: Long = UNSAFE.objectFieldOffset(tk.getDeclaredField("threadLocalRandomProbe"))
}
