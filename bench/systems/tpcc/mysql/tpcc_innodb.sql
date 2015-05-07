DROP TABLE IF EXISTS history;
DROP TABLE IF EXISTS new_orders;
DROP TABLE IF EXISTS order_line;

DROP TABLE IF EXISTS stock;
DROP TABLE IF EXISTS item;

DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS customer;
DROP TABLE IF EXISTS district;
DROP TABLE IF EXISTS warehouse;

-- -----------------------------------------------------------------------------

CREATE TABLE warehouse (
  w_id smallint(6) NOT NULL,
  w_name varchar(10),
  w_street_1 varchar(20),
  w_street_2 varchar(20),
  w_city varchar(20),
  w_state char(2),
  w_zip char(9),
  w_tax decimal(4,2),
  w_ytd decimal(12,2),
  PRIMARY KEY (w_id)
) ENGINE=InnoDB;

CREATE TABLE district (
  d_id tinyint(4) NOT NULL,
  d_w_id smallint(6) NOT NULL,
  d_name varchar(10),
  d_street_1 varchar(20),
  d_street_2 varchar(20),
  d_city varchar(20),
  d_state char(2),
  d_zip char(9),
  d_tax decimal(4,2),
  d_ytd decimal(12,2),
  d_next_o_id int(11),
  PRIMARY KEY (d_w_id,d_id),
  CONSTRAINT fkey_district_1 FOREIGN KEY (d_w_id) REFERENCES warehouse (w_id)
) ENGINE=InnoDB;

CREATE TABLE customer (
  c_id int(11) NOT NULL,
  c_d_id tinyint(4) NOT NULL,
  c_w_id smallint(6) NOT NULL,
  c_first varchar(16),
  c_middle char(2),
  c_last varchar(16),
  c_street_1 varchar(20),
  c_street_2 varchar(20),
  c_city varchar(20),
  c_state char(2),
  c_zip char(9),
  c_phone char(16),
  c_since datetime,
  c_credit char(2),
  c_credit_lim bigint(20),
  c_discount decimal(4,2),
  c_balance decimal(12,2),
  c_ytd_payment decimal(12,2),
  c_payment_cnt smallint(6),
  c_delivery_cnt smallint(6),
  c_data text,
  PRIMARY KEY (c_w_id,c_d_id,c_id),
  KEY idx_customer (c_w_id,c_d_id,c_last,c_first),
  CONSTRAINT fkey_customer_1 FOREIGN KEY (c_w_id, c_d_id) REFERENCES district (d_w_id, d_id)
) ENGINE=InnoDB;

CREATE TABLE orders (
  o_id int(11) NOT NULL,
  o_d_id tinyint(4) NOT NULL,
  o_w_id smallint(6) NOT NULL,
  o_c_id int(11),
  o_entry_d datetime,
  o_carrier_id tinyint(4),
  o_ol_cnt tinyint(4),
  o_all_local tinyint(4),
  PRIMARY KEY (o_w_id,o_d_id,o_id),
  KEY idx_orders (o_w_id,o_d_id,o_c_id,o_id),
  CONSTRAINT fkey_orders_1 FOREIGN KEY (o_w_id, o_d_id, o_c_id) REFERENCES customer (c_w_id, c_d_id, c_id)
) ENGINE=InnoDB;


CREATE TABLE item (
  i_id int(11) NOT NULL,
  i_im_id int(11),
  i_name varchar(24),
  i_price decimal(5,2),
  i_data varchar(50),
  PRIMARY KEY (i_id)
) ENGINE=InnoDB;

CREATE TABLE stock (
  s_i_id int(11) NOT NULL,
  s_w_id smallint(6) NOT NULL,
  s_quantity smallint(6),
  s_dist_01 char(24),
  s_dist_02 char(24),
  s_dist_03 char(24),
  s_dist_04 char(24),
  s_dist_05 char(24),
  s_dist_06 char(24),
  s_dist_07 char(24),
  s_dist_08 char(24),
  s_dist_09 char(24),
  s_dist_10 char(24),
  s_ytd decimal(8,0),
  s_order_cnt smallint(6),
  s_remote_cnt smallint(6),
  s_data varchar(50),
  PRIMARY KEY (s_w_id,s_i_id),
  KEY fkey_stock_2 (s_i_id),
  CONSTRAINT fkey_stock_1 FOREIGN KEY (s_w_id) REFERENCES warehouse (w_id),
  CONSTRAINT fkey_stock_2 FOREIGN KEY (s_i_id) REFERENCES item (i_id)
) ENGINE=InnoDB;


CREATE TABLE history (
  h_c_id int(11),
  h_c_d_id tinyint(4),
  h_c_w_id smallint(6),
  h_d_id tinyint(4),
  h_w_id smallint(6),
  h_date datetime,
  h_amount decimal(6,2),
  h_data varchar(24),
  KEY fkey_history_2 (h_w_id,h_d_id),
  CONSTRAINT fkey_history_2 FOREIGN KEY (h_w_id, h_d_id) REFERENCES district (d_w_id, d_id)
) ENGINE=InnoDB;


CREATE TABLE new_orders (
  no_o_id int(11) NOT NULL,
  no_d_id tinyint(4) NOT NULL,
  no_w_id smallint(6) NOT NULL,
  PRIMARY KEY (no_w_id,no_d_id,no_o_id),
  CONSTRAINT fkey_new_orders_1 FOREIGN KEY (no_w_id, no_d_id, no_o_id) REFERENCES orders (o_w_id, o_d_id, o_id)
) ENGINE=InnoDB;

CREATE TABLE order_line (
  ol_o_id int(11) NOT NULL,
  ol_d_id tinyint(4) NOT NULL,
  ol_w_id smallint(6) NOT NULL,
  ol_number tinyint(4) NOT NULL,
  ol_i_id int(11),
  ol_supply_w_id smallint(6),
  ol_delivery_d datetime,
  ol_quantity tinyint(4),
  ol_amount decimal(6,2),
  ol_dist_info char(24),
  PRIMARY KEY (ol_w_id,ol_d_id,ol_o_id,ol_number),
  KEY fkey_order_line_2 (ol_supply_w_id,ol_i_id),
  CONSTRAINT fkey_order_line_1 FOREIGN KEY (ol_w_id, ol_d_id, ol_o_id) REFERENCES orders (o_w_id, o_d_id, o_id)
) ENGINE=InnoDB;
