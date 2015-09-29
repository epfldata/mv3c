package ddbt.lib.util

object Comp {
  // @inline //inlining is disabled during development
  def refEquals[A](x: A, y: A)(implicit eq: Equiv[A]) = {
    eq.equiv(x, y)
  }

  // @inline //inlining is disabled during development
  implicit def anyRefHasRefEquality[A <: AnyRef] = new Equiv[A] {
    def equiv(x: A, y: A) = x eq y
  }

  // @inline //inlining is disabled during development
  implicit def anyValHasUserEquality[A <: AnyVal] = new Equiv[A] {
    def equiv(x: A, y: A) = x == y
  }
}