package ddbt.tpcc.loadtest

import ddbt.lib.util.ThreadInfo
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.sql.Connection
import java.sql.DriverManager
import java.sql.SQLException
import java.text.DecimalFormat
import java.util.Map
import java.util.Properties
import java.util.Set
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import TpccThread._
import scala.collection.JavaConversions._
import ddbt.tpcc.itx._

object TpccThread {

  private val logger = LoggerFactory.getLogger(classOf[TpccThread])

  private val DEBUG = logger.isDebugEnabled
}

class TpccThread(val tInfo: ThreadInfo, 
    val port: Int, 
    val is_local: Int, 
    val db_user: String, 
    val db_password: String, 
    val num_ware: Int, 
    val num_conn: Int, 
    val driverClassName: String, 
    val jdbcUrl: String, 
    val fetchSize: Int, 
    val TRANSACTION_COUNT: Int, 
    var conn: Connection,
    val newOrder: INewOrder,
    val payment: IPayment,
    val orderStat: IOrderStatus,
    val slev: IStockLevel,
    val delivery: IDelivery,
    loopConditionChecker: => Boolean,
    val maximumNumberOfTransactionsToExecute:Int = 0) extends Thread /*with DatabaseConnector*/{

  /**
   * Dedicated JDBC connection for this thread.
   */
  // var conn: Connection = connectToDatabase

  var driver = new TpccDriver(conn, fetchSize, TRANSACTION_COUNT, newOrder, payment, orderStat, slev, delivery)

  override def run() {
    try {
      if (DEBUG) {
        logger.debug("Starting driver with: tInfo: " + tInfo + " num_ware: " + 
          num_ware + 
          " num_conn: " + 
          num_conn)
      }
      driver.runAllTransactions(tInfo, num_ware, num_conn, loopConditionChecker, maximumNumberOfTransactionsToExecute)
    } catch {
      case e: Throwable => logger.error("Unhandled exception", e)
    }
  }

  // private def connectToDatabase:Connection = connectToDB(driverClassName, jdbcUrl, db_user, db_password)
}
