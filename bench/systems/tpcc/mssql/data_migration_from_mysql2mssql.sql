USE tpcc
GO

INSERT INTO tpcc.dbo.warehouse
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.warehouse')
GO

INSERT INTO tpcc.dbo.district
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.district')
GO

INSERT INTO tpcc.dbo.customer
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.customer')
GO

INSERT INTO tpcc.dbo.item
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.item')
GO

INSERT INTO tpcc.dbo.orders
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.orders')
GO

INSERT INTO tpcc.dbo.new_orders
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.new_orders')
GO

INSERT INTO tpcc.dbo.order_line
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.order_line')
GO

INSERT INTO tpcc.dbo.stock
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.stock')
GO

INSERT INTO tpcc.dbo.history
SELECT * FROM openquery(MYSQL, 'SELECT * FROM tpcc_migration.history')
GO