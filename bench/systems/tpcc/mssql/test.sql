INSERT INTO [dbo].[item]
           ([i_id]
           ,[i_im_id]
           ,[i_name]
           ,[i_price]
           ,[i_data])
     VALUES
           (1
           ,0
           ,'Ab Havij'
           ,20.00
           ,'Nothing special 111')
GO

INSERT INTO [dbo].[item]
           ([i_id]
           ,[i_im_id]
           ,[i_name]
           ,[i_price]
           ,[i_data])
     VALUES
           (2
           ,1
           ,'Shir moz'
           ,30.50
           ,'Nothing special 222')
GO

INSERT INTO [dbo].[item]
           ([i_id]
           ,[i_im_id]
           ,[i_name]
           ,[i_price]
           ,[i_data])
     VALUES
           (3
           ,9
           ,'Nargil'
           ,41.70
           ,'Nothing special 333')
GO

CREATE TYPE [dbo].[itemfiltered] AS TABLE(
 ID int IDENTITY(1,1) PRIMARY KEY NONCLUSTERED HASH WITH (BUCKET_COUNT = 500),
 if_id int NOT NULL, 
 /*if_im_id int NOT NULL, */
 if_name varchar(24) COLLATE Latin1_General_100_BIN2 NOT NULL, 
 if_price decimal(5, 2) NOT NULL, 
 if_data varchar(1000) COLLATE Latin1_General_100_BIN2 NOT NULL
)
WITH ( MEMORY_OPTIMIZED = ON )
GO

/*CREATE TABLE dbo.itemfiltered
(
    if_id int NOT NULL PRIMARY KEY NONCLUSTERED HASH WITH (BUCKET_COUNT = 100000), 
    if_im_id int NOT NULL, 
    if_name varchar(24) COLLATE Latin1_General_100_BIN2 NOT NULL, 
    if_price decimal(5, 2) NOT NULL, 
    if_data varchar(1000) COLLATE Latin1_General_100_BIN2 NOT NULL
) WITH (
  MEMORY_OPTIMIZED = ON, 
  DURABILITY = SCHEMA_ONLY
)
GO*/

CREATE PROCEDURE dbo.testproc  (
  @bynameP1 varchar(500) OUTPUT
) WITH NATIVE_COMPILATION, SCHEMABINDING, EXECUTE AS OWNER
AS
BEGIN
      ATOMIC WITH
      (
        TRANSACTION ISOLATION LEVEL = SNAPSHOT, LANGUAGE = 'us_english'
      )
      /*DECLARE @itemfiltered TABLE
        (
          i_id int, 
          i_im_id int,
          i_name varchar(24), 
          i_price decimal(5, 2)
        )*/
      DECLARE @ifill dbo.itemfiltered

      INSERT INTO @ifill (if_id, if_name, if_price,if_data)
        SELECT i_id, i_name, i_price,i_data
        FROM dbo.item
      
      DECLARE @i int = 1
      DECLARE @tmp int = 0
      SET @bynameP1 = N''
      WHILE @i=(@tmp+1)
        BEGIN
          SELECT @tmp=ID, @bynameP1=(@bynameP1+N'$$'+if_data)
          FROM @ifill
          WHERE ID = @i

          SET @i = @i + 1
        
        END
      /*SET @bynameP1 = @bynameP1 + 1*/

      RETURN 
END
GO

CREATE PROCEDURE dbo.testprocrunner
AS
BEGIN
    DECLARE
        @byname varchar(500)
    EXECUTE dbo.testproc @byname OUTPUT
    PRINT N'Testing....' + @byname
END
GO