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

trait InMemoryTxImpl extends IInMemoryTx {
	var SharedData:TpccTable = null

	override def setSharedData(db:AnyRef) = {
		SharedData = db.asInstanceOf[TpccTable]
		this
	}
}

trait InMemoryTxImplViaITpccTable extends InMemoryTxImpl {
	var ISharedData:ITpccTable = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[ITpccTable]
		this
	}
} 

trait InMemoryTxImplViaMVCCTpccTableV0 extends InMemoryTxImpl {
	var ISharedData:MVCCTpccTableV0 = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[MVCCTpccTableV0]
		this
	}
}

trait InMemoryTxImplViaMVCCTpccTableV1 extends InMemoryTxImpl {
	var ISharedData:MVCCTpccTableV1 = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[MVCCTpccTableV1]
		this
	}
}

trait InMemoryTxImplViaMVCCTpccTableV2 extends InMemoryTxImpl {
	var ISharedData:MVCCTpccTableV2 = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[MVCCTpccTableV2]
		this
	}
}

trait InMemoryTxImplViaMVCCTpccTableV3 extends InMemoryTxImpl {
	var ISharedData:MVCCTpccTableV3 = null

	override def setSharedData(db:AnyRef) = {
		ISharedData = db.asInstanceOf[MVCCTpccTableV3]
		this
	}
}

trait INewOrderInMem extends INewOrder with IInMemoryTx
trait IPaymentInMem extends IPayment with IInMemoryTx
trait IOrderStatusInMem extends IOrderStatus with IInMemoryTx
trait IDeliveryInMem extends IDelivery with IInMemoryTx
trait IStockLevelInMem extends IStockLevel with IInMemoryTx
