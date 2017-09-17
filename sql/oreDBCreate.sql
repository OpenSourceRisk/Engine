USE [master]

IF NOT db_id('ORE') is null DROP DATABASE ORE

CREATE DATABASE [ORE]
 ON  PRIMARY 
( NAME = N'ORE', FILENAME = N'C:\Program Files\Microsoft SQL Server\MSSQL13.MSSQLSERVER\MSSQL\DATA\ORE.mdf' )
 LOG ON 
( NAME = N'ORE_log', FILENAME = N'C:\Program Files\Microsoft SQL Server\MSSQL13.MSSQLSERVER\MSSQL\DATA\ORE_log.ldf' )
