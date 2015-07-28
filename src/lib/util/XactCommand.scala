package ddbt.lib.util

/**
 * This class is the parent of all transactional command classes
 * Each transaction can be compacted to the name of the transaction
 * (with points to the transaction program) alongside with the
 * arguments to that comman
 */
abstract class XactCommand