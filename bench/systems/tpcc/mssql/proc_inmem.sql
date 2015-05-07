/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.lastname  
  @byname int  OUTPUT,
  @last nvarchar(30)  OUTPUT
AS
BEGIN
      SET  XACT_ABORT  ON
      SET  NOCOUNT  ON
      SET @last = NULL
      SET @byname = NULL
      DECLARE
          @n int = (CAST(floor(rand() * 8919) AS int) % 17389) % 1000
          SET @byname = 
          CASE 
              WHEN (floor(rand() * 100) < 60) THEN 1
              ELSE 0
          END
          IF (@byname = 1)
          BEGIN
          	SET @last = dbo._lastname(@n * 1.0 / 100) + dbo._lastname((@n * 1.0 / 10) % 10) + dbo._lastname(@n % 10)
          END
END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.delivery  
    @d_w_id int,
    @d_o_carrier_id int,
    @timestamp date
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        DECLARE
            @d_no_o_id int
        DECLARE
            @current_rowid int
        DECLARE
            @d_d_id int
        DECLARE
            @d_c_id int
        DECLARE
            @d_ol_total int
        DECLARE
            @deliv_data nvarchar(100)
        DECLARE
            @loop_counter int
        SET @loop_counter = 1
        WHILE @loop_counter <= 10
        
            BEGIN
                SET @d_d_id = @loop_counter
                SELECT TOP (1) @d_no_o_id = new_orders.no_o_id
                FROM dbo.new_orders
                WHERE new_orders.no_w_id = @d_w_id AND new_orders.no_d_id = @d_d_id
                DELETE 
                  FROM dbo.new_orders
                  WHERE 
                      new_orders.no_w_id = @d_w_id AND 
                      new_orders.no_d_id = @d_d_id AND 
                      new_orders.no_o_id = @d_no_o_id
                SELECT @d_c_id = orders.o_c_id
                FROM dbo.orders
                WHERE 
                    orders.o_id = @d_no_o_id AND 
                    orders.o_d_id = @d_d_id AND 
                    orders.o_w_id = @d_w_id
                UPDATE dbo.orders
                   SET 
                       o_carrier_id = @d_o_carrier_id
                  WHERE 
                      orders.o_id = @d_no_o_id AND 
                      orders.o_d_id = @d_d_id AND 
                      orders.o_w_id = @d_w_id
                UPDATE dbo.order_line
                   SET 
                       ol_delivery_d = @timestamp
                  WHERE 
                      order_line.ol_o_id = @d_no_o_id AND 
                      order_line.ol_d_id = @d_d_id AND 
                      order_line.ol_w_id = @d_w_id
                SELECT @d_ol_total = CAST(ROUND(sum(order_line.ol_amount), 0) AS bigint)
                FROM dbo.order_line
                WHERE 
                    order_line.ol_o_id = @d_no_o_id AND 
                    order_line.ol_d_id = @d_d_id AND 
                    order_line.ol_w_id = @d_w_id
                UPDATE dbo.customer
                   SET 
                       c_balance = customer.c_balance + @d_ol_total
                  WHERE 
                      customer.c_id = @d_c_id AND 
                      customer.c_d_id = @d_d_id AND 
                      customer.c_w_id = @d_w_id
                SET @deliv_data = CAST(@d_d_id AS varchar(50)) + N' ' + CAST(@d_no_o_id AS varchar(50)) + N' ' + CONVERT(varchar(10), @timestamp, 120)
                WHILE @@TRANCOUNT > 0
                
                    COMMIT 
                SET @loop_counter = @loop_counter + 1

            END

    END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.neword  
    @no_w_id int,
    @no_max_w_id int,
    @no_d_id int,
    @no_c_id int,
    @no_o_ol_cnt int,
    @no_c_discount decimal(4, 4)  OUTPUT,
    @no_c_last nvarchar(16)  OUTPUT,
    @no_c_credit nvarchar(2)  OUTPUT,
    @no_d_tax decimal(4, 4)  OUTPUT,
    @no_w_tax decimal(4, 4)  OUTPUT,
    @no_d_next_o_id int  OUTPUT,
    @timestamp date
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        SET @no_d_next_o_id = NULL
        SET @no_w_tax = NULL
        SET @no_d_tax = NULL
        SET @no_c_credit = NULL
        SET @no_c_last = NULL
        SET @no_c_discount = NULL
        DECLARE
            @no_ol_supply_w_id int
        DECLARE
            @no_ol_i_id int
        DECLARE
            @no_ol_quantity int
        DECLARE
            @no_o_all_local int
        DECLARE
            @o_id int
        DECLARE
            @no_i_name nvarchar(24)
        DECLARE
            @no_i_price decimal(5, 2)
        DECLARE
            @no_i_data nvarchar(50)
        DECLARE
            @no_s_quantity decimal(6, 0)
        DECLARE
            @no_ol_amount decimal(6, 2)
        DECLARE
            @no_s_dist_01 nchar(24)
        DECLARE
            @no_s_dist_02 nchar(24)
        DECLARE
            @no_s_dist_03 nchar(24)
        DECLARE
            @no_s_dist_04 nchar(24)
        DECLARE
            @no_s_dist_05 nchar(24)
        DECLARE
            @no_s_dist_06 nchar(24)
        DECLARE
            @no_s_dist_07 nchar(24)
        DECLARE
            @no_s_dist_08 nchar(24)
        DECLARE
            @no_s_dist_09 nchar(24)
        DECLARE
            @no_s_dist_10 nchar(24)
        DECLARE
            @no_ol_dist_info nchar(24)
        DECLARE
            @no_s_data nvarchar(50)
        DECLARE
            @x int
        DECLARE
            @rbk int
        DECLARE
            @loop_counter int
        SET @no_o_all_local = 0
        SELECT @no_c_discount = customer.c_discount, @no_c_last = customer.c_last, @no_c_credit = customer.c_credit, @no_w_tax = warehouse.w_tax
        FROM dbo.customer, dbo.warehouse
        WHERE 
            warehouse.w_id = @no_w_id AND 
            customer.c_w_id = @no_w_id AND 
            customer.c_d_id = @no_d_id AND 
            customer.c_id = @no_c_id
        WHILE @@TRANCOUNT > 0
        
            COMMIT 
        BEGIN TRANSACTION 
        SELECT @no_d_next_o_id = district.d_next_o_id, @no_d_tax = district.d_tax
        FROM dbo.district 
        WHERE district.d_id = @no_d_id AND district.d_w_id = @no_w_id
        UPDATE dbo.district
           SET 
               d_next_o_id = district.d_next_o_id + 1
          WHERE district.d_id = @no_d_id AND district.d_w_id = @no_w_id
        SET @o_id = @no_d_next_o_id
        INSERT dbo.orders(
           dbo.orders.o_id, 
           dbo.orders.o_d_id, 
           dbo.orders.o_w_id, 
           dbo.orders.o_c_id, 
           dbo.orders.o_entry_d, 
           dbo.orders.o_ol_cnt, 
           dbo.orders.o_all_local)
           VALUES (
               @o_id, 
               @no_d_id, 
               @no_w_id, 
               @no_c_id, 
               @timestamp, 
               @no_o_ol_cnt, 
               @no_o_all_local)
        INSERT dbo.new_orders(dbo.new_orders.no_o_id, dbo.new_orders.no_d_id, dbo.new_orders.no_w_id)
           VALUES (@o_id, @no_d_id, @no_w_id)
        SET @rbk = floor(1 + (rand() * 99))
        SET @loop_counter = 1
        WHILE @loop_counter <= @no_o_ol_cnt
        
            BEGIN
                IF ((@loop_counter = @no_o_ol_cnt) AND (@rbk = 1))
                    SET @no_ol_i_id = 100001
                ELSE 
                    SET @no_ol_i_id = floor(1 + (rand() * 100000))
                SET @x = floor(1 + (rand() * 100))
                IF (@x > 1)
                    SET @no_ol_supply_w_id = @no_w_id
                ELSE 
                    BEGIN
                        SET @no_ol_supply_w_id = @no_w_id
                        SET @no_o_all_local = 0
                        WHILE ((@no_ol_supply_w_id = @no_w_id) AND (@no_max_w_id <> 1))
                        
                            BEGIN
                                SET @no_ol_supply_w_id = floor(1 + (rand() * @no_max_w_id))
                            END

                    END
                SET @no_ol_quantity = floor(1 + (rand() * 10))
                SELECT @no_i_price = item.i_price, @no_i_name = item.i_name, @no_i_data = item.i_data
                FROM dbo.item
                WHERE item.i_id = @no_ol_i_id
                SELECT 
                    @no_s_quantity = stock.s_quantity, 
                    @no_s_data = stock.s_data, 
                    @no_s_dist_01 = stock.s_dist_01, 
                    @no_s_dist_02 = stock.s_dist_02, 
                    @no_s_dist_03 = stock.s_dist_03, 
                    @no_s_dist_04 = stock.s_dist_04, 
                    @no_s_dist_05 = stock.s_dist_05, 
                    @no_s_dist_06 = stock.s_dist_06, 
                    @no_s_dist_07 = stock.s_dist_07, 
                    @no_s_dist_08 = stock.s_dist_08, 
                    @no_s_dist_09 = stock.s_dist_09, 
                    @no_s_dist_10 = stock.s_dist_10
                FROM dbo.stock
                WHERE stock.s_i_id = @no_ol_i_id AND stock.s_w_id = @no_ol_supply_w_id
                IF (@no_s_quantity > @no_ol_quantity)
                    SET @no_s_quantity = (@no_s_quantity - @no_ol_quantity)
                ELSE 
                    SET @no_s_quantity = (@no_s_quantity - @no_ol_quantity + 91)
                UPDATE dbo.stock
                   SET 
                       s_quantity = CAST(ROUND(@no_s_quantity, 0) AS bigint)
                  WHERE stock.s_i_id = @no_ol_i_id AND stock.s_w_id = @no_ol_supply_w_id
                SET @no_ol_amount = (@no_ol_quantity * @no_i_price * (1 + @no_w_tax + @no_d_tax) * (1 - @no_c_discount))
                IF @no_d_id = 1
                    SET @no_ol_dist_info = @no_s_dist_01
                ELSE 
                    IF @no_d_id = 2
                        SET @no_ol_dist_info = @no_s_dist_02
                    ELSE 
                        IF @no_d_id = 3
                            SET @no_ol_dist_info = @no_s_dist_03
                        ELSE 
                            IF @no_d_id = 4
                                SET @no_ol_dist_info = @no_s_dist_04
                            ELSE                                  IF @no_d_id = 5                                     SET @no_ol_dist_info = @no_s_dist_05                                 ELSE                                      IF @no_d_id = 6                                         SET @no_ol_dist_info = @no_s_dist_06                                     ELSE                                          IF @no_d_id = 7                                             SET @no_ol_dist_info = @no_s_dist_07                                         ELSE                                              IF @no_d_id = 8                                                 SET @no_ol_dist_info = @no_s_dist_08                                             ELSE                                                  IF @no_d_id = 9                                                     SET @no_ol_dist_info = @no_s_dist_09                                                 ELSE                                                      IF @no_d_id = 10                                                         SET @no_ol_dist_info = @no_s_dist_10
                INSERT dbo.order_line(
                   dbo.order_line.ol_o_id, 
                   dbo.order_line.ol_d_id, 
                   dbo.order_line.ol_w_id, 
                   dbo.order_line.ol_number, 
                   dbo.order_line.ol_i_id, 
                   dbo.order_line.ol_supply_w_id, 
                   dbo.order_line.ol_quantity, 
                   dbo.order_line.ol_amount, 
                   dbo.order_line.ol_dist_info)                    VALUES (                        @o_id,                         @no_d_id,                         @no_w_id,                         @loop_counter,                         @no_ol_i_id,                         @no_ol_supply_w_id,                         @no_ol_quantity,                         @no_ol_amount,                         @no_ol_dist_info)
                SET @loop_counter = @loop_counter + 1

            END
        WHILE @@TRANCOUNT > 0
        
            COMMIT 

    END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.slev  
    @st_w_id int,
    @st_d_id int,
    @threshold int
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        DECLARE
            @st_o_id int
        DECLARE
            @stock_count int
        SELECT @st_o_id = district.d_next_o_id
        FROM dbo.district
        WHERE district.d_w_id = @st_w_id AND district.d_id = @st_d_id
        SELECT @stock_count = count_big(DISTINCT stock.s_i_id)
        FROM dbo.order_line, dbo.stock
        WHERE 
            order_line.ol_w_id = @st_w_id AND 
            order_line.ol_d_id = @st_d_id AND 
            (order_line.ol_o_id < @st_o_id) AND 
            order_line.ol_o_id >= (@st_o_id - 20) AND 
            stock.s_w_id = @st_w_id AND 
            stock.s_i_id = order_line.ol_i_id AND 
            stock.s_quantity < @threshold

    END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.ostat  
    @os_w_id int,
    @os_d_id int,
    @os_c_id int  OUTPUT,
    @byname int,
    @os_c_last nvarchar(16)  OUTPUT,
    @os_c_first nvarchar(16)  OUTPUT,
    @os_c_middle nchar(2)  OUTPUT,
    @os_c_balance decimal(12, 2)  OUTPUT,
    @os_o_id int  OUTPUT,
    @os_entdate date  OUTPUT,
    @os_o_carrier_id int  OUTPUT
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        SET @os_o_carrier_id = NULL
        SET @os_entdate = NULL
        SET @os_o_id = NULL
        SET @os_c_balance = NULL
        SET @os_c_middle = NULL
        SET @os_c_first = NULL
        SET @os_c_last = NULL
        SET @os_c_id = NULL
        DECLARE
            @os_ol_i_id int
        DECLARE
            @os_ol_supply_w_id int
        DECLARE
            @os_ol_quantity int
        DECLARE
            @os_ol_amount int
        DECLARE
            @os_ol_delivery_d date
        DECLARE
            @done int = 0
        DECLARE
            @namecnt int
        DECLARE
            @i int
        DECLARE
            @loop_counter int
        DECLARE
            @no_order_status nvarchar(100)
        DECLARE
            @os_ol_i_id_array nvarchar(200)
        DECLARE
            @os_ol_supply_w_id_array nvarchar(200)
        DECLARE
            @os_ol_quantity_array nvarchar(200)
        DECLARE
            @os_ol_amount_array nvarchar(200)
        DECLARE
            @os_ol_delivery_d_array nvarchar(210)
        SET @no_order_status = N''
        SET @os_ol_i_id_array = N'CSV,'
        SET @os_ol_supply_w_id_array = N'CSV,'
        SET @os_ol_quantity_array = N'CSV,'
        SET @os_ol_amount_array = N'CSV,'
        SET @os_ol_delivery_d_array = N'CSV,'
        IF (@byname = 1)
            BEGIN
                SELECT @namecnt = count_big(customer.c_id)
                FROM dbo.customer
                WHERE 
                    customer.c_last = @os_c_last AND 
                    customer.c_d_id = @os_d_id AND 
                    customer.c_w_id = @os_w_id
                /* 
                *   SSMA error messages:
                *   M2SS0016: Identifier MOD cannot be converted because it was not resolved.

                IF (MOD = 1)
                    SET @namecnt = (@namecnt + 1)
                */
                IF ((@namecnt % 2) = 1)
					SET @namecnt = (@namecnt + 1)

                DECLARE
                     c_name CURSOR LOCAL FORWARD_ONLY FOR 
                        SELECT customer.c_balance, customer.c_first, customer.c_middle, customer.c_id
                        FROM dbo.customer
                        WHERE 
                            customer.c_last = @os_c_last AND 
                            customer.c_d_id = @os_d_id AND 
                            customer.c_w_id = @os_w_id
                            ORDER BY customer.c_first
                OPEN c_name
                SET @loop_counter = 0
                WHILE @loop_counter <= (@namecnt * 1.0 / 2)
                
                    BEGIN
                        FETCH c_name
                             INTO @os_c_balance, @os_c_first, @os_c_middle, @os_c_id
                        IF @@FETCH_STATUS <> 0
                            SET @done = 1
                        SET @loop_counter = @loop_counter + 1

                    END
                CLOSE c_name
                DEALLOCATE c_name

            END
        ELSE 
            SELECT @os_c_balance = customer.c_balance, @os_c_first = customer.c_first, @os_c_middle = customer.c_middle, @os_c_last = customer.c_last
            FROM dbo.customer
            WHERE 
                customer.c_id = @os_c_id AND 
                customer.c_d_id = @os_d_id AND 
                customer.c_w_id = @os_w_id
        SET @done = 0
        SELECT TOP (1) @os_o_id = sb.o_id, @os_o_carrier_id = sb.o_carrier_id, @os_entdate = sb.o_entry_d
        FROM 
            (
                SELECT TOP (9223372036854775807) orders.o_id, orders.o_carrier_id, orders.o_entry_d
                FROM dbo.orders
                WHERE 
                    orders.o_d_id = @os_d_id AND 
                    orders.o_w_id = @os_w_id AND 
                    orders.o_c_id = @os_c_id
                    ORDER BY orders.o_id DESC
            )  AS sb
        IF @done <> 0
            SET @no_order_status = N'No orders for customer'
        SET @done = 0
        SET @i = 0
        DECLARE
             c_line CURSOR LOCAL FORWARD_ONLY FOR 
                SELECT 
                    order_line.ol_i_id, 
                    order_line.ol_supply_w_id, 
                    order_line.ol_quantity, 
                    order_line.ol_amount, 
                    order_line.ol_delivery_d
                FROM dbo.order_line
                WHERE 
                    order_line.ol_o_id = @os_o_id AND 
                    order_line.ol_d_id = @os_d_id AND 
                    order_line.ol_w_id = @os_w_id
        OPEN c_line
        WHILE (1 = 1)
        
            BEGIN
                FETCH c_line
                     INTO 
                        @os_ol_i_id, 
                        @os_ol_supply_w_id, 
                        @os_ol_quantity, 
                        @os_ol_amount, 
                        @os_ol_delivery_d
                IF @@FETCH_STATUS <> 0
                    SET @done = 1
                IF NOT @done <> 0
                    BEGIN
                        SET @os_ol_i_id_array = @os_ol_i_id_array + N',' + CAST(@i AS varchar(50)) + N',' + CAST(@os_ol_i_id AS varchar(50))
                        SET @os_ol_supply_w_id_array = @os_ol_supply_w_id_array + N',' + CAST(@i AS varchar(50)) + N',' + CAST(@os_ol_supply_w_id AS varchar(50))
                        SET @os_ol_quantity_array = @os_ol_quantity_array + N',' + CAST(@i AS varchar(50)) + N',' + CAST(@os_ol_quantity AS varchar(50))
                        SET @os_ol_amount_array = @os_ol_amount_array + N',' + CAST(@i AS varchar(50)) + N',' + CAST(@os_ol_amount AS varchar(50))
                        SET @os_ol_delivery_d_array = @os_ol_delivery_d_array + N',' + CAST(@i AS varchar(50)) + N',' + CONVERT(varchar(10), @os_ol_delivery_d, 120)
                        SET @i = @i + 1

                    END
                IF @done <> 0
                    BREAK

            END
        CLOSE c_line
        DEALLOCATE c_line

    END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.payment  
    @p_w_id int,
    @p_d_id int,
    @p_c_w_id int,
    @p_c_d_id int,
    @p_c_id int  OUTPUT,
    @byname int,
    @p_h_amount decimal(6, 2),
    @p_c_last nvarchar(16)  OUTPUT,
    @p_w_street_1 nvarchar(20)  OUTPUT,
    @p_w_street_2 nvarchar(20)  OUTPUT,
    @p_w_city nvarchar(20)  OUTPUT,
    @p_w_state nchar(2)  OUTPUT,
    @p_w_zip nchar(9)  OUTPUT,
    @p_d_street_1 nvarchar(20)  OUTPUT,
    @p_d_street_2 nvarchar(20)  OUTPUT,
    @p_d_city nvarchar(20)  OUTPUT,
    @p_d_state nchar(2)  OUTPUT,
    @p_d_zip nchar(9)  OUTPUT,
    @p_c_first nvarchar(16)  OUTPUT,
    @p_c_middle nchar(2)  OUTPUT,
    @p_c_street_1 nvarchar(20)  OUTPUT,
    @p_c_street_2 nvarchar(20)  OUTPUT,
    @p_c_city nvarchar(20)  OUTPUT,
    @p_c_state nchar(2)  OUTPUT,
    @p_c_zip nchar(9)  OUTPUT,
    @p_c_phone nchar(16)  OUTPUT,
    @p_c_since date  OUTPUT,
    @p_c_credit nchar(2)  OUTPUT,
    @p_c_credit_lim decimal(12, 2)  OUTPUT,
    @p_c_discount decimal(4, 4)  OUTPUT,
    @p_c_balance decimal(12, 2)  OUTPUT,
    @p_c_data nvarchar(500)  OUTPUT,
    @timestamp date
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        SET @p_c_data = NULL
        SET @p_c_balance = NULL
        SET @p_c_discount = NULL
        SET @p_c_credit_lim = NULL
        SET @p_c_credit = NULL
        SET @p_c_since = NULL
        SET @p_c_phone = NULL
        SET @p_c_zip = NULL
        SET @p_c_state = NULL
        SET @p_c_city = NULL
        SET @p_c_street_2 = NULL
        SET @p_c_street_1 = NULL
        SET @p_c_middle = NULL
        SET @p_c_first = NULL
        SET @p_d_zip = NULL
        SET @p_d_state = NULL
        SET @p_d_city = NULL
        SET @p_d_street_2 = NULL
        SET @p_d_street_1 = NULL
        SET @p_w_zip = NULL
        SET @p_w_state = NULL
        SET @p_w_city = NULL
        SET @p_w_street_2 = NULL
        SET @p_w_street_1 = NULL
        SET @p_c_last = NULL
        SET @p_c_id = NULL
        DECLARE
            @done int = 0
        DECLARE
            @namecnt int
        DECLARE
            @p_d_name nvarchar(11)
        DECLARE
            @p_w_name nvarchar(11)
        DECLARE
            @p_c_new_data nvarchar(500)
        DECLARE
            @h_data nvarchar(30)
        DECLARE
            @loop_counter int
        WHILE @@TRANCOUNT > 0
        
            COMMIT 
        BEGIN TRANSACTION 
        UPDATE dbo.warehouse
           SET 
               w_ytd = warehouse.w_ytd + @p_h_amount
          WHERE warehouse.w_id = @p_w_id
        SELECT 
            @p_w_street_1 = warehouse.w_street_1, 
            @p_w_street_2 = warehouse.w_street_2, 
            @p_w_city = warehouse.w_city, 
            @p_w_state = warehouse.w_state, 
            @p_w_zip = warehouse.w_zip, 
            @p_w_name = warehouse.w_name
        FROM dbo.warehouse
        WHERE warehouse.w_id = @p_w_id
        UPDATE dbo.district
           SET 
               d_ytd = district.d_ytd + @p_h_amount
          WHERE district.d_w_id = @p_w_id AND district.d_id = @p_d_id
        SELECT 
            @p_d_street_1 = district.d_street_1, 
            @p_d_street_2 = district.d_street_2, 
            @p_d_city = district.d_city, 
            @p_d_state = district.d_state, 
            @p_d_zip = district.d_zip, 
            @p_d_name = district.d_name
        FROM dbo.district
        WHERE district.d_w_id = @p_w_id AND district.d_id = @p_d_id
        IF (@byname = 1)
            BEGIN
                SELECT @namecnt = count_big(customer.c_id)
                FROM dbo.customer
                WHERE 
                    customer.c_last = @p_c_last AND 
                    customer.c_d_id = @p_c_d_id AND 
                    customer.c_w_id = @p_c_w_id
                DECLARE
                     c_byname CURSOR LOCAL FORWARD_ONLY FOR 
                        SELECT 
                            customer.c_first, 
                            customer.c_middle, 
                            customer.c_id, 
                            customer.c_street_1, 
                            customer.c_street_2, 
                            customer.c_city, 
                            customer.c_state, 
                            customer.c_zip, 
                            customer.c_phone, 
                            customer.c_credit, 
                            customer.c_credit_lim, 
                            customer.c_discount, 
                            customer.c_balance, 
                            customer.c_since
                        FROM dbo.customer
                        WHERE 
                            customer.c_w_id = @p_c_w_id AND 
                            customer.c_d_id = @p_c_d_id AND 
                            customer.c_last = @p_c_last
                            ORDER BY customer.c_first
                OPEN c_byname
                /* 
                *   SSMA error messages:
                *   M2SS0016: Identifier MOD cannot be converted because it was not resolved.

                IF (MOD = 1)
                    SET @namecnt = (@namecnt + 1)
                */
                IF ((@namecnt % 2) = 1)
					SET @namecnt = (@namecnt + 1)

                SET @loop_counter = 0
                WHILE @loop_counter <= (@namecnt * 1.0 / 2)
                
                    BEGIN
                        FETCH c_byname
                             INTO 
                                @p_c_first, 
                                @p_c_middle, 
                                @p_c_id, 
                                @p_c_street_1, 
                                @p_c_street_2, 
                                @p_c_city, 
                                @p_c_state, 
                                @p_c_zip, 
                                @p_c_phone, 
                                @p_c_credit, 
                                @p_c_credit_lim, 
                                @p_c_discount, 
                                @p_c_balance, 
                                @p_c_since
                        IF @@FETCH_STATUS <> 0
                            SET @done = 1
                        SET @loop_counter = @loop_counter + 1

                    END
                CLOSE c_byname
                DEALLOCATE c_byname

            END
        ELSE 
            SELECT 
                @p_c_first = customer.c_first, 
                @p_c_middle = customer.c_middle, 
                @p_c_last = customer.c_last, 
                @p_c_street_1 = customer.c_street_1, 
                @p_c_street_2 = customer.c_street_2, 
                @p_c_city = customer.c_city, 
                @p_c_state = customer.c_state, 
                @p_c_zip = customer.c_zip, 
                @p_c_phone = customer.c_phone, 
                @p_c_credit = customer.c_credit, 
                @p_c_credit_lim = customer.c_credit_lim, 
                @p_c_discount = customer.c_discount, 
                @p_c_balance = customer.c_balance, 
                @p_c_since = customer.c_since
            FROM dbo.customer
            WHERE 
                customer.c_w_id = @p_c_w_id AND 
                customer.c_d_id = @p_c_d_id AND 
                customer.c_id = @p_c_id
        SET @p_c_balance = (@p_c_balance + @p_h_amount)
        IF @p_c_credit = 'BC'
            BEGIN
                SELECT @p_c_data = customer.c_data
                FROM dbo.customer
                WHERE 
                    customer.c_w_id = @p_c_w_id AND 
                    customer.c_d_id = @p_c_d_id AND 
                    customer.c_id = @p_c_id
                SET @h_data = @p_w_name + N' ' + @p_d_name
                /* 
                *   SSMA error messages:
                *   M2SS0201: MySQL standard function FORMAT is not supported in current SSMA version
                */

                SET @p_c_new_data = 
                    (CAST(@p_c_id AS varchar(50)))
                     + 
                    (N' ')
                     + 
                    (CAST(@p_c_d_id AS varchar(50)))
                     + 
                    (N' ')
                     + 
                    (CAST(@p_c_w_id AS varchar(50)))
                     + 
                    (N' ')
                     + 
                    (CAST(@p_d_id AS varchar(50)))
                     + 
                    (N' ')
                     + 
                    (CAST(@p_w_id AS varchar(50)))
                     + 
                    (N' ')
                     + 
                    (FORMAT(@p_h_amount, 'N', 'en-us'))
                     + 
                    (CONVERT(varchar(10), @timestamp, 120))
                     + 
                    (@h_data)

                SET @p_c_new_data = substring(@p_c_new_data + @p_c_data, 1, 500 - datalength(@p_c_new_data))
                UPDATE dbo.customer
                   SET 
                       c_balance = @p_c_balance, 
                       c_data = @p_c_new_data
                  WHERE 
                      customer.c_w_id = @p_c_w_id AND 
                      customer.c_d_id = @p_c_d_id AND 
                      customer.c_id = @p_c_id

            END
        ELSE 
            UPDATE dbo.customer
               SET 
                   c_balance = @p_c_balance
              WHERE 
                  customer.c_w_id = @p_c_w_id AND 
                  customer.c_d_id = @p_c_d_id AND 
                  customer.c_id = @p_c_id
        SET @h_data = @p_w_name + N' ' + @p_d_name
        INSERT dbo.history(
           dbo.history.h_c_d_id, 
           dbo.history.h_c_w_id, 
           dbo.history.h_c_id, 
           dbo.history.h_d_id, 
           dbo.history.h_w_id, 
           dbo.history.h_date, 
           dbo.history.h_amount, 
           dbo.history.h_data)
           VALUES (
               @p_c_d_id, 
               @p_c_w_id, 
               @p_c_id, 
               @p_d_id, 
               @p_w_id, 
               @timestamp, 
               @p_h_amount, 
               @h_data)
        WHILE @@TRANCOUNT > 0
        
            COMMIT 

    END
GO

/*
*   SSMA informational messages:
*   M2SS0003: The following SQL clause was ignored during conversion:
*   DEFINER = `root`@`localhost`.
*/
CREATE PROCEDURE dbo.[exec]  
    @loops int,
    @count_ware int,
    @new_order_committed int  OUTPUT
AS 
    BEGIN
        SET  XACT_ABORT  ON
        SET  NOCOUNT  ON
        SET @new_order_committed = NULL
        DECLARE
            @prob int
        SET @new_order_committed = 0
        WHILE @loops > 0
        
            BEGIN
                SET @loops = @loops - 1
                SET @prob = floor(rand() * 100)
                IF (@prob < 4)
                    BEGIN
                        DECLARE
                            @c_id int = 1 + (CAST(floor(rand() * 8919) AS int) % 17389) % 3000
                        DECLARE
                            @byname int
                        DECLARE
                            @c_last nvarchar(16)
                        DECLARE
                            @c_first nvarchar(16)
                        DECLARE
                            @c_middle nvarchar(2)
                        DECLARE
                            @c_balance decimal(12, 2)
                        DECLARE
                            @num1 int
                        DECLARE
                            @date1 date
                        DECLARE
                            @num2 int
                        EXECUTE dbo.lastname @byname  OUTPUT, @c_last  OUTPUT
                        DECLARE
                            @temp float(53)
                        SET @temp = 1 + floor(rand() * @count_ware)
                        DECLARE
                            @temp$2 float(53)
                        SET @temp$2 = 1 + floor(rand() * 10)
                        EXECUTE dbo.ostat 
                            @temp, 
                            @temp$2, 
                            @c_id, 
                            @byname, 
                            @c_last, 
                            @c_first  OUTPUT, 
                            @c_middle  OUTPUT, 
                            @c_balance  OUTPUT, 
                            @num1  OUTPUT, 
                            @date1  OUTPUT, 
                            @num2  OUTPUT

                    END
                ELSE 
                    IF (@prob < 8)
                        BEGIN
                            DECLARE
                                @temp$3 float(53)
                            SET @temp$3 = 1 + floor(rand() * @count_ware)
                            DECLARE
                                @temp$4 float(53)
                            SET @temp$4 = 1 + floor(rand() * 10)
                            DECLARE
                                @temp$5 float(53)
                            SET @temp$5 = 10 + floor(rand() * 11)
                            EXECUTE dbo.slev @temp$3, @temp$4, @temp$5

                        END
                    ELSE 
                        IF (@prob < 12)
                            BEGIN
                                DECLARE
                                    @temp$6 float(53)
                                SET @temp$6 = 1 + floor(rand() * @count_ware)
                                DECLARE
                                    @temp$7 float(53)
                                SET @temp$7 = 1 + floor(rand() * 10)
                                DECLARE
                                    @temp$8 datetime
                                SET @temp$8 = getdate()
                                EXECUTE dbo.delivery @temp$6, @temp$7, @temp$8

                            END
                        ELSE 
                            IF (@prob < 55)
                                BEGIN
                                    DECLARE
                                        @ws1 nvarchar(20), 
                                        @ws2 nvarchar(20), 
                                        @wc nvarchar(20), 
                                        @ds1 nvarchar(20), 
                                        @ds2 nvarchar(20), 
                                        @dc nvarchar(20), 
                                        @cs1 nvarchar(20), 
                                        @cs2 nvarchar(20), 
                                        @cc nvarchar(20)
                                    DECLARE
                                        @ws nchar(2), 
                                        @ds nchar(2), 
                                        @cs nchar(2)
                                    DECLARE
                                        @wz nchar(9), 
                                        @dz nchar(9), 
                                        @cz nchar(9)
                                    DECLARE
                                        @w_id int = 1 + floor(rand() * @count_ware)
                                    DECLARE
                                        @d_id int = 1 + floor(rand() * 10)
                                    DECLARE
                                        @c_d_id int = 
                                            CASE 
                                                WHEN (floor(rand() * 100) < 85) THEN @d_id
                                                ELSE 1 + floor(rand() * 10)
                                            END
                                    DECLARE
                                        @c_id$2 int = 1 + (CAST(floor(rand() * 8919) AS int) % 17389) % 3000
                                    DECLARE
                                        @byname$2 int
                                    DECLARE
                                        @c_first$2 nvarchar(16), 
                                        @c_last$2 nvarchar(16)
                                    DECLARE
                                        @c_middle$2 nchar(2), 
                                        @c_credit nchar(2)
                                    DECLARE
                                        @c_phone nchar(16)
                                    DECLARE
                                        @c_since date
                                    DECLARE
                                        @c_credit_lim decimal(12, 2), 
                                        @c_balance$2 decimal(12, 2)
                                    DECLARE
                                        @c_discount decimal(4, 4)
                                    DECLARE
                                        @c_data nvarchar(500)
                                    EXECUTE dbo.lastname @byname$2  OUTPUT, @c_last$2  OUTPUT
                                    DECLARE
                                        @temp$9 float(53)
                                    SET @temp$9 = 5 + floor(rand() * 4996)
                                    DECLARE
                                        @temp$10 datetime
                                    SET @temp$10 = getdate()
                                    EXECUTE dbo.payment 
                                        @w_id, 
                                        @d_id, 
                                        @w_id, 
                                        @c_d_id, 
                                        @c_id$2, 
                                        @byname$2, 
                                        @temp$9, 
                                        @c_last$2, 
                                        @ws1  OUTPUT, 
                                        @ws2  OUTPUT, 
                                        @wc  OUTPUT, 
                                        @ws  OUTPUT, 
                                        @wz  OUTPUT, 
                                        @ds1  OUTPUT, 
                                        @ds2  OUTPUT, 
                                        @dc  OUTPUT, 
                                        @ds  OUTPUT, 
                                        @dz  OUTPUT, 
                                        @c_first$2  OUTPUT, 
                                        @c_middle$2  OUTPUT, 
                                        @cs1  OUTPUT, 
                                        @cs2  OUTPUT, 
                                        @cc  OUTPUT, 
                                        @cs  OUTPUT, 
                                        @cz  OUTPUT, 
                                        @c_phone  OUTPUT, 
                                        @c_since  OUTPUT, 
                                        @c_credit, 
                                        @c_credit_lim  OUTPUT, 
                                        @c_discount  OUTPUT, 
                                        @c_balance$2, 
                                        @c_data  OUTPUT, 
                                        @temp$10

                                END
                            ELSE 
                                BEGIN
                                    DECLARE
                                        @c_discount$2 decimal(4, 4)
                                    DECLARE
                                        @c_last$3 nvarchar(16)
                                    DECLARE
                                        @c_credit$2 nvarchar(2)
                                    DECLARE
                                        @d_tax decimal(4, 4)
                                    DECLARE
                                        @w_tax decimal(4, 4)
                                    DECLARE
                                        @next_o_id int = 1 + (CAST(floor(rand() * 896719) AS bigint) % 1746389) % 99999
                                    DECLARE
                                        @temp$11 float(53)
                                    SET @temp$11 = 1 + floor(rand() * @count_ware)
                                    DECLARE
                                        @temp$12 float(53)
                                    SET @temp$12 = 1 + floor(rand() * 10)
                                    DECLARE
                                        @temp$13 int
                                    SET @temp$13 = 1 + (CAST(floor(rand() * 8919) AS bigint) % 17389) % 3000
                                    DECLARE
                                        @temp$14 float(53)
                                    SET @temp$14 = 5 + floor(rand() * 11)
                                    DECLARE
                                        @temp$15 datetime
                                    SET @temp$15 = getdate()
                                    EXECUTE dbo.neword 
                                        @temp$11, 
                                        @count_ware, 
                                        @temp$12, 
                                        @temp$13, 
                                        @temp$14, 
                                        @c_discount$2  OUTPUT, 
                                        @c_last$3  OUTPUT, 
                                        @c_credit$2  OUTPUT, 
                                        @d_tax  OUTPUT, 
                                        @w_tax  OUTPUT, 
                                        @next_o_id, 
                                        @temp$15
                                    SET @new_order_committed = @new_order_committed + 1

                                END

            END

    END
GO