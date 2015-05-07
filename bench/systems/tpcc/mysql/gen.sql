DROP PROCEDURE IF EXISTS lastname;
DROP PROCEDURE IF EXISTS exec;
DROP FUNCTION IF EXISTS _lastname;

CREATE FUNCTION _lastname(n INT) RETURNS VARCHAR(10) RETURN CASE n
WHEN 0 THEN "BAR" WHEN 1 THEN "OUGHT" WHEN 2 THEN "ABLE" WHEN 3 THEN "PRI" WHEN 4 THEN "PRES"
WHEN 5 THEN "ESE" WHEN 6 THEN "ANTI" WHEN 7 THEN "CALLY" WHEN 8 THEN "ATION" WHEN 9 THEN "EING" END;

DELIMITER |

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
DELIMITER ;
-- execute
SET @res = 0;
CALL exec(100, 1, @res);
SELECT @res;
