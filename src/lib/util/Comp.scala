package ddbt.lib.util

object Comp {
  @inline
  def refEquals[A](x: A, y: A)(implicit eq: Equiv[A]) = {
    eq.equiv(x, y)
  }

  @inline
  implicit def anyRefHasRefEquality[A <: AnyRef] = new Equiv[A] {
    def equiv(x: A, y: A) = x eq y
  }

  @inline
  implicit def anyValHasUserEquality[A <: AnyVal] = new Equiv[A] {
    def equiv(x: A, y: A) = x == y
  }
}