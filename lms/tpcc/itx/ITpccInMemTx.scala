package ddbt.tpcc.itx

import java.util.Date
import ddbt.tpcc.tx._

/**
 * NewOrder Transaction for TPC-C Benchmark
 *
 * @author Mohammad Dashti
 */
trait IInMemoryTx { self =>
	def setSharedData(db:AnyRef): self.type
}

class InMemoryTxImpl extends IInMemoryTx {
	var SharedData:TpccTable = null

	override def setSharedData(db:AnyRef) = {
		SharedData = db.asInstanceOf[TpccTable]
		this
	}
}

class InMemoryTxImplViaITpccTable extends InMemoryTxImpl {
	var ISharedData:ITpccTable = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[ITpccTable]
		this
	}
} 

class InMemoryTxImplViaMVCCTpccTable extends InMemoryTxImpl {
	var ISharedData:MVCCTpccTable = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[MVCCTpccTable]
		this
	}
} 

trait INewOrderInMem extends INewOrder with IInMemoryTx
trait IPaymentInMem extends IPayment with IInMemoryTx
trait IOrderStatusInMem extends IOrderStatus with IInMemoryTx
trait IDeliveryInMem extends IDelivery with IInMemoryTx
trait IStockLevelInMem extends IStockLevel with IInMemoryTx
