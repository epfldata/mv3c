  CREATE TABLE dbo.warehouse
  (
      w_id smallint NOT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      w_name varchar(10) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      w_street_1 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      w_street_2 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      w_city varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      w_state char(2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      w_zip char(9) NULL DEFAULT NULL, 
      w_tax decimal(4, 2) NULL DEFAULT NULL, 
      w_ytd decimal(12, 2) NULL DEFAULT NULL, 
      CONSTRAINT PK_warehouse_w_id PRIMARY KEY (w_id)
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
  
      d_name varchar(10) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      d_street_1 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      d_street_2 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      d_city varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      d_state char(2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      d_zip char(9) NULL DEFAULT NULL, 
      d_tax decimal(4, 2) NULL DEFAULT NULL, 
      d_ytd decimal(12, 2) NULL DEFAULT NULL, 
      d_next_o_id int NULL DEFAULT NULL, 
      CONSTRAINT PK_district_d_w_id PRIMARY KEY (d_w_id, d_id), 
      CONSTRAINT district$fkey_district_1 FOREIGN KEY (d_w_id) REFERENCES dbo.warehouse (w_id)
  )
  GO

  CREATE TABLE dbo.item
  (
      i_id int NOT NULL, 
      i_im_id int NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      i_name varchar(24) NULL DEFAULT NULL, 
      i_price decimal(5, 2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      i_data varchar(50) NULL DEFAULT NULL, 
      CONSTRAINT PK_item_i_id PRIMARY KEY (i_id)
  )
  GO

  CREATE TABLE dbo.customer
  (
      c_id int NOT NULL, 
      c_d_id smallint NOT NULL, 
      c_w_id smallint NOT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      c_first varchar(16) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      c_middle char(2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      c_last varchar(16) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      c_street_1 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      c_street_2 varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      c_city varchar(20) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      c_state char(2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      c_zip char(9) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      c_phone char(16) NULL DEFAULT NULL, 
      c_since datetime2(0) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      c_credit char(2) NULL DEFAULT NULL, 
      c_credit_lim bigint NULL DEFAULT NULL, 
      c_discount decimal(4, 2) NULL DEFAULT NULL, 
      c_balance decimal(12, 2) NULL DEFAULT NULL, 
      c_ytd_payment decimal(12, 2) NULL DEFAULT NULL, 
      c_payment_cnt smallint NULL DEFAULT NULL, 
      c_delivery_cnt smallint NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR(MAX) according to character set mapping for latin1 character set
      */
  
      c_data varchar(max) NULL, 
      CONSTRAINT PK_customer_c_w_id PRIMARY KEY (c_w_id, c_d_id, c_id), 
      CONSTRAINT customer$fkey_customer_1 FOREIGN KEY (c_w_id, c_d_id) REFERENCES dbo.district (d_w_id, d_id)
  )
  GO
  CREATE NONCLUSTERED INDEX idx_customer
      ON dbo.customer (c_w_id ASC, c_d_id ASC, c_last ASC, c_first ASC)
  GO

  CREATE TABLE dbo.history
  (
      h_c_id int NULL DEFAULT NULL, 
      h_c_d_id smallint NULL DEFAULT NULL, 
      h_c_w_id smallint NULL DEFAULT NULL, 
      h_d_id smallint NULL DEFAULT NULL, 
      h_w_id smallint NULL DEFAULT NULL, 
      h_date datetime2(0) NULL DEFAULT NULL, 
      h_amount decimal(6, 2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      h_data varchar(24) NULL DEFAULT NULL, 
      CONSTRAINT history$fkey_history_2 FOREIGN KEY (h_w_id, h_d_id) REFERENCES dbo.district (d_w_id, d_id)
  )
  GO
  CREATE NONCLUSTERED INDEX fkey_history_2
      ON dbo.history (h_w_id ASC, h_d_id ASC)
  GO

  CREATE TABLE dbo.orders
  (
      o_id int NOT NULL, 
      o_d_id smallint NOT NULL, 
      o_w_id smallint NOT NULL, 
      o_c_id int NULL DEFAULT NULL, 
      o_entry_d datetime2(0) NULL DEFAULT NULL, 
      o_carrier_id smallint NULL DEFAULT NULL, 
      o_ol_cnt smallint NULL DEFAULT NULL, 
      o_all_local smallint NULL DEFAULT NULL, 
      CONSTRAINT PK_orders_o_w_id PRIMARY KEY (o_w_id, o_d_id, o_id), 
      CONSTRAINT orders$fkey_orders_1 FOREIGN KEY (o_w_id, o_d_id, o_c_id) REFERENCES dbo.customer (c_w_id, c_d_id, c_id)
  )
  GO
  CREATE NONCLUSTERED INDEX idx_orders
      ON dbo.orders (o_w_id ASC, o_d_id ASC, o_c_id ASC, o_id ASC)
  GO

  CREATE TABLE dbo.order_line
  (
      ol_o_id int NOT NULL, 
      ol_d_id smallint NOT NULL, 
      ol_w_id smallint NOT NULL, 
      ol_number smallint NOT NULL, 
      ol_i_id int NULL DEFAULT NULL, 
      ol_supply_w_id smallint NULL DEFAULT NULL, 
      ol_delivery_d datetime2(0) NULL DEFAULT NULL, 
      ol_quantity smallint NULL DEFAULT NULL, 
      ol_amount decimal(6, 2) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      ol_dist_info char(24) NULL DEFAULT NULL, 
      CONSTRAINT PK_order_line_ol_w_id PRIMARY KEY (
  ol_w_id, 
  ol_d_id, 
  ol_o_id, 
  ol_number), 
      CONSTRAINT order_line$fkey_order_line_1 FOREIGN KEY (ol_w_id, ol_d_id, ol_o_id) REFERENCES dbo.orders (o_w_id, o_d_id, o_id)
  )
  GO
  CREATE NONCLUSTERED INDEX fkey_order_line_2
      ON dbo.order_line (ol_supply_w_id ASC, ol_i_id ASC)
  GO

  CREATE TABLE dbo.new_orders
  (
      no_o_id int NOT NULL, 
      no_d_id smallint NOT NULL, 
      no_w_id smallint NOT NULL, 
      CONSTRAINT PK_new_orders_no_w_id PRIMARY KEY (no_w_id, no_d_id, no_o_id), 
      CONSTRAINT new_orders$fkey_new_orders_1 FOREIGN KEY (no_w_id, no_d_id, no_o_id) REFERENCES dbo.orders (o_w_id, o_d_id, o_id)
  )
  GO

    CREATE TABLE dbo.stock
  (
      s_i_id int NOT NULL, 
      s_w_id smallint NOT NULL, 
      s_quantity smallint NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_01 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_02 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_03 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_04 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_05 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_06 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_07 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_08 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_09 char(24) NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to CHAR according to character set mapping for latin1 character set
      */
  
      s_dist_10 char(24) NULL DEFAULT NULL, 
      s_ytd decimal(8, 0) NULL DEFAULT NULL, 
      s_order_cnt smallint NULL DEFAULT NULL, 
      s_remote_cnt smallint NULL DEFAULT NULL, 
      /*
      *   SSMA informational messages:
      *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
      */
  
      s_data varchar(50) NULL DEFAULT NULL, 
      CONSTRAINT PK_stock_s_w_id PRIMARY KEY (s_w_id, s_i_id), 
      CONSTRAINT stock$fkey_stock_1 FOREIGN KEY (s_w_id) REFERENCES dbo.warehouse (w_id), 
      CONSTRAINT stock$fkey_stock_2 FOREIGN KEY (s_i_id) REFERENCES dbo.item (i_id)
  )
  GO
  CREATE NONCLUSTERED INDEX fkey_stock_2
      ON dbo.stock (s_i_id ASC)
  GO