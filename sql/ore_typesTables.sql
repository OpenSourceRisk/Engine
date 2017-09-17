use ORE

IF NOT OBJECT_ID(N'TypesIndexName','U') is null DROP TABLE TypesIndexName
IF NOT OBJECT_ID(N'TypesBool','U') is null DROP TABLE TypesBool
IF NOT OBJECT_ID(N'TypesCurrencyCode','U') is null DROP TABLE TypesCurrencyCode 
IF NOT OBJECT_ID(N'TypesCurrencyPair','U') is null DROP TABLE TypesCurrencyPair
IF NOT OBJECT_ID(N'TypesBusinessDayConvention','U') is null DROP TABLE TypesBusinessDayConvention
IF NOT OBJECT_ID(N'TypesCalendar','U') is null DROP TABLE TypesCalendar
IF NOT OBJECT_ID(N'TypesDayCounter','U') is null DROP TABLE TypesDayCounter
IF NOT OBJECT_ID(N'TypesFrequencyType','U') is null DROP TABLE TypesFrequencyType
IF NOT OBJECT_ID(N'TypesDateRule','U') is null DROP TABLE TypesDateRule
IF NOT OBJECT_ID(N'TypesSubPeriodsCouponType','U') is null DROP TABLE TypesSubPeriodsCouponType
IF NOT OBJECT_ID(N'TypesCompounding','U') is null DROP TABLE TypesCompounding
IF NOT OBJECT_ID(N'TypesInterpolationVariableType','U') is null DROP TABLE TypesInterpolationVariableType
IF NOT OBJECT_ID(N'TypesInterpolationMethodType','U') is null DROP TABLE TypesInterpolationMethodType
IF NOT OBJECT_ID(N'TypesCdsType','U') is null DROP TABLE TypesCdsType
IF NOT OBJECT_ID(N'TypesEquityType','U') is null DROP TABLE TypesEquityType
IF NOT OBJECT_ID(N'TypesDimensionType','U') is null DROP TABLE TypesDimensionType
IF NOT OBJECT_ID(N'TypesVolatilityType','U') is null DROP TABLE TypesVolatilityType
IF NOT OBJECT_ID(N'TypesExtrapolationType','U') is null DROP TABLE TypesExtrapolationType
IF NOT OBJECT_ID(N'TypesLongShort','U') is null DROP TABLE TypesLongShort
IF NOT OBJECT_ID(N'TypesInflationType','U') is null DROP TABLE TypesInflationType


CREATE TABLE TypesIndexName(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesIndexName PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesBool(
	value varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesBool PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesCurrencyCode(
	value varchar(7) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesCurrencyCode PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesCurrencyPair(
	value varchar(7) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesCurrencyPair PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesBusinessDayConvention(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesBusinessDayConvention PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesCalendar(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesCalendar PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesDayCounter(
	value varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesDayCounter PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesFrequencyType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesFrequencyType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesDateRule(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesDateRule PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesSubPeriodsCouponType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesSubPeriodsCouponType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesCompounding(
	value varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesCompounding PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesInterpolationVariableType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesInterpolationVariableType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesInterpolationMethodType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesInterpolationMethodType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesCdsType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesCdsType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesEquityType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesEquityType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesDimensionType(
	value varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesDimensionType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesVolatilityType(
	value varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesVolatilityType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesExtrapolationType(
	value varchar(10) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesExtrapolationType PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesLongShort(
	value varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesLongShort PRIMARY KEY CLUSTERED (
	value ASC
));

CREATE TABLE TypesInflationType(
	value varchar(2) COLLATE Latin1_General_CS_AS NOT NULL,
CONSTRAINT PK_TypesInflationType PRIMARY KEY CLUSTERED (
	value ASC
));

