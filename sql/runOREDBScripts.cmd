set SQLCMD=sqlcmd -S LENOVO-PC -Usa -Possaps -i
%SQLCMD% oreDBCreate.sql
%SQLCMD% ore_typesTables.sql
%SQLCMD% ore_types.sql
%SQLCMD% ore_typesIndexes.sql
%SQLCMD% ore_typesCurrencyPairs.sql
%SQLCMD% ore_typesOther.sql
%SQLCMD% pricingengineTables.sql
%SQLCMD% pricingengine.sql
%SQLCMD% conventionsTables.sql
%SQLCMD% conventions.sql
pause