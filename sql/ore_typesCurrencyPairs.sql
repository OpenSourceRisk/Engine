use ORE

delete from TypescurrencyPair
INSERT INTO TypesCurrencyPair
SELECT t1.[value]+t2.value currencypair
  FROM [dbo].[TypesCurrencyCode] t1 , [TypesCurrencyCode] t2
  WHERE t1.value <> t2.value
  ORDER BY currencypair