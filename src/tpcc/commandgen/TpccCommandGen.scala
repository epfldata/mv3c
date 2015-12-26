package ddbt.tpcc.loadtest

import ddbt.lib.util.ThreadInfo
import java.io.FileInputStream
import java.io.InputStream
import java.text.DecimalFormat
import java.util.{Date, Properties}
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import ddbt.tpcc.tx.TpccTable
import TpccCommandGen._
import ddbt.tpcc.mtx._
import ddbt.tpcc.itx._
import TpccConstants._
import TpccTable._

object TpccCommandGen {

  private val NUMBER_OF_TX_TESTS = 100

  private val logger = LoggerFactory.getLogger(classOf[Tpcc])

  private val DEBUG = logger.isDebugEnabled

  val VERSION = "1.0.1"

  private val DRIVER = "DRIVER"

  private val WAREHOUSECOUNT = "WAREHOUSECOUNT"

  private val DATABASE = "DATABASE"

  private val USER = "USER"

  private val PASSWORD = "PASSWORD"

  private val CONNECTIONS = "CONNECTIONS"

  private val RAMPUPTIME = "RAMPUPTIME"

  private val DURATION = "DURATION"

  private val JDBCURL = "JDBCURL"

  private val PROPERTIESFILE = "./conf/tpcc.properties"

  private val TRANSACTION_NAME = Array("NewOrder", "Payment", "Order Stat", "Delivery", "Slev")

  @volatile var counting_on: Boolean = false

  def main(argv: Array[String]) {
//    println("TPCC version " + VERSION + " Number of Arguments: " +
//      argv.length)
//    val sysProp = Array("os.name", "os.arch", "os.version", "java.runtime.name", "java.vm.version", "java.library.path")
//    for (s <- sysProp) {
//      logger.info("System Property: " + s + " = " + System.getProperty(s))
//    }
//    val df = new DecimalFormat("#,##0.0")
//    println("maxMemory = " +
//      df.format(Runtime.getRuntime.totalMemory() / (1024.0 * 1024.0)) +
//      " MB")

    val tpcc = new TpccCommandGen
    var ret = 0
    if (argv.length == 0) {
//      println("Using the properties file for configuration.")
      tpcc.init()
      ret = tpcc.runBenchmark(false, argv)
    } else {
      if ((argv.length % 2) == 0) {
//        println("Using the command line arguments for configuration.")
        tpcc.init()
        ret = tpcc.runBenchmark(true, argv)
      } else {
        println("Invalid number of arguments.")
        println("The possible arguments are as follows: ")
        println("-w [number of warehouses]")
        println("-n [number of transactions to execute]")
        System.exit(-1)
      }
    }
//    println("Terminating process now")
    System.exit(ret)
  }
}

class TpccCommandGen {

  private var numberOfTestTransactions = NUMBER_OF_TX_TESTS

  private var numWare: Int = _

  private var properties: Properties = _

  private var inputStream: InputStream = _

  private def init() {
//    logger.info("Loading properties from: " + PROPERTIESFILE)
    properties = new Properties()
    inputStream = new FileInputStream(PROPERTIESFILE)
    properties.load(inputStream)
  }

  private def runBenchmark(overridePropertiesFile: Boolean, argv: Array[String]): Int = {
//    println("***************************************")
//    println("****** Java TPC-C Load Generator ******")
//    println("***************************************")
    RtHist.histInit()

    {
      numWare = Integer.parseInt(properties.getProperty(WAREHOUSECOUNT))
      numberOfTestTransactions = 1000
    }

    if (overridePropertiesFile) {
      var i = 0
      while (i < argv.length) {
        if (argv(i) == "-w") {
          numWare = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-n") {
          numberOfTestTransactions = Integer.parseInt(argv(i + 1))
        } else {
          println("Incorrect Argument: " + argv(i))
          println("The possible arguments are as follows: ")
          println("-w [number of warehouses]")
          println("-n [number of transactions to execute]")
          System.exit(-1)
        }
        i = i + 2
      }
    }
    if (numWare < 1) {
      throw new RuntimeException("Warehouse count has to be greater than or equal to 1.")
    }
    if (numberOfTestTransactions < 1) {
      throw new RuntimeException("Number of transactions to execute has to be greater than or equal to 1.")
    }


//    System.out.print("<Parameters>\n")
//    System.out.print("  [warehouse]: %d\n".format(numWare))
//    System.out.print("   [numTests]: %d\n".format(numberOfTestTransactions))
    Util.seqInit(10, 10, 1, 1, 1)

    val commandHistory:Array[TpccCommand]=new Array[TpccCommand](numberOfTestTransactions);
    var commandHistoryCounter = 0

    val newOrderMix: INewOrder = new INewOrder {
      override def newOrderTx(datetime: Date, t_num: Int, w_id: Int, d_id: Int, c_id: Int, o_ol_count: Int, o_all_local: Int, itemid: Array[Int], supware: Array[Int], quantity: Array[Int], price: Array[Float], iname: Array[String], stock: Array[Int], bg: Array[Char], amt: Array[Float])(implicit tInfo: ThreadInfo): Int = {
        commandHistory(commandHistoryCounter) = NewOrderCommand(datetime, t_num, w_id, d_id, c_id, o_ol_count, o_all_local, itemid, supware, quantity, price, iname, stock, bg, amt)
        commandHistoryCounter += 1

        1
      }
    }
    val paymentMix: IPayment = new IPayment {
      override def paymentTx(datetime: Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_w_id: Int, c_d_id: Int, c_id: Int, c_last: String, h_amount: Float)(implicit tInfo: ThreadInfo): Int = {
        commandHistory(commandHistoryCounter) = PaymentCommand(datetime,t_num,w_id,d_id, c_by_name, c_w_id, c_d_id, c_id, c_last, h_amount)
        commandHistoryCounter += 1

        1
      }
    }
    val orderStatMix: IOrderStatus = new IOrderStatus {
      override def orderStatusTx(datetime: Date, t_num: Int, w_id: Int, d_id: Int, c_by_name: Int, c_id: Int, c_last: String)(implicit tInfo: ThreadInfo): Int = {
        commandHistory(commandHistoryCounter) = OrderStatusCommand(datetime, t_num, w_id, d_id, c_by_name, c_id, c_last)
        commandHistoryCounter += 1

        1
      }
    }
    val slevMix: IStockLevel = new IStockLevel {
      override def stockLevelTx(t_num: Int, w_id: Int, d_id: Int, threshold: Int)(implicit tInfo: ThreadInfo): Int = {
        commandHistory(commandHistoryCounter) = StockLevelCommand(t_num, w_id, d_id, threshold)
        commandHistoryCounter += 1

        1
      }
    }
    val deliveryMix: IDelivery = new IDelivery {
      override def deliveryTx(datetime: Date, w_id: Int, o_carrier_id: Int)(implicit tInfo: ThreadInfo): Int = {
        commandHistory(commandHistoryCounter) = DeliveryCommand(datetime, w_id, o_carrier_id)
        commandHistoryCounter += 1

        1
      }
    }
    val fetchSize = 1
    val numConn = 1
    val numberOfTestTransactionsPerThread = numberOfTestTransactions

    val driver = new TpccDriver(null, fetchSize, TRANSACTION_COUNT, newOrderMix, paymentMix, orderStatMix, slevMix, deliveryMix)
    
    try {
//      if (DEBUG) {
//        logger.debug("Starting driver with: numberOfTestTransactions: " + numberOfTestTransactions + " num_ware: " + numWare)
//      }
      driver.runAllTransactions(new ThreadInfo(0), numWare, numConn, false, numberOfTestTransactionsPerThread)
    } catch {
      case e: Throwable => logger.error("Unhandled exception", e)
    }
    println(commandHistory.mkString("","\n",""))

    0
  }


}
