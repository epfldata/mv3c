package ddbt.lib.util

import java.util.concurrent.atomic.AtomicReference
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
class BackOffAtomicReference[T](init:T) {

    private[this] val value:AtomicReference[T] = new AtomicReference[T](init);

    def get:T = value.get()

    def compareAndSet(current:T, next:T) =
        if (value.compareAndSet(current, next)) true
        else {
            LockSupport.parkNanos(1)
            false
        }

}
