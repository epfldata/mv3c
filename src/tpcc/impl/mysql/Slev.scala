package ddbt.tpcc.loadtest

import ddbt.lib.util.ThreadInfo
import java.sql.Connection
import java.sql.ResultSet
import java.sql.SQLException
import org.slf4j.LoggerFactory
import org.slf4j.Logger
import ddbt.tpcc.itx.IStockLevel
import Slev._
import TpccConstants._

object Slev {

  private val logger = LoggerFactory.getLogger(classOf[Slev])

  private val DEBUG = logger.isDebugEnabled

  private val TRACE = logger.isTraceEnabled

  private val SHOW_OUTPUT = TpccConstants.SHOW_OUTPUT
}

class Slev(pStms: TpccStatements) extends IStockLevel {

  private var pStmts: TpccStatements = pStms

  override def stockLevelTx(t_num: Int, 
      w_id_arg: Int, 
      d_id_arg: Int, 
      level_arg: Int)(implicit tInfo: ThreadInfo): Int = {
    try {
      pStmts.setAutoCommit(false)
      if (DEBUG) logger.debug("Transaction: 	SLEV")
      val w_id = w_id_arg
      val d_id = d_id_arg
      val level = level_arg
      var d_next_o_id = 0
      var stock_count = 0
      var ol_i_id = 0
      try {
        pStmts.getStatement(32).setInt(1, d_id)
        pStmts.getStatement(32).setInt(2, w_id)
        if (TRACE) logger.trace("SELECT d_next_o_id FROM district WHERE d_id = " + d_id + 
          " AND d_w_id = " + 
          w_id)
        val rs = pStmts.getStatement(32).executeQuery()
        if (rs.next()) {
          d_next_o_id = rs.getInt(1)
        }
        rs.close()
      } catch {
        case e: SQLException => {
          logger.error("SELECT d_next_o_id FROM district WHERE d_id = " + d_id + 
            " AND d_w_id = " + 
            w_id, e)
          throw new Exception("Slev select transaction error", e)
        }
      }
      try {
        pStmts.getStatement(33).setInt(1, w_id)
        pStmts.getStatement(33).setInt(2, d_id)
        pStmts.getStatement(33).setInt(3, d_next_o_id)
        pStmts.getStatement(33).setInt(4, d_next_o_id)
        pStmts.getStatement(33).setInt(5, level)
        if (TRACE) logger.trace("SELECT COUNT(DISTINCT (s_i_id)) FROM order_line, stock WHERE ol_w_id = " + 
          w_id + 
          " AND ol_d_id = " + 
          d_id + 
          " AND ol_o_id < " + 
          d_next_o_id + 
          " AND ol_o_id >= (" + 
          d_next_o_id + 
          " - 20) AND s_w_id=ol_w_id AND s_i_id=ol_i_id AND s_quantity < " + level)
        val rs = pStmts.getStatement(33).executeQuery()
        while (rs.next()) {
          stock_count = rs.getInt(1)
        }
        rs.close()
      } catch {
        case e: SQLException => {
          logger.error("SELECT COUNT(DISTINCT (s_i_id)) FROM order_line, stock WHERE ol_w_id = " + 
            w_id + 
            " AND ol_d_id = " + 
            d_id + 
            " AND ol_o_id < " + 
            d_next_o_id + 
            " AND ol_o_id >= (" + 
            d_next_o_id + 
          " - 20) AND s_w_id=ol_w_id AND s_i_id=ol_i_id AND s_quantity < " + level, e)
          throw new Exception("Slev select transaction error", e)
        }
      }
      // try {
      //   pStmts.getStatement(33).setInt(1, w_id)
      //   pStmts.getStatement(33).setInt(2, d_id)
      //   pStmts.getStatement(33).setInt(3, d_next_o_id)
      //   pStmts.getStatement(33).setInt(4, d_next_o_id)
      //   if (TRACE) logger.trace("SELECT DISTINCT ol_i_id FROM order_line WHERE ol_w_id = " + 
      //     w_id + 
      //     " AND ol_d_id = " + 
      //     d_id + 
      //     " AND ol_o_id < " + 
      //     d_next_o_id + 
      //     " AND ol_o_id >= (" + 
      //     d_next_o_id + 
      //     " - 20)")
      //   val rs = pStmts.getStatement(33).executeQuery()
      //   while (rs.next()) {
      //     ol_i_id = rs.getInt(1)
      //   }
      //   rs.close()
      // } catch {
      //   case e: SQLException => {
      //     logger.error("SELECT DISTINCT ol_i_id FROM order_line WHERE ol_w_id = " + 
      //       w_id + 
      //       " AND ol_d_id = " + 
      //       d_id + 
      //       " AND ol_o_id < " + 
      //       d_next_o_id + 
      //       " AND ol_o_id >= (" + 
      //       d_next_o_id + 
      //       " - 20)", e)
      //     throw new Exception("Slev select transaction error", e)
      //   }
      // }
      // try {
      //   pStmts.getStatement(34).setInt(1, w_id)
      //   pStmts.getStatement(34).setInt(2, ol_i_id)
      //   pStmts.getStatement(34).setInt(3, level)
      //   if (TRACE) logger.trace("SELECT count(*) FROM stock WHERE s_w_id = " + w_id + 
      //     " AND s_i_id = " + 
      //     ol_i_id + 
      //     " AND s_quantity < " + 
      //     level)
      //   val rs = pStmts.getStatement(34).executeQuery()
      //   if (rs.next()) {
      //     stock_count = rs.getInt(1)
      //   }
      //   rs.close()
      // } catch {
      //   case e: SQLException => {
      //     logger.error("SELECT count(*) FROM stock WHERE s_w_id = " + w_id + 
      //       " AND s_i_id = " + 
      //       ol_i_id + 
      //       " AND s_quantity < " + 
      //       level, e)
      //     throw new Exception("Slev select transaction error", e)
      //   }
      // }
      pStmts.commit()

      val output: StringBuilder = new StringBuilder
      output.append("\n+########################## STOCK-LEVEL ##########################+")
      output.append("\n Warehouse: ").append(w_id)
      output.append("\n District:  ").append(d_id)
      output.append("\n\n Stock Level Threshold: ").append(level)
      output.append("\n Low Stock Count:       ").append(stock_count)
      output.append("\n+#################################################################+\n\n")
      if(Slev.SHOW_OUTPUT) logger.info(output.toString)

      1
    } catch {
      case e: Exception => try {
        pStmts.rollback()
        0
      } catch {
        case th: Throwable => throw new RuntimeException("Slev error", th)
      } finally {
        logger.error("Slev error", e)
      }
    }
  }
}
