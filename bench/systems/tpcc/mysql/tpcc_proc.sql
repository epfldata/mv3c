-- -----------------------------------------------------------------------------
-- Source: http://pastebin.com/pd2K5Py3
-- -----------------------------------------------------------------------------
DROP PROCEDURE IF EXISTS exec;
DROP PROCEDURE IF EXISTS lastname;
DROP FUNCTION IF EXISTS _lastname;

DROP PROCEDURE IF EXISTS delivery;
DROP PROCEDURE IF EXISTS neword;
DROP PROCEDURE IF EXISTS ostat;
DROP PROCEDURE IF EXISTS payment;
DROP PROCEDURE IF EXISTS slev;
-- -----------------------------------------------------------------------------
DELIMITER |
-- -----------------------------------------------------------------------------
CREATE PROCEDURE neword (
no_w_id         INTEGER,
no_max_w_id     INTEGER,
no_d_id         INTEGER,
no_c_id         INTEGER,
no_o_ol_cnt             INTEGER,
OUT no_c_discount       DECIMAL(4,4),
OUT no_c_last           VARCHAR(16),
OUT no_c_credit         VARCHAR(2),
OUT no_d_tax            DECIMAL(4,4),
OUT no_w_tax            DECIMAL(4,4),
INOUT no_d_next_o_id    INTEGER,
IN timestamp            DATE
)
BEGIN
DECLARE no_ol_supply_w_id       INTEGER;
DECLARE no_ol_i_id              INTEGER;
DECLARE no_ol_quantity          INTEGER;
DECLARE no_o_all_local          INTEGER;
DECLARE o_id                    INTEGER;
DECLARE no_i_name               VARCHAR(24);
DECLARE no_i_price              DECIMAL(5,2);
DECLARE no_i_data               VARCHAR(50);
DECLARE no_s_quantity           DECIMAL(6);
DECLARE no_ol_amount            DECIMAL(6,2);
DECLARE no_s_dist_01            CHAR(24);
DECLARE no_s_dist_02            CHAR(24);
DECLARE no_s_dist_03            CHAR(24);
DECLARE no_s_dist_04            CHAR(24);
DECLARE no_s_dist_05            CHAR(24);
DECLARE no_s_dist_06            CHAR(24);
DECLARE no_s_dist_07            CHAR(24);
DECLARE no_s_dist_08            CHAR(24);
DECLARE no_s_dist_09            CHAR(24);
DECLARE no_s_dist_10            CHAR(24);
DECLARE no_ol_dist_info         CHAR(24);
DECLARE no_s_data               VARCHAR(50);
DECLARE x                       INTEGER;
DECLARE rbk                     INTEGER;
DECLARE loop_counter            INT;
DECLARE `Constraint Violation` CONDITION FOR SQLSTATE '23000';
DECLARE EXIT HANDLER FOR `Constraint Violation` ROLLBACK;
DECLARE EXIT HANDLER FOR NOT FOUND ROLLBACK;
SET no_o_all_local = 0;
SELECT c_discount, c_last, c_credit, w_tax
INTO no_c_discount, no_c_last, no_c_credit, no_w_tax
FROM customer, warehouse
WHERE warehouse.w_id = no_w_id AND customer.c_w_id = no_w_id AND
customer.c_d_id = no_d_id AND customer.c_id = no_c_id;
START TRANSACTION;
SELECT d_next_o_id, d_tax INTO no_d_next_o_id, no_d_tax
FROM district
WHERE d_id = no_d_id AND d_w_id = no_w_id FOR UPDATE;
UPDATE district SET d_next_o_id = d_next_o_id + 1 WHERE d_id = no_d_id AND d_w_id = no_w_id;
SET o_id = no_d_next_o_id;
INSERT INTO orders (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local) VALUES (o_id, no_d_id, no_w_id, no_c_id, timestamp, no_o_ol_cnt, no_o_all_local);
INSERT INTO new_orders (no_o_id, no_d_id, no_w_id) VALUES (o_id, no_d_id, no_w_id);
SET rbk = FLOOR(1 + (RAND() * 99));
SET loop_counter = 1;
WHILE loop_counter <= no_o_ol_cnt DO
IF ((loop_counter = no_o_ol_cnt) AND (rbk = 1))
THEN
SET no_ol_i_id = 100001;
ELSE
SET no_ol_i_id = FLOOR(1 + (RAND() * 100000));
END IF;
SET x = FLOOR(1 + (RAND() * 100));
IF ( x > 1 )
THEN
SET no_ol_supply_w_id = no_w_id;
ELSE
SET no_ol_supply_w_id = no_w_id;
SET no_o_all_local = 0;
WHILE ((no_ol_supply_w_id = no_w_id) AND (no_max_w_id != 1)) DO
SET no_ol_supply_w_id = FLOOR(1 + (RAND() * no_max_w_id));
END WHILE;
END IF;
SET no_ol_quantity = FLOOR(1 + (RAND() * 10));
SELECT i_price, i_name, i_data INTO no_i_price, no_i_name, no_i_data
FROM item WHERE i_id = no_ol_i_id;
SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
INTO no_s_quantity, no_s_data, no_s_dist_01, no_s_dist_02, no_s_dist_03, no_s_dist_04, no_s_dist_05, no_s_dist_06, no_s_dist_07, no_s_dist_08, no_s_dist_09, no_s_dist_10
FROM stock WHERE s_i_id = no_ol_i_id AND s_w_id = no_ol_supply_w_id;
IF ( no_s_quantity > no_ol_quantity )
THEN
SET no_s_quantity = ( no_s_quantity - no_ol_quantity );
ELSE
SET no_s_quantity = ( no_s_quantity - no_ol_quantity + 91 );
END IF;
UPDATE stock SET s_quantity = no_s_quantity
WHERE s_i_id = no_ol_i_id
AND s_w_id = no_ol_supply_w_id;
SET no_ol_amount = (  no_ol_quantity * no_i_price * ( 1 + no_w_tax + no_d_tax ) * ( 1 - no_c_discount ) );
CASE no_d_id
WHEN 1 THEN
SET no_ol_dist_info = no_s_dist_01;
WHEN 2 THEN
SET no_ol_dist_info = no_s_dist_02;
WHEN 3 THEN
SET no_ol_dist_info = no_s_dist_03;
WHEN 4 THEN
SET no_ol_dist_info = no_s_dist_04;
WHEN 5 THEN
SET no_ol_dist_info = no_s_dist_05;
WHEN 6 THEN
SET no_ol_dist_info = no_s_dist_06;
WHEN 7 THEN
SET no_ol_dist_info = no_s_dist_07;
WHEN 8 THEN
SET no_ol_dist_info = no_s_dist_08;
WHEN 9 THEN
SET no_ol_dist_info = no_s_dist_09;
WHEN 10 THEN
SET no_ol_dist_info = no_s_dist_10;
END CASE;
INSERT INTO order_line (ol_o_id, ol_d_id, ol_w_id, ol_number, ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_dist_info)
VALUES (o_id, no_d_id, no_w_id, loop_counter, no_ol_i_id, no_ol_supply_w_id, no_ol_quantity, no_ol_amount, no_ol_dist_info);
set loop_counter = loop_counter + 1;
END WHILE;
COMMIT;
END|
-- -----------------------------------------------------------------------------
CREATE PROCEDURE delivery (
d_w_id                  INTEGER,
d_o_carrier_id          INTEGER,
IN timestamp            DATE
)
BEGIN
DECLARE d_no_o_id       INTEGER;
DECLARE current_rowid   INTEGER;
DECLARE d_d_id          INTEGER;
DECLARE d_c_id          INTEGER;
DECLARE d_ol_total      INTEGER;
DECLARE deliv_data      VARCHAR(100);
DECLARE loop_counter    INT;
DECLARE `Constraint Violation` CONDITION FOR SQLSTATE '23000';
DECLARE EXIT HANDLER FOR `Constraint Violation` ROLLBACK;
SET loop_counter = 1;
WHILE loop_counter <= 10 DO
SET d_d_id = loop_counter;
SELECT no_o_id INTO d_no_o_id FROM new_orders WHERE no_w_id = d_w_id AND no_d_id = d_d_id LIMIT 1;
DELETE FROM new_orders WHERE no_w_id = d_w_id AND no_d_id = d_d_id AND no_o_id = d_no_o_id;
SELECT o_c_id INTO d_c_id FROM orders
WHERE o_id = d_no_o_id AND o_d_id = d_d_id AND
o_w_id = d_w_id;
 UPDATE orders SET o_carrier_id = d_o_carrier_id
WHERE o_id = d_no_o_id AND o_d_id = d_d_id AND
o_w_id = d_w_id;
UPDATE order_line SET ol_delivery_d = timestamp
WHERE ol_o_id = d_no_o_id AND ol_d_id = d_d_id AND
ol_w_id = d_w_id;
SELECT SUM(ol_amount) INTO d_ol_total
FROM order_line
WHERE ol_o_id = d_no_o_id AND ol_d_id = d_d_id
AND ol_w_id = d_w_id;
UPDATE customer SET c_balance = c_balance + d_ol_total
WHERE c_id = d_c_id AND c_d_id = d_d_id AND
c_w_id = d_w_id;
SET deliv_data = CONCAT(d_d_id,' ',d_no_o_id,' ',timestamp);
COMMIT;
set loop_counter = loop_counter + 1;
END WHILE;
END|
-- -----------------------------------------------------------------------------
CREATE PROCEDURE payment (
p_w_id                  INTEGER,
p_d_id                  INTEGER,
p_c_w_id                INTEGER,
p_c_d_id                INTEGER,
INOUT p_c_id            INTEGER,
byname                  INTEGER,
p_h_amount              DECIMAL(6,2),
INOUT p_c_last          VARCHAR(16),
OUT p_w_street_1        VARCHAR(20),
OUT p_w_street_2        VARCHAR(20),
OUT p_w_city            VARCHAR(20),
OUT p_w_state           CHAR(2),
OUT p_w_zip             CHAR(9),
OUT p_d_street_1        VARCHAR(20),
OUT p_d_street_2        VARCHAR(20),
OUT p_d_city            VARCHAR(20),
OUT p_d_state           CHAR(2),
OUT p_d_zip             CHAR(9),
OUT p_c_first           VARCHAR(16),
OUT p_c_middle          CHAR(2),
OUT p_c_street_1        VARCHAR(20),
OUT p_c_street_2        VARCHAR(20),
OUT p_c_city            VARCHAR(20),
OUT p_c_state           CHAR(2),
OUT p_c_zip             CHAR(9),
OUT p_c_phone           CHAR(16),
OUT p_c_since           DATE,
INOUT p_c_credit        CHAR(2),
OUT p_c_credit_lim      DECIMAL(12,2),
OUT p_c_discount        DECIMAL(4,4),
INOUT p_c_balance       DECIMAL(12,2),
OUT p_c_data            VARCHAR(500),
IN timestamp            DATE
)
BEGIN
DECLARE done            INT DEFAULT 0;
DECLARE namecnt         INTEGER;
DECLARE p_d_name        VARCHAR(11);
DECLARE p_w_name        VARCHAR(11);
DECLARE p_c_new_data    VARCHAR(500);
DECLARE h_data          VARCHAR(30);
DECLARE loop_counter    INT;
DECLARE `Constraint Violation` CONDITION FOR SQLSTATE '23000';
DECLARE c_byname CURSOR FOR
SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
FROM customer
WHERE c_w_id = p_c_w_id AND c_d_id = p_c_d_id AND c_last = p_c_last
ORDER BY c_first;
DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;
DECLARE EXIT HANDLER FOR `Constraint Violation` ROLLBACK;
START TRANSACTION;
UPDATE warehouse SET w_ytd = w_ytd + p_h_amount
WHERE w_id = p_w_id;
SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
INTO p_w_street_1, p_w_street_2, p_w_city, p_w_state, p_w_zip, p_w_name
FROM warehouse
WHERE w_id = p_w_id;
UPDATE district SET d_ytd = d_ytd + p_h_amount
WHERE d_w_id = p_w_id AND d_id = p_d_id;
SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
INTO p_d_street_1, p_d_street_2, p_d_city, p_d_state, p_d_zip, p_d_name
FROM district
WHERE d_w_id = p_w_id AND d_id = p_d_id;
IF (byname = 1)
THEN
SELECT count(c_id) INTO namecnt
FROM customer
WHERE c_last = p_c_last AND c_d_id = p_c_d_id AND c_w_id = p_c_w_id;
OPEN c_byname;
IF ( MOD (namecnt, 2) = 1 )
THEN
SET namecnt = (namecnt + 1);
END IF;
SET loop_counter = 0;
WHILE loop_counter <= (namecnt/2) DO
FETCH c_byname
INTO p_c_first, p_c_middle, p_c_id, p_c_street_1, p_c_street_2, p_c_city,
p_c_state, p_c_zip, p_c_phone, p_c_credit, p_c_credit_lim, p_c_discount, p_c_balance, p_c_since;
set loop_counter = loop_counter + 1;
END WHILE;
CLOSE c_byname;
ELSE
SELECT c_first, c_middle, c_last,
c_street_1, c_street_2, c_city, c_state, c_zip,
c_phone, c_credit, c_credit_lim,
c_discount, c_balance, c_since
INTO p_c_first, p_c_middle, p_c_last,
p_c_street_1, p_c_street_2, p_c_city, p_c_state, p_c_zip,
p_c_phone, p_c_credit, p_c_credit_lim,
p_c_discount, p_c_balance, p_c_since
FROM customer
WHERE c_w_id = p_c_w_id AND c_d_id = p_c_d_id AND c_id = p_c_id;
END IF;
SET p_c_balance = ( p_c_balance + p_h_amount );
IF p_c_credit = 'BC'
THEN
 SELECT c_data INTO p_c_data
FROM customer
WHERE c_w_id = p_c_w_id AND c_d_id = p_c_d_id AND c_id = p_c_id;
SET h_data = CONCAT(p_w_name,' ',p_d_name);
SET p_c_new_data = CONCAT(CAST(p_c_id AS CHAR),' ',CAST(p_c_d_id AS CHAR),' ',CAST(p_c_w_id AS CHAR),' ',CAST(p_d_id AS CHAR),' ',CAST(p_w_id AS CHAR),' ',CAST(FORMAT(p_h_amount,2) AS CHAR),CAST(timestamp AS CHAR),h_data);
SET p_c_new_data = SUBSTR(CONCAT(p_c_new_data,p_c_data),1,500-(LENGTH(p_c_new_data)));
UPDATE customer
SET c_balance = p_c_balance, c_data = p_c_new_data
WHERE c_w_id = p_c_w_id AND c_d_id = p_c_d_id AND
c_id = p_c_id;
ELSE
UPDATE customer SET c_balance = p_c_balance
WHERE c_w_id = p_c_w_id AND c_d_id = p_c_d_id AND
c_id = p_c_id;
END IF;
SET h_data = CONCAT(p_w_name,' ',p_d_name);
INSERT INTO history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
VALUES (p_c_d_id, p_c_w_id, p_c_id, p_d_id, p_w_id, timestamp, p_h_amount, h_data);
COMMIT;
END|
-- -----------------------------------------------------------------------------
CREATE PROCEDURE ostat (
os_w_id                 INTEGER,
os_d_id                 INTEGER,
INOUT os_c_id           INTEGER,
byname                  INTEGER,
INOUT os_c_last         VARCHAR(16),
OUT os_c_first          VARCHAR(16),
OUT os_c_middle         CHAR(2),
OUT os_c_balance        DECIMAL(12,2),
OUT os_o_id             INTEGER,
OUT os_entdate          DATE,
OUT os_o_carrier_id     INTEGER
)
BEGIN
DECLARE  os_ol_i_id     INTEGER;
DECLARE  os_ol_supply_w_id INTEGER;
DECLARE  os_ol_quantity INTEGER;
DECLARE  os_ol_amount   INTEGER;
DECLARE  os_ol_delivery_d       DATE;
DECLARE done            INT DEFAULT 0;
DECLARE namecnt         INTEGER;
DECLARE i               INTEGER;
DECLARE loop_counter    INT;
DECLARE no_order_status VARCHAR(100);
DECLARE os_ol_i_id_array VARCHAR(200);
DECLARE os_ol_supply_w_id_array VARCHAR(200);
DECLARE os_ol_quantity_array VARCHAR(200);
DECLARE os_ol_amount_array VARCHAR(200);
DECLARE os_ol_delivery_d_array VARCHAR(210);
DECLARE `Constraint Violation` CONDITION FOR SQLSTATE '23000';
DECLARE c_name CURSOR FOR
SELECT c_balance, c_first, c_middle, c_id
FROM customer
WHERE c_last = os_c_last AND c_d_id = os_d_id AND c_w_id = os_w_id
ORDER BY c_first;
DECLARE c_line CURSOR FOR
SELECT ol_i_id, ol_supply_w_id, ol_quantity,
ol_amount, ol_delivery_d
FROM order_line
WHERE ol_o_id = os_o_id AND ol_d_id = os_d_id AND ol_w_id = os_w_id;
DECLARE EXIT HANDLER FOR `Constraint Violation` ROLLBACK;
DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;
set no_order_status = '';
set os_ol_i_id_array = 'CSV,';
set os_ol_supply_w_id_array = 'CSV,';
set os_ol_quantity_array = 'CSV,';
set os_ol_amount_array = 'CSV,';
set os_ol_delivery_d_array = 'CSV,';
IF ( byname = 1 )
THEN
SELECT count(c_id) INTO namecnt
FROM customer
WHERE c_last = os_c_last AND c_d_id = os_d_id AND c_w_id = os_w_id;
IF ( MOD (namecnt, 2) = 1 )
THEN
SET namecnt = (namecnt + 1);
END IF;
OPEN c_name;
SET loop_counter = 0;
WHILE loop_counter <= (namecnt/2) DO
FETCH c_name
INTO os_c_balance, os_c_first, os_c_middle, os_c_id;
set loop_counter = loop_counter + 1;
END WHILE;
close c_name;
ELSE
SELECT c_balance, c_first, c_middle, c_last
INTO os_c_balance, os_c_first, os_c_middle, os_c_last
FROM customer
WHERE c_id = os_c_id AND c_d_id = os_d_id AND c_w_id = os_w_id;
END IF;
set done = 0;
SELECT o_id, o_carrier_id, o_entry_d
INTO os_o_id, os_o_carrier_id, os_entdate
FROM
(SELECT o_id, o_carrier_id, o_entry_d
FROM orders where o_d_id = os_d_id AND o_w_id = os_w_id and o_c_id = os_c_id
ORDER BY o_id DESC) AS sb LIMIT 1;
IF done THEN
set no_order_status = 'No orders for customer';
END IF;
set done = 0;
set i = 0;
OPEN c_line;
REPEAT
FETCH c_line INTO os_ol_i_id, os_ol_supply_w_id, os_ol_quantity, os_ol_amount, os_ol_delivery_d;
IF NOT done THEN
set os_ol_i_id_array = CONCAT(os_ol_i_id_array,',',CAST(i AS CHAR),',',CAST(os_ol_i_id AS CHAR));
set os_ol_supply_w_id_array = CONCAT(os_ol_supply_w_id_array,',',CAST(i AS CHAR),',',CAST(os_ol_supply_w_id AS CHAR));
set os_ol_quantity_array = CONCAT(os_ol_quantity_array,',',CAST(i AS CHAR),',',CAST(os_ol_quantity AS CHAR));
set os_ol_amount_array = CONCAT(os_ol_amount_array,',',CAST(i AS CHAR),',',CAST(os_ol_amount AS CHAR));
set os_ol_delivery_d_array = CONCAT(os_ol_delivery_d_array,',',CAST(i AS CHAR),',',CAST(os_ol_delivery_d AS CHAR));
set i = i+1;
END IF;
UNTIL done END REPEAT;
CLOSE c_line;
END|
-- -----------------------------------------------------------------------------
CREATE PROCEDURE slev (
st_w_id                 INTEGER,
st_d_id                 INTEGER,
threshold               INTEGER
)
BEGIN
DECLARE st_o_id         INTEGER;
DECLARE stock_count     INTEGER;
DECLARE `Constraint Violation` CONDITION FOR SQLSTATE '23000';
DECLARE EXIT HANDLER FOR `Constraint Violation` ROLLBACK;
DECLARE EXIT HANDLER FOR NOT FOUND ROLLBACK;
SELECT d_next_o_id INTO st_o_id
FROM district
WHERE d_w_id=st_w_id AND d_id=st_d_id;
SELECT COUNT(DISTINCT (s_i_id)) INTO stock_count
FROM order_line, stock
WHERE ol_w_id = st_w_id AND
ol_d_id = st_d_id AND (ol_o_id < st_o_id) AND
ol_o_id >= (st_o_id - 20) AND s_w_id = st_w_id AND
s_i_id = ol_i_id AND s_quantity < threshold;
END|
-- -----------------------------------------------------------------------------
CREATE FUNCTION _lastname(n INT) RETURNS VARCHAR(10) RETURN CASE n
WHEN 0 THEN "BAR" WHEN 1 THEN "OUGHT" WHEN 2 THEN "ABLE" WHEN 3 THEN "PRI" WHEN 4 THEN "PRES"
WHEN 5 THEN "ESE" WHEN 6 THEN "ANTI" WHEN 7 THEN "CALLY" WHEN 8 THEN "ATION" WHEN 9 THEN "EING" END|

CREATE PROCEDURE lastname(OUT byname INT, OUT last VARCHAR(30))
BEGIN
  DECLARE n INTEGER DEFAULT (FLOOR(RAND()*8919)%17389)%1000;
    SET byname := IF(FLOOR(RAND()*100) < 60, 1, 0);
  IF (byname = 1) THEN BEGIN
    SET last := CONCAT(_lastname(n/100),_lastname((n/10)%10),_lastname(n%10));
  END; END IF;
END|

CREATE PROCEDURE exec(IN loops INT, IN count_ware INT, OUT new_order_committed INT)
BEGIN
  DECLARE prob INTEGER;
  SET new_order_committed := 0;
  WHILE loops > 0 DO
    SET loops := loops - 1;
    SET prob := FLOOR(RAND()*100);
    IF (prob < 4) THEN BEGIN
      DECLARE c_id INTEGER DEFAULT 1+(FLOOR(RAND()*8919)%17389)%3000;
      DECLARE byname INTEGER;
      DECLARE c_last VARCHAR(16);
      DECLARE c_first VARCHAR(16);
      DECLARE c_middle VARCHAR(2);
      DECLARE c_balance DECIMAL(12,2);
      DECLARE num1 INTEGER;
      DECLARE date1 DATE;
      DECLARE num2 INTEGER;
      CALL lastname(byname, c_last);
      CALL ostat(1+FLOOR(RAND()*count_ware), 1+FLOOR(RAND()*10), c_id, byname, c_last, c_first, c_middle, c_balance, num1, date1, num2);
    END;
    ELSEIF (prob < 8) THEN CALL slev(1+FLOOR(RAND()*count_ware), 1+FLOOR(RAND()*10), 10+FLOOR(RAND()*11));
    ELSEIF (prob < 12) THEN CALL delivery(1+FLOOR(RAND()*count_ware), 1+FLOOR(RAND()*10), NOW());
    ELSEIF (prob < 55) THEN BEGIN
      DECLARE ws1, ws2, wc, ds1, ds2, dc, cs1, cs2, cc VARCHAR(20);
      DECLARE ws, ds, cs CHAR(2);
      DECLARE wz, dz, cz CHAR(9);
      DECLARE w_id INTEGER DEFAULT 1+FLOOR(RAND()*count_ware);
      DECLARE d_id INTEGER DEFAULT 1+FLOOR(RAND()*10);
      DECLARE c_d_id INTEGER DEFAULT IF(FLOOR(RAND()*100)<85,d_id,1+FLOOR(RAND()*10));
      DECLARE c_id INTEGER DEFAULT 1+(FLOOR(RAND()*8919)%17389)%3000;
      DECLARE byname INTEGER;
      DECLARE c_first, c_last VARCHAR(16);
      DECLARE c_middle, c_credit CHAR(2);
      DECLARE c_phone CHAR(16);
      DECLARE c_since DATE;
      DECLARE c_credit_lim, c_balance DECIMAL(12,2);
      DECLARE c_discount DECIMAL(4,4);
      DECLARE c_data VARCHAR(500);
      CALL lastname(byname, c_last);
      CALL payment(w_id, d_id, w_id, c_d_id, c_id, byname, 5+FLOOR(RAND()*4996), c_last,
                   ws1, ws2, wc, ws, wz, ds1, ds2, dc, ds, dz, c_first, c_middle, cs1, cs2, cc, cs, cz,
                   c_phone, c_since, c_credit, c_credit_lim, c_discount, c_balance, c_data, NOW());
    END; ELSE BEGIN
      DECLARE c_discount DECIMAL(4,4);
      DECLARE c_last VARCHAR(16);
      DECLARE c_credit VARCHAR(2);
      DECLARE d_tax DECIMAL(4,4);
      DECLARE w_tax DECIMAL(4,4);
      DECLARE next_o_id INTEGER DEFAULT 1+(FLOOR(RAND()*896719)%1746389)%99999;
      CALL neword (1+FLOOR(RAND()*count_ware), count_ware, 1+FLOOR(RAND()*10), 1+(FLOOR(RAND()*8919)%17389)%3000, 5+FLOOR(RAND()*11),
                   c_discount, c_last, c_credit, d_tax, w_tax, next_o_id, NOW());
      SET new_order_committed := new_order_committed + 1;
    END; END IF;
  END WHILE;
END|
-- -----------------------------------------------------------------------------
DELIMITER ;
-- -----------------------------------------------------------------------------
-- execute by using the following commands:
-- SET @res = 0;
-- CALL exec(100, 1, @res);
-- SELECT @res;
