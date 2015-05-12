package ddbt.tpcc.loadtest

import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.nio.charset.Charset
import java.text.DecimalFormat
import java.util.Properties
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import ddbt.tpcc.tx.TpccTable
import TpccUnitTest._
import ddbt.tpcc.mtx._
import ddbt.tpcc.itx._
import DatabaseConnector._
import TpccConstants._
import tpcc.lmsgen._
import tpcc.lms._

object TpccUnitTest {

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

  @volatile var activate_transaction: Int = 0

  def main(argv: Array[String]) {
    println("TPCC version " + VERSION + " Number of Arguments: " + 
      argv.length)
    val sysProp = Array("os.name", "os.arch", "os.version", "java.runtime.name", "java.vm.version", "java.library.path")
    for (s <- sysProp) {
      logger.info("System Property: " + s + " = " + System.getProperty(s))
    }
    val df = new DecimalFormat("#,##0.0")
    println("maxMemory = " + 
      df.format(Runtime.getRuntime.totalMemory() / (1024.0 * 1024.0)) + 
      " MB")

    val tpcc = new TpccUnitTest
    var ret = 0
    if (argv.length == 0) {
      println("Using the properties file for configuration.")
      tpcc.init()
      ret = tpcc.runBenchmark(false, argv)
    } else {
      if ((argv.length % 2) == 0) {
        println("Using the command line arguments for configuration.")
        tpcc.init()
        ret = tpcc.runBenchmark(true, argv)
      } else {
        println("Invalid number of arguments.")
        println("The possible arguments are as follows: ")
        println("-h [database host]")
        println("-d [database name]")
        println("-u [database username]")
        println("-p [database password]")
        println("-w [number of warehouses]")
        println("-c [number of connections]")
        println("-r [ramp up time]")
        println("-t [duration of the benchmark (sec)]")
        println("-j [java driver]")
        println("-l [jdbc url]")
        println("-h [jdbc fetch size]")
        System.exit(-1)
      }
    }
    println("Terminating process now")
    System.exit(ret)
  }
}

class TpccUnitTest {

  private var implVersionUnderTest = 0

  private var javaDriver: String = _

  private var jdbcUrl: String = _

  private var dbUser: String = _

  private var dbPassword: String = _

  private var numWare: Int = _

  private var numConn: Int = _

  private var rampupTime: Int = _

  private var measureTime: Int = _

  private var fetchSize: Int = 100

  private var num_node: Int = _

  private val success = new Array[Int](TRANSACTION_COUNT)

  private val late = new Array[Int](TRANSACTION_COUNT)

  private val retry = new Array[Int](TRANSACTION_COUNT)

  private val failure = new Array[Int](TRANSACTION_COUNT)

  private var success2: Array[Array[Int]] = _

  private var late2: Array[Array[Int]] = _

  private var retry2: Array[Array[Int]] = _

  private var failure2: Array[Array[Int]] = _

  private var success2_sum: Array[Int] = new Array[Int](TRANSACTION_COUNT)

  private var late2_sum: Array[Int] = new Array[Int](TRANSACTION_COUNT)

  private var retry2_sum: Array[Int] = new Array[Int](TRANSACTION_COUNT)

  private var failure2_sum: Array[Int] = new Array[Int](TRANSACTION_COUNT)

  private var prev_s: Array[Int] = new Array[Int](5)

  private var prev_l: Array[Int] = new Array[Int](5)

  java.util.Arrays.fill(success, 0)
  java.util.Arrays.fill(late, 0)
  java.util.Arrays.fill(retry, 0)
  java.util.Arrays.fill(failure, 0)
  java.util.Arrays.fill(success2_sum, 0)
  java.util.Arrays.fill(late2_sum, 0)
  java.util.Arrays.fill(retry2_sum, 0)
  java.util.Arrays.fill(failure2_sum, 0)
  java.util.Arrays.fill(prev_s, 0)
  java.util.Arrays.fill(prev_l, 0)

  private var max_rt: Array[Float] = new Array[Float](5)

  java.util.Arrays.fill(max_rt, 0f)

  private var port: Int = 3306

  private var properties: Properties = _

  private var inputStream: InputStream = _

  private def init() {
    logger.info("Loading properties from: " + PROPERTIESFILE)
    properties = new Properties()
    inputStream = new FileInputStream(PROPERTIESFILE)
    properties.load(inputStream)
  }

  private def runBenchmark(overridePropertiesFile: Boolean, argv: Array[String]): Int = {
    println("***************************************")
    println("****** Java TPC-C Load Generator ******")
    println("***************************************")
    RtHist.histInit()
    activate_transaction = 1
    for (i <- 0 until TRANSACTION_COUNT) {
      success(i) = 0
      late(i) = 0
      retry(i) = 0
      failure(i) = 0
      prev_s(i) = 0
      prev_l(i) = 0
      max_rt(i) = 0f
    }
    num_node = 0

    {
      dbUser = properties.getProperty(USER)
      dbPassword = properties.getProperty(PASSWORD)
      numWare = Integer.parseInt(properties.getProperty(WAREHOUSECOUNT))
      numConn = Integer.parseInt(properties.getProperty(CONNECTIONS))
      rampupTime = Integer.parseInt(properties.getProperty(RAMPUPTIME))
      measureTime = Integer.parseInt(properties.getProperty(DURATION))
      javaDriver = properties.getProperty(DRIVER)
      jdbcUrl = properties.getProperty(JDBCURL)
      val jdbcFetchSize = properties.getProperty("JDBCFETCHSIZE")
      if (jdbcFetchSize != null) {
        fetchSize = Integer.parseInt(jdbcFetchSize)
      }
    }

    if (overridePropertiesFile) {
      var i = 0
      while (i < argv.length) {
        if (argv(i) == "-u") {
          dbUser = argv(i + 1)
        } else if (argv(i) == "-p") {
          dbPassword = argv(i + 1)
        } else if (argv(i) == "-w") {
          numWare = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-c") {
          numConn = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-r") {
          rampupTime = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-t") {
          measureTime = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-j") {
          javaDriver = argv(i + 1)
        } else if (argv(i) == "-l") {
          jdbcUrl = argv(i + 1)
        } else if (argv(i) == "-f") {
          fetchSize = Integer.parseInt(argv(i + 1))
        } else if (argv(i) == "-i") {
          implVersionUnderTest = Integer.parseInt(argv(i + 1))
        } else {
          println("Incorrect Argument: " + argv(i))
          println("The possible arguments are as follows: ")
          println("-h [database host]")
          println("-d [database name]")
          println("-u [database username]")
          println("-p [database password]")
          println("-w [number of warehouses]")
          println("-c [number of connections]")
          println("-r [ramp up time]")
          println("-t [duration of the benchmark (sec)]")
          println("-j [java driver]")
          println("-l [jdbc url]")
          println("-h [jdbc fetch size]")
          println("-i [target implementation (-1|1|2|3|4|5|6|7)]")
          System.exit(-1)
        }
        i = i + 2
      }
    }
    if (num_node > 0) {
      if (numWare % num_node != 0) {
        logger.error(" [warehouse] value must be devided by [num_node].")
        return 1
      }
      if (numConn % num_node != 0) {
        logger.error("[connection] value must be devided by [num_node].")
        return 1
      }
    }
    if (javaDriver == null) {
      throw new RuntimeException("Java Driver is null.")
    }
    if (jdbcUrl == null) {
      throw new RuntimeException("JDBC Url is null.")
    }
    if (dbUser == null) {
      throw new RuntimeException("User is null.")
    }
    if (dbPassword == null) {
      throw new RuntimeException("Password is null.")
    }
    if (numWare < 1) {
      throw new RuntimeException("Warehouse count has to be greater than or equal to 1.")
    }
    if (numConn < 1) {
      throw new RuntimeException("Connections has to be greater than or equal to 1.")
    }
    if (rampupTime < 1) {
      throw new RuntimeException("Rampup time has to be greater than or equal to 1.")
    }
    if (measureTime < 1) {
      throw new RuntimeException("Duration has to be greater than or equal to 1.")
    }
    if (implVersionUnderTest == 0) {
      throw new RuntimeException("Target implementation should be selected for testing.")
    }

    var newOrder: INewOrderInMem = null
    var payment: IPaymentInMem = null
    var orderStat: IOrderStatusInMem = null
    var delivery: IDeliveryInMem = null
    var slev: IStockLevelInMem = null

    if(implVersionUnderTest == 7) {
      newOrder = new ddbt.tpcc.tx7.NewOrder
      payment = new ddbt.tpcc.tx7.Payment
      orderStat = new ddbt.tpcc.tx7.OrderStatus
      delivery = new ddbt.tpcc.tx7.Delivery
      slev = new ddbt.tpcc.tx7.StockLevel
    } else if(implVersionUnderTest == 6) {
      newOrder = new ddbt.tpcc.tx6.NewOrder
      payment = new ddbt.tpcc.tx6.Payment
      orderStat = new ddbt.tpcc.tx6.OrderStatus
      delivery = new ddbt.tpcc.tx6.Delivery
      slev = new ddbt.tpcc.tx6.StockLevel
    } else if(implVersionUnderTest == 5) {
      newOrder = new ddbt.tpcc.tx5.NewOrder
      payment = new ddbt.tpcc.tx5.Payment
      orderStat = new ddbt.tpcc.tx5.OrderStatus
      delivery = new ddbt.tpcc.tx5.Delivery
      slev = new ddbt.tpcc.tx5.StockLevel
    } else if(implVersionUnderTest == 4) {
      newOrder = new ddbt.tpcc.tx4.NewOrder
      payment = new ddbt.tpcc.tx4.Payment
      orderStat = new ddbt.tpcc.tx4.OrderStatus
      delivery = new ddbt.tpcc.tx4.Delivery
      slev = new ddbt.tpcc.tx4.StockLevel
    } else if(implVersionUnderTest == 3) {
      newOrder = new ddbt.tpcc.tx3.NewOrder
      payment = new ddbt.tpcc.tx3.Payment
      orderStat = new ddbt.tpcc.tx3.OrderStatus
      delivery = new ddbt.tpcc.tx3.Delivery
      slev = new ddbt.tpcc.tx3.StockLevel
    } else if(implVersionUnderTest == 2) {
      newOrder = new ddbt.tpcc.tx2.NewOrder
      payment = new ddbt.tpcc.tx2.Payment
      orderStat = new ddbt.tpcc.tx2.OrderStatus
      delivery = new ddbt.tpcc.tx2.Delivery
      slev = new ddbt.tpcc.tx2.StockLevel
    } else if(implVersionUnderTest == 1) {
      newOrder = new ddbt.tpcc.tx1.NewOrder
      payment = new ddbt.tpcc.tx1.Payment
      orderStat = new ddbt.tpcc.tx1.OrderStatus
      delivery = new ddbt.tpcc.tx1.Delivery
      slev = new ddbt.tpcc.tx1.StockLevel
    } else if(implVersionUnderTest == -1) {
      newOrder = new NewOrderLMSImpl
      payment = new PaymentLMSImpl
      orderStat = new OrderStatusLMSImpl
      delivery = new DeliveryLMSImpl
      slev = new StockLevelLMSImpl
    } else {
      throw new RuntimeException("No in-memory implementation selected.")
    }

    success2 = Array.ofDim[Int](TRANSACTION_COUNT, numConn)
    late2 = Array.ofDim[Int](TRANSACTION_COUNT, numConn)
    retry2 = Array.ofDim[Int](TRANSACTION_COUNT, numConn)
    failure2 = Array.ofDim[Int](TRANSACTION_COUNT, numConn)
    System.out.print("<Parameters>\n")
    System.out.print("     [driver]: %s\n".format(javaDriver))
    System.out.print("        [URL]: %s\n".format(jdbcUrl))
    System.out.print("       [user]: %s\n".format(dbUser))
    System.out.print("       [pass]: %s\n".format(dbPassword))
    System.out.print("  [warehouse]: %d\n".format(numWare))
    System.out.print(" [connection]: %d\n".format(numConn))
    System.out.print("     [rampup]: %d (sec.)\n".format(rampupTime))
    System.out.print("    [measure]: %d (sec.)\n".format(measureTime))
    Util.seqInit(10, 10, 1, 1, 1)
    if (DEBUG) logger.debug("Creating TpccThread")

    val conn = connectToDB(javaDriver, jdbcUrl, dbUser, dbPassword)
    val pStmts: TpccStatements = new TpccStatements(conn, fetchSize)

    var SharedDataScala: TpccTable = null
    var SharedDataLMS: EfficientExecutor = null
    if(implVersionUnderTest > 0) {
      SharedDataScala = new TpccTable(implVersionUnderTest)
      SharedDataScala.loadDataIntoMaps(javaDriver,jdbcUrl,dbUser,dbPassword)
      logger.info(SharedDataScala.getAllMapsInfoStr)
      if(implVersionUnderTest == 6) {
        SharedDataScala = SharedDataScala.toITpccTable
      } else if(implVersionUnderTest == 7) {
        SharedDataScala = SharedDataScala.toMVCCTpccTableV0
      }
      newOrder.setSharedData(SharedDataScala)
      payment.setSharedData(SharedDataScala)
      orderStat.setSharedData(SharedDataScala)
      slev.setSharedData(SharedDataScala)
      delivery.setSharedData(SharedDataScala)
    } else {
      SharedDataLMS = new EfficientExecutor
      LMSDataLoader.loadDataIntoMaps(SharedDataLMS,javaDriver,jdbcUrl,dbUser,dbPassword)
      logger.info(LMSDataLoader.getAllMapsInfoStr(SharedDataLMS))
      newOrder.setSharedData(SharedDataLMS)
      payment.setSharedData(SharedDataLMS)
      orderStat.setSharedData(SharedDataLMS)
      slev.setSharedData(SharedDataLMS)
      delivery.setSharedData(SharedDataLMS)
    }
    // val initialData = new TpccTable
    // initialData.loadDataIntoMaps(javaDriver,jdbcUrl,dbUser,dbPassword)

    // if(initialData equals SharedDataScala) {
    //   println("\n1- initialData equals SharedDataScala")
    // } else {
    //   println("\n1- initialData is not equal to SharedDataScala")
    // }
    val newOrderMix: INewOrder = new NewOrderMixedImpl(new ddbt.tpcc.loadtest.NewOrder(pStmts), newOrder)
    val paymentMix: IPayment = new PaymentMixedImpl(new ddbt.tpcc.loadtest.Payment(pStmts), payment)
    val orderStatMix: IOrderStatus = new OrderStatusMixedImpl(new ddbt.tpcc.loadtest.OrderStat(pStmts), orderStat)
    val slevMix: IStockLevel = new StockLevelMixedImpl(new ddbt.tpcc.loadtest.Slev(pStmts), slev)
    val deliveryMix: IDelivery = new DeliveryMixedImpl(new ddbt.tpcc.loadtest.Delivery(pStmts), delivery)

    val driver = new Driver(conn, fetchSize, success, late, retry, failure, success2, late2, retry2, 
      failure2, newOrderMix, paymentMix, orderStatMix, slevMix, deliveryMix)

    val number = 100

    try {
      if (DEBUG) {
        logger.debug("Starting driver with: number: " + number + " num_ware: " + 
          numWare + 
          " num_conn: " + 
          numConn)
      }
      driver.runTransaction(number, numWare, numConn, transactionCountChecker)
    } catch {
      case e: Throwable => logger.error("Unhandled exception", e)
    }

    {
      if(!(implVersionUnderTest > 0)) {
        SharedDataScala = LMSDataLoader.moveDataToTpccTable(SharedDataLMS, implVersionUnderTest)
      }
      val newData = new TpccTable(if(implVersionUnderTest == 6) 5 else implVersionUnderTest)
      newData.loadDataIntoMaps(javaDriver,jdbcUrl,dbUser,dbPassword)

      if(newData equals SharedDataScala) {
        println("\nAll transactions completed successfully and the result is correct.")
      } else {
        println("\nThere is some error in transactions, as the results does not match.")
      }
    }

    0
  }

  def transactionCountChecker(counter:Int) = (counter < NUMBER_OF_TX_TESTS)

}
