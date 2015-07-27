package ddbt.tpcc.lib.util

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