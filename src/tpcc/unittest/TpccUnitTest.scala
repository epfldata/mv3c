package ddbt.tpcc.loadtest

import ddbt.lib.util.ThreadInfo
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
import java.sql.Connection
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

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
        println("-i [target implementation]")
        println("-n [number of transactions to execute]")
        System.exit(-1)
      }
    }
    println("Terminating process now")
    System.exit(ret)
  }
}

class TpccUnitTest {

  private var numberOfTestTransactions = NUMBER_OF_TX_TESTS

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

  private var prev_s: Array[Int] = new Array[Int](5)

  private var prev_l: Array[Int] = new Array[Int](5)

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
        } else if (argv(i) == "-n") {
          numberOfTestTransactions = Integer.parseInt(argv(i + 1))
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
          println("-i [target implementation]")
          println("-n [number of transactions to execute]")
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

    if(implVersionUnderTest == 11) {
      // newOrder = new ddbt.tpcc.tx11.NewOrder
      // payment = new ddbt.tpcc.tx11.Payment
      // orderStat = new ddbt.tpcc.tx11.OrderStatus
      // delivery = new ddbt.tpcc.tx11.Delivery
      // slev = new ddbt.tpcc.tx11.StockLevel
    } else if(implVersionUnderTest == 10) {
      newOrder = new ddbt.tpcc.tx10.NewOrder
      payment = new ddbt.tpcc.tx10.Payment
      orderStat = new ddbt.tpcc.tx10.OrderStatus
      delivery = new ddbt.tpcc.tx10.Delivery
      slev = new ddbt.tpcc.tx10.StockLevel
    } else if(implVersionUnderTest == 9) {
      newOrder = new ddbt.tpcc.tx9.NewOrder
      payment = new ddbt.tpcc.tx9.Payment
      orderStat = new ddbt.tpcc.tx9.OrderStatus
      delivery = new ddbt.tpcc.tx9.Delivery
      slev = new ddbt.tpcc.tx9.StockLevel
    } else if(implVersionUnderTest == 8) {
      newOrder = new ddbt.tpcc.tx8.NewOrder
      payment = new ddbt.tpcc.tx8.Payment
      orderStat = new ddbt.tpcc.tx8.OrderStatus
      delivery = new ddbt.tpcc.tx8.Delivery
      slev = new ddbt.tpcc.tx8.StockLevel
    } else if(implVersionUnderTest == 7) {
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

    System.out.print("<Parameters>\n")
    System.out.print("     [driver]: %s\n".format(javaDriver))
    System.out.print("        [URL]: %s\n".format(jdbcUrl))
    System.out.print("       [user]: %s\n".format(dbUser))
    System.out.print("       [pass]: %s\n".format(dbPassword))
    System.out.print("  [warehouse]: %d\n".format(numWare))
    System.out.print(" [connection]: %d\n".format(numConn))
    System.out.print("     [rampup]: %d (sec.)\n".format(rampupTime))
    System.out.print("    [measure]: %d (sec.)\n".format(measureTime))
    System.out.print("[implVersion]: %d\n".format(implVersionUnderTest))
    System.out.print("   [numTests]: %d\n".format(numberOfTestTransactions))
    Util.seqInit(10, 10, 1, 1, 1)
    if (DEBUG) logger.debug("Creating TpccThread")

    val executor = Executors.newFixedThreadPool(numConn, new NamedThreadFactory("tpcc-thread"))

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
      } else if(implVersionUnderTest == 8) {
        SharedDataScala = SharedDataScala.toMVCCTpccTableV1
      } else if(implVersionUnderTest == 9) {
        SharedDataScala = SharedDataScala.toMVCCTpccTableV2
      } else if(implVersionUnderTest == 10) {
        SharedDataScala = SharedDataScala.toMVCCTpccTableV3(numConn)
      } 
      // else if(implVersionUnderTest == 11) {
      //   SharedDataScala = SharedDataScala.toMVCCTpccTableV4
      // }
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

    val drivers = new Array[TpccDriver](numConn)
    if(numConn == 1) {
      val newOrderMix: INewOrder = new NewOrderMixedImpl(new ddbt.tpcc.loadtest.NewOrder(pStmts), newOrder)
      val paymentMix: IPayment = new PaymentMixedImpl(new ddbt.tpcc.loadtest.Payment(pStmts), payment)
      val orderStatMix: IOrderStatus = new OrderStatusMixedImpl(new ddbt.tpcc.loadtest.OrderStat(pStmts), orderStat)
      val slevMix: IStockLevel = new StockLevelMixedImpl(new ddbt.tpcc.loadtest.Slev(pStmts), slev)
      val deliveryMix: IDelivery = new DeliveryMixedImpl(new ddbt.tpcc.loadtest.Delivery(pStmts), delivery)

      val driver = new TpccDriver(conn, fetchSize, TRANSACTION_COUNT, newOrderMix, paymentMix, orderStatMix, slevMix, deliveryMix)
      drivers(0) = driver
      try {
        if (DEBUG) {
          logger.debug("Starting driver with: numberOfTestTransactions: " + numberOfTestTransactions + " num_ware: " + 
            numWare + 
            " num_conn: " + 
            numConn)
        }
        driver.runAllTransactions(new ThreadInfo(0), numWare, numConn, false, numberOfTestTransactionsPerThread)
      } catch {
        case e: Throwable => logger.error("Unhandled exception", e)
      }

      if(implVersionUnderTest == -1) {
        SharedDataScala = LMSDataLoader.moveDataToTpccTable(SharedDataLMS, implVersionUnderTest)
      }
    } else {
      var listOfCommittedCommands: Seq[ddbt.lib.util.XactCommand] = null

      { // Running the parallel implementation with enabled unit-test
        // in order to collect the committed transactions
        SharedDataScala.enableUnitTest
        for (i <- 0 until numConn) {
          val conn: Connection = null //connectToDB(javaDriver, jdbcUrl, dbUser, dbPassword)
          // val pStmts: TpccStatements = new TpccStatements(conn, fetchSize)
          // val newOrder: NewOrder = new NewOrder(pStmts)
          // val payment: Payment = new Payment(pStmts)
          // val orderStat: OrderStat = new OrderStat(pStmts)
          // val slev: Slev = new Slev(pStmts)
          // val delivery: Delivery = new Delivery(pStmts)
          // val newOrder: INewOrder = new ddbt.tpcc.tx.NewOrder(SharedDataScala)
          // val payment: IPayment = new ddbt.tpcc.tx.Payment(SharedDataScala)
          // val orderStat: IOrderStatus = new ddbt.tpcc.tx.OrderStatus(SharedDataScala)
          // val slev: IStockLevel = new ddbt.tpcc.tx.StockLevel(SharedDataScala)
          // val delivery: IDelivery = new ddbt.tpcc.tx.Delivery(SharedDataScala)

          val worker = new TpccThread(new ThreadInfo(i), port, 1, dbUser, dbPassword, numWare, numConn, javaDriver, jdbcUrl, 
            fetchSize, TRANSACTION_COUNT, conn, newOrder, payment, orderStat, slev, delivery, false, numberOfTestTransactionsPerThread)
          drivers(i) = worker.driver
          executor.execute(worker)

          // conn.close
        }
        executor.shutdown()
        try {
          executor.awaitTermination(3600, TimeUnit.SECONDS)
        } catch {
          case e: InterruptedException => throw new RuntimeException("Timed out waiting for executor to terminate")
        }

        listOfCommittedCommands = SharedDataScala.getListOfCommittedCommands
      }

      { // Running the transactions serially, in the same serial order
        // against a database using only a single thread, to make sure
        // that no transaction fails, in order to check the correctness
        // of the execution
        val newOrder: INewOrder = new ddbt.tpcc.loadtest.NewOrder(pStmts)
        val payment: IPayment = new ddbt.tpcc.loadtest.Payment(pStmts)
        val orderStat: IOrderStatus = new ddbt.tpcc.loadtest.OrderStat(pStmts)
        val slev: IStockLevel = new ddbt.tpcc.loadtest.Slev(pStmts)
        val delivery: IDelivery = new ddbt.tpcc.loadtest.Delivery(pStmts)

        val driver = new TpccDriver(conn, fetchSize, TRANSACTION_COUNT, newOrder, payment, orderStat, slev, delivery)

        val numConn = 1 //we want to avoid any unwanted rollback due to concurrency in the reference DB
        try {
          if (DEBUG) {
            logger.debug("Starting driver with: numberOfTestTransactions: " + numberOfTestTransactions + " num_ware: " + 
              numWare + 
              " num_conn: " + 
              numConn)
          }
          logger.info("Number of committed transactions in the reference implementation: " + listOfCommittedCommands.size)
          driver.runAllTransactions(new ThreadInfo(0), numWare, numConn, false, numberOfTestTransactionsPerThread, listOfCommittedCommands)
        } catch {
          case e: Throwable => logger.error("Unhandled exception", e)
        }
      }
    }

    {
      val newData = new TpccTable(if(implVersionUnderTest == 6) 5 else implVersionUnderTest)
      newData.loadDataIntoMaps(javaDriver,jdbcUrl,dbUser,dbPassword)

      if(newData equals SharedDataScala.toTpccTable) {
        println("\nAll transactions completed successfully and the result is correct.")
      } else {
        println("\nThere is some error in transactions, as the results does not match.")
      }
    }

    0
  }

  lazy val numberOfTestTransactionsPerThread = numberOfTestTransactions/numConn
  // def transactionCountChecker(counter:Int) = (counter < numberOfTestTransactionsPerThread)

}
