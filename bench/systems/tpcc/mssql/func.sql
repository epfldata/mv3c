  /*
  *   SSMA informational messages:
  *   M2SS0003: The following SQL clause was ignored during conversion:
  *   DEFINER = `root`@`localhost`.
  */
  CREATE FUNCTION dbo._lastname 
  ( 
      @n int
  )
  /*
  *   SSMA informational messages:
  *   M2SS0055: Data type was converted to VARCHAR according to character set mapping for latin1 character set
  */
  
  RETURNS varchar(10)
  AS
  BEGIN
         /*
          *   SSMA warning messages:
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          *   M2SS0284: Numeric value is converted to string value
          */
  
          RETURN 
              CASE @n
                  WHEN 0 THEN N'BAR'
                  WHEN 1 THEN N'OUGHT'
                  WHEN 2 THEN N'ABLE'
                  WHEN 3 THEN N'PRI'
                  WHEN 4 THEN N'PRES'
                  WHEN 5 THEN N'ESE'
                  WHEN 6 THEN N'ANTI'
                  WHEN 7 THEN N'CALLY'
                  WHEN 8 THEN N'ATION'
                  WHEN 9 THEN N'EING'
              END
    END
    GO