EXEC master.dbo.sp_addlinkedserver 
@server = N'MYSQL', 
@srvproduct=N'MySQL', 
@provider=N'MSDASQL', 
@provstr=N'DRIVER={MySQL ODBC 5.1 Driver}; SERVER=128.179.147.69; _
	DATABASE=tpcc_migration; USER=root; PASSWORD=ROOT; OPTION=3'