package ddbt.lib.mvc3t

class MVCCException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends RuntimeException(message, cause) {
	// xact.rollback //rollback the transaction upon any exception
}
class MVCCConcurrentWriteException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause) {
	xact.tm.failedConcurrentUpdate += 1
}
class MVCCRecordAlreadyExistsException(message: String = null, cause: Throwable = null)(implicit xact:Transaction) extends MVCCException(message, cause) {
	xact.tm.failedConcurrentInsert += 1
}