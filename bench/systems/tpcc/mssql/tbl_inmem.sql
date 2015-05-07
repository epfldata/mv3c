-- Create new database
CREATE DATABASE tpcc
GO

--Add MEMORY_OPTIMIZED_DATA filegroup to the database.
ALTER DATABASE tpcc
ADD FILEGROUP TPCCFileGroup CONTAINS MEMORY_OPTIMIZED_DATA
GO

USE tpcc
GO

-- Add a new file to the previous created file group
ALTER DATABASE tpcc ADD FILE
(
  NAME = N'tpccContainer', 
  FILENAME = N'C:\Program Files\Microsoft SQL Server\MSSQL12.MSSQLSERVER\MSSQL\DATA\tpccContainer'
)
TO FILEGROUP [TPCCFileGroup]
GO



CREATE TABLE dbo.warehouse
(
    w_id smallint NOT NULL PRIMARY KEY NONCLUSTERED HASH WITH (BUCKET_COUNT = 64), 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    w_name varchar(10) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    w_street_1 varchar(20) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    w_street_2 varchar(20) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    w_city varchar(20) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    w_state char(2) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    w_zip char(9) NOT NULL, 
    w_tax decimal(4, 2) NOT NULL, 
    w_ytd decimal(12, 2) NOT NULL
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO

CREATE TABLE dbo.district
(
    d_id smallint NOT NULL, 
    d_w_id smallint NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    d_name varchar(10) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    d_street_1 varchar(20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    d_street_2 varchar(20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    d_city varchar(20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    d_state char(2)  COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    d_zip char(9) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    d_tax decimal(4, 2) NOT NULL, 
    d_ytd decimal(12, 2) NOT NULL, 
    d_next_o_id int NOT NULL,
    CONSTRAINT [district_primaryKey] PRIMARY KEY NONCLUSTERED HASH ([d_w_id], [d_id]) WITH ( BUCKET_COUNT = 1024)
    /*CONSTRAINT [PK_district_d_w_id] PRIMARY KEY CLUSTERED ([d_w_id] ASC, [d_id] ASC)
     NOT SUPPORTED BY IN-MEMORY DB
    , 
    CONSTRAINT district$fkey_district_1 FOREIGN KEY (d_w_id) REFERENCES dbo.warehouse (w_id)
    */
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO

CREATE TABLE dbo.item
(
    i_id int NOT NULL PRIMARY KEY NONCLUSTERED HASH WITH (BUCKET_COUNT = 100000), 
    i_im_id int NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    i_name varchar(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    i_price decimal(5, 2) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    i_data varchar(1000) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO

CREATE TABLE dbo.customer
(
    [c_id] [int] NOT NULL,
    [c_d_id] [smallint] NOT NULL,
    [c_w_id] [smallint] NOT NULL,
    [c_first] [varchar](16) COLLATE Latin1_General_100_BIN2 NOT NULL,
    [c_middle] [char](2) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_last] [varchar](16) COLLATE Latin1_General_100_BIN2 NOT NULL,
    [c_street_1] [varchar](20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_street_2] [varchar](20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_city] [varchar](20) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_state] [char](2) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_zip] [char](9) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_phone] [char](16) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_since] [datetime2](0) NOT NULL,
    [c_credit] [char](2) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
    [c_credit_lim] [bigint] NOT NULL,
    [c_discount] [decimal](4, 2) NOT NULL,
    [c_balance] [decimal](12, 2) NOT NULL,
    [c_ytd_payment] [decimal](12, 2) NOT NULL,
    [c_payment_cnt] [smallint] NOT NULL,
    [c_delivery_cnt] [smallint] NOT NULL,
    [c_data] [varchar](1000) COLLATE SQL_Latin1_General_CP1_CI_AS NULL,

    /*c_id int NOT NULL, 
    c_d_id smallint NOT NULL, 
    c_w_id smallint NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    /*c_first varchar(16) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    /*c_middle char(2) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    /*c_last varchar(16) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    /*c_street_1 varchar(20) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    /*c_street_2 varchar(20) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    /*c_city varchar(20) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    /*c_state char(2) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    /*c_zip char(9) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    /*c_phone char(16) NOT NULL, */
    /*c_since datetime2(0) NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    /*c_credit char(2) NOT NULL, 
    c_credit_lim bigint NOT NULL, 
    c_discount decimal(4, 2) NOT NULL, 
    c_balance decimal(12, 2) NOT NULL, 
    c_ytd_payment decimal(12, 2) NOT NULL, 
    c_payment_cnt smallint NOT NULL, 
    c_delivery_cnt smallint NOT NULL, */
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR(MAX) according to character set mapping for latin1 character set
    */

    /*c_data varchar(1000) NULL, */
    CONSTRAINT [PK_customer_c_w_id] PRIMARY KEY NONCLUSTERED HASH ([c_w_id], [c_d_id], [c_id]) WITH ( BUCKET_COUNT = 131072),
    INDEX [idx_customer] NONCLUSTERED ([c_w_id] ASC, [c_d_id] ASC, [c_last] ASC, [c_first] ASC)
    /* NOT SUPPORTED BY IN-MEMORY DB
    ,
    CONSTRAINT customer$fkey_customer_1 FOREIGN KEY (c_w_id, c_d_id) REFERENCES dbo.district (d_w_id, d_id)
    */
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO
/*CREATE NONCLUSTERED INDEX idx_customer
    ON dbo.customer (c_w_id ASC, c_d_id ASC, c_last ASC, c_first ASC)
GO*/

CREATE TABLE dbo.history
(
    h_c_id int NOT NULL, 
    h_c_d_id smallint NOT NULL, 
    h_c_w_id smallint NOT NULL, 
    h_d_id smallint NOT NULL, 
    h_w_id smallint NOT NULL, 
    h_date datetime2(0) NOT NULL, 
    h_amount decimal(6, 2) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    h_data varchar(24) NOT NULL
    /* NOT SUPPORTED BY IN-MEMORY DB
    , 
    CONSTRAINT history$fkey_history_2 FOREIGN KEY (h_w_id, h_d_id) REFERENCES dbo.district (d_w_id, d_id)
    */
    INDEX [fkey_history_2] NONCLUSTERED HASH ([h_w_id], [h_d_id]) WITH ( BUCKET_COUNT = 1024)
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO
/*CREATE NONCLUSTERED INDEX fkey_history_2
    ON dbo.history (h_w_id ASC, h_d_id ASC)
GO*/

CREATE TABLE dbo.orders
(
    o_id int NOT NULL, 
    o_d_id smallint NOT NULL, 
    o_w_id smallint NOT NULL, 
    o_c_id int NOT NULL, 
    o_entry_d datetime2(0) NOT NULL, 
    o_carrier_id smallint NOT NULL, 
    o_ol_cnt smallint NOT NULL, 
    o_all_local smallint NOT NULL, 
    /*CONSTRAINT PK_orders_o_w_id PRIMARY KEY (o_w_id, o_d_id, o_id), 
     NOT SUPPORTED BY IN-MEMORY DB
    CONSTRAINT orders$fkey_orders_1 FOREIGN KEY (o_w_id, o_d_id, o_c_id) REFERENCES dbo.customer (c_w_id, c_d_id, c_id)
    */
    CONSTRAINT [PK_orders_o_w_id] PRIMARY KEY NONCLUSTERED ([o_w_id] ASC, [o_d_id] ASC, [o_id] ASC),
    INDEX [idx_orders] NONCLUSTERED HASH ([o_w_id], [o_d_id], [o_c_id], [o_id]) WITH ( BUCKET_COUNT = 2097152)
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO
/*CREATE NONCLUSTERED INDEX idx_orders
    ON dbo.orders (o_w_id ASC, o_d_id ASC, o_c_id ASC, o_id ASC)
GO*/

CREATE TABLE dbo.order_line
(
    ol_o_id int NOT NULL, 
    ol_d_id smallint NOT NULL, 
    ol_w_id smallint NOT NULL, 
    ol_number smallint NOT NULL, 
    ol_i_id int NOT NULL, 
    ol_supply_w_id smallint NOT NULL, 
    ol_delivery_d datetime2(0) NOT NULL, 
    ol_quantity smallint NOT NULL, 
    ol_amount decimal(6, 2) NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    ol_dist_info char(24) NOT NULL,
    CONSTRAINT [PK_order_line_ol_w_id] PRIMARY KEY NONCLUSTERED HASH ([ol_w_id], [ol_d_id], [ol_o_id], [ol_number]) WITH ( BUCKET_COUNT = 33554432),
    INDEX [fkey_order_line_2] NONCLUSTERED ([ol_supply_w_id] ASC, [ol_i_id] ASC)
    /* NOT SUPPORTED BY IN-MEMORY DB
    , 
    CONSTRAINT order_line$fkey_order_line_1 FOREIGN KEY (ol_w_id, ol_d_id, ol_o_id) REFERENCES dbo.orders (o_w_id, o_d_id, o_id)
    */
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO
/*CREATE NONCLUSTERED INDEX fkey_order_line_2
    ON dbo.order_line (ol_supply_w_id ASC, ol_i_id ASC)
GO*/

CREATE TABLE dbo.new_orders
(
    no_o_id int NOT NULL, 
    no_d_id smallint NOT NULL, 
    no_w_id smallint NOT NULL, 
    CONSTRAINT [PK_new_orders_no_w_id] PRIMARY KEY NONCLUSTERED HASH ([no_w_id], [no_d_id], [no_o_id]) WITH ( BUCKET_COUNT = 524288),
    /* NOT SUPPORTED BY IN-MEMORY DB
    , 
    CONSTRAINT new_orders$fkey_new_orders_1 FOREIGN KEY (no_w_id, no_d_id, no_o_id) REFERENCES dbo.orders (o_w_id, o_d_id, o_id)
    */
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO

  CREATE TABLE dbo.stock
(
    s_i_id int NOT NULL, 
    s_w_id smallint NOT NULL, 
    s_quantity smallint NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_01 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_02 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_03 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_04 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_05 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_06 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_07 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_08 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_09 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
    */

    s_dist_10 char(24) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    s_ytd decimal(8, 0) NOT NULL, 
    s_order_cnt smallint NOT NULL, 
    s_remote_cnt smallint NOT NULL, 
    /*
    *   SSMA informational messages:
    *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
    */

    s_data varchar(50) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL, 
    
    CONSTRAINT [PK_stock_s_w_id] PRIMARY KEY NONCLUSTERED HASH ([s_w_id], [s_i_id]) WITH ( BUCKET_COUNT = 8388608),
    INDEX [fkey_stock_2] NONCLUSTERED HASH ([s_i_id]) WITH ( BUCKET_COUNT = 131072)
    /*CONSTRAINT PK_stock_s_w_id PRIMARY KEY (s_w_id, s_i_id)
     NOT SUPPORTED BY IN-MEMORY DB
    ,  
    CONSTRAINT stock$fkey_stock_1 FOREIGN KEY (s_w_id) REFERENCES dbo.warehouse (w_id), 
    CONSTRAINT stock$fkey_stock_2 FOREIGN KEY (s_i_id) REFERENCES dbo.item (i_id)
    */
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO
/*CREATE NONCLUSTERED INDEX fkey_stock_2
    ON dbo.stock (s_i_id ASC)
GO*/