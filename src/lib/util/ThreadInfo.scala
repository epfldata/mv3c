package ddbt.lib.util

import java.util.concurrent.atomic.AtomicReference

class ThreadInfo(val tid:Int) {
	var currentXact:AtomicReference[AnyRef] = null
}
