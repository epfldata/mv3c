package ddbt.lib.util

import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.locks.LockSupport

/**
 * The code for implementing back-off contention management
 * is fairly simple. Instead of looping over failed
 * compare-and-swaps, it backs off for a very short period
 * letting other threads try with their updates.
 * 
 * More info: "Lightweight Contention Management for Efficient
 * Compare-and-Swap Operations" http://arxiv.org/abs/1305.5800
 */
class BackOffAtomicLong(init:Long) {

    private[this] val value:AtomicLong = new AtomicLong(init);

    def get:Long = value.get()

    def getAndIncrement:Long = {

        while (true) {

            val current = get;

            val next = current + 1;

            if (compareAndSet(current, next)) return next

        }

        0L

    }

    def compareAndSet(current:Long, next:Long) =
        if (value.compareAndSet(current, next)) true
        else {
            LockSupport.parkNanos(1)
            false
        }

}
