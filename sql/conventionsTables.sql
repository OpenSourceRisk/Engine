use ORE

IF NOT OBJECT_ID(N'ConventionsZero ','U') is null DROP TABLE ConventionsZero 
IF NOT OBJECT_ID(N'ConventionsCDS ','U') is null DROP TABLE ConventionsCDS 
IF NOT OBJECT_ID(N'ConventionsDeposit ','U') is null DROP TABLE ConventionsDeposit 
IF NOT OBJECT_ID(N'ConventionsFuture ','U') is null DROP TABLE ConventionsFuture 
IF NOT OBJECT_ID(N'ConventionsFRA ','U') is null DROP TABLE ConventionsFRA 
IF NOT OBJECT_ID(N'ConventionsOIS ','U') is null DROP TABLE ConventionsOIS 
IF NOT OBJECT_ID(N'ConventionsSwap ','U') is null DROP TABLE ConventionsSwap 
IF NOT OBJECT_ID(N'ConventionsAverageOIS ','U') is null DROP TABLE ConventionsAverageOIS 
IF NOT OBJECT_ID(N'ConventionsTenorBasisSwap ','U') is null DROP TABLE ConventionsTenorBasisSwap 
IF NOT OBJECT_ID(N'ConventionsTenorBasisTwoSwap ','U') is null DROP TABLE ConventionsTenorBasisTwoSwap 
IF NOT OBJECT_ID(N'ConventionsFX ','U') is null DROP TABLE ConventionsFX 
IF NOT OBJECT_ID(N'ConventionsCrossCurrencyBasis ','U') is null DROP TABLE ConventionsCrossCurrencyBasis 
IF NOT OBJECT_ID(N'ConventionsSwapIndex ','U') is null DROP TABLE ConventionsSwapIndex 
IF NOT OBJECT_ID(N'ConventionsInflationSwap ','U') is null DROP TABLE ConventionsInflationSwap 

CREATE TABLE ConventionsZero (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	TenorBased varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
	DayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	Compounding varchar(30) COLLATE Latin1_General_CS_AS,
	CompoundingFrequency varchar(20) COLLATE Latin1_General_CS_AS,
	TenorCalendar varchar(20) COLLATE Latin1_General_CS_AS,
	SpotLag int,
	SpotCalendar varchar(20) COLLATE Latin1_General_CS_AS,
	RollConvention varchar(20) COLLATE Latin1_General_CS_AS,
	EOM varchar(5) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsZero PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_TenorBased FOREIGN KEY(TenorBased)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_DayCounter FOREIGN KEY(DayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_Compounding FOREIGN KEY(Compounding)
REFERENCES TypesCompounding (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_CompoundingFrequency FOREIGN KEY(CompoundingFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_TenorCalendar FOREIGN KEY(TenorCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_SpotCalendar FOREIGN KEY(SpotCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_RollConvention FOREIGN KEY(RollConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsZero WITH CHECK ADD CONSTRAINT FK_ConventionsZero_EOM FOREIGN KEY(EOM)
REFERENCES TypesBool (value)

CREATE TABLE ConventionsCDS (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	SettlementDays int NOT NULL,
	Calendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	Frequency varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	PaymentConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	RuleName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	DayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	SettlesAccrual varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
	PaysAtDefaultTime varchar(5) COLLATE Latin1_General_CS_AS  NOT NULL
CONSTRAINT PK_ConventionsCDS PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_Calendar FOREIGN KEY(Calendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_Frequency FOREIGN KEY(Frequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_PaymentConvention FOREIGN KEY(PaymentConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_RuleName FOREIGN KEY(RuleName)
REFERENCES TypesDateRule (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_DayCounter FOREIGN KEY(DayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_SettlesAccrual FOREIGN KEY(SettlesAccrual)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsCDS WITH CHECK ADD CONSTRAINT FK_ConventionsCDS_PaysAtDefaultTime FOREIGN KEY(PaysAtDefaultTime)
REFERENCES TypesBool (value)

CREATE TABLE ConventionsDeposit (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexBased varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS,
	Calendar varchar(20) COLLATE Latin1_General_CS_AS,
	Convention varchar(20) COLLATE Latin1_General_CS_AS,
	EOM varchar(5) COLLATE Latin1_General_CS_AS,
	DayCounter varchar(30) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsDeposit PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_IndexBased FOREIGN KEY(IndexBased)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_Calendar FOREIGN KEY(Calendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_Convention FOREIGN KEY(Convention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_EOM FOREIGN KEY(EOM)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsDeposit WITH CHECK ADD CONSTRAINT FK_ConventionsDeposit_DayCounter FOREIGN KEY(DayCounter)
REFERENCES TypesDayCounter (value)

CREATE TABLE ConventionsFuture (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL
CONSTRAINT PK_ConventionsFuture PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsFuture WITH CHECK ADD CONSTRAINT FK_ConventionsFuture_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)

CREATE TABLE ConventionsFRA (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL
CONSTRAINT PK_ConventionsFRA PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsFRA WITH CHECK ADD CONSTRAINT FK_ConventionsFRA_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)

CREATE TABLE ConventionsOIS (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	SpotLag int NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedDayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	PaymentLag int,
	EOM varchar(5) COLLATE Latin1_General_CS_AS,
	FixedFrequency varchar(20) COLLATE Latin1_General_CS_AS,
	FixedConvention varchar(20) COLLATE Latin1_General_CS_AS,
	FixedPaymentConvention varchar(20) COLLATE Latin1_General_CS_AS,
	RuleName varchar(20) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsOIS PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_FixedDayCounter FOREIGN KEY(FixedDayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_EOM FOREIGN KEY(EOM)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_FixedFrequency FOREIGN KEY(FixedFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_FixedConvention FOREIGN KEY(FixedConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_FixedPaymentConvention FOREIGN KEY(FixedPaymentConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsOIS WITH CHECK ADD CONSTRAINT FK_ConventionsOIS_RuleName FOREIGN KEY(RuleName)
REFERENCES TypesDateRule (value)

CREATE TABLE ConventionsSwap (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedCalendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedFrequency varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedDayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FloatFrequency varchar(20) COLLATE Latin1_General_CS_AS,
	SubPeriodsCouponType varchar(20) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsSwap PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_FixedCalendar FOREIGN KEY(FixedCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_FixedFrequency FOREIGN KEY(FixedFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_FixedConvention FOREIGN KEY(FixedConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_FixedDayCounter FOREIGN KEY(FixedDayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_FloatFrequency FOREIGN KEY(FloatFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsSwap WITH CHECK ADD CONSTRAINT FK_ConventionsSwap_SubPeriodsCouponType FOREIGN KEY(SubPeriodsCouponType)
REFERENCES TypesSubPeriodsCouponType (value)

CREATE TABLE ConventionsAverageOIS (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	SpotLag int NOT NULL,
	FixedTenor varchar(8) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedDayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedCalendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixedPaymentConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	OnTenor varchar(8) COLLATE Latin1_General_CS_AS NOT NULL,
	RateCutoff varchar(8) COLLATE Latin1_General_CS_AS NOT NULL
CONSTRAINT PK_ConventionsAverageOIS PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsAverageOIS WITH CHECK ADD CONSTRAINT FK_ConventionsAverageOIS_FixedDayCounter FOREIGN KEY(FixedDayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsAverageOIS WITH CHECK ADD CONSTRAINT FK_ConventionsAverageOIS_FixedCalendar FOREIGN KEY(FixedCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsAverageOIS WITH CHECK ADD CONSTRAINT FK_ConventionsAverageOIS_FixedConvention FOREIGN KEY(FixedConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsAverageOIS WITH CHECK ADD CONSTRAINT FK_ConventionsAverageOIS_FixedPaymentConvention FOREIGN KEY(FixedPaymentConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsAverageOIS WITH CHECK ADD CONSTRAINT FK_ConventionsAverageOIS_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)


CREATE TABLE ConventionsTenorBasisSwap (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	LongIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortPayTenor varchar(8) COLLATE Latin1_General_CS_AS,
	SpreadOnShort varchar(5) COLLATE Latin1_General_CS_AS,
	IncludeSpread varchar(5) COLLATE Latin1_General_CS_AS,
	SubPeriodsCouponType varchar(20) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsTenorBasisSwap PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsTenorBasisSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisSwap_SpreadOnShort FOREIGN KEY(SpreadOnShort)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsTenorBasisSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisSwap_IncludeSpread FOREIGN KEY(IncludeSpread)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsTenorBasisSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisSwap_LongIndex FOREIGN KEY(LongIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsTenorBasisSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisSwap_ShortIndex FOREIGN KEY(ShortIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsTenorBasisSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisSwap_SubPeriodsCouponType FOREIGN KEY(SubPeriodsCouponType)
REFERENCES TypesSubPeriodsCouponType (value)

CREATE TABLE ConventionsTenorBasisTwoSwap (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	Calendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	LongFixedFrequency varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	LongFixedConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	LongFixedDayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	LongIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortFixedFrequency varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortFixedConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortFixedDayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	ShortIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	LongMinusShort varchar(5) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsTenorBasisTwoSwap PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_Calendar FOREIGN KEY(Calendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_LongFixedFrequency FOREIGN KEY(LongFixedFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_LongFixedConvention FOREIGN KEY(LongFixedConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_LongFixedDayCounter FOREIGN KEY(LongFixedDayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_LongIndex FOREIGN KEY(LongIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_ShortFixedFrequency FOREIGN KEY(ShortFixedFrequency)
REFERENCES TypesFrequencyType (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_ShortFixedConvention FOREIGN KEY(ShortFixedConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_ShortFixedDayCounter FOREIGN KEY(ShortFixedDayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_ShortIndex FOREIGN KEY(ShortIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsTenorBasisTwoSwap WITH CHECK ADD CONSTRAINT FK_ConventionsTenorBasisTwoSwap_LongMinusShort FOREIGN KEY(LongMinusShort)
REFERENCES TypesBool (value)

CREATE TABLE ConventionsFX (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	SpotDays int NOT NULL,
	SourceCurrency varchar(7) COLLATE Latin1_General_CS_AS NOT NULL,
	TargetCurrency varchar(7) COLLATE Latin1_General_CS_AS NOT NULL,
	PointsFactor Decimal(18,4) NOT NULL,
	AdvanceCalendar varchar(20) COLLATE Latin1_General_CS_AS,
	SpotRelative varchar(5) COLLATE Latin1_General_CS_AS,
	AdditionalSettleCalendar varchar(20) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsFX PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsFX WITH CHECK ADD CONSTRAINT FK_ConventionsFX_SourceCurrency FOREIGN KEY(SourceCurrency)
REFERENCES TypesCurrencyCode (value)
ALTER TABLE ConventionsFX WITH CHECK ADD CONSTRAINT FK_ConventionsFX_TargetCurrency FOREIGN KEY(TargetCurrency)
REFERENCES TypesCurrencyCode (value)
ALTER TABLE ConventionsFX WITH CHECK ADD CONSTRAINT FK_ConventionsFX_AdvanceCalendar FOREIGN KEY(AdvanceCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsFX WITH CHECK ADD CONSTRAINT FK_ConventionsFX_SpotRelative FOREIGN KEY(SpotRelative)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsFX WITH CHECK ADD CONSTRAINT FK_ConventionsFX_AdditionalSettleCalendar FOREIGN KEY(AdditionalSettleCalendar)
REFERENCES TypesCalendar (value)

CREATE TABLE ConventionsCrossCurrencyBasis (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	SettlementDays int NOT NULL,
	SettlementCalendar varchar(20) COLLATE Latin1_General_CS_AS,
	RollConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FlatIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	SpreadIndex varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	EOM varchar(5) COLLATE Latin1_General_CS_AS
CONSTRAINT PK_ConventionsCrossCurrencyBasis PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsCrossCurrencyBasis WITH CHECK ADD CONSTRAINT FK_ConventionsCrossCurrencyBasis_SettlementCalendar FOREIGN KEY(SettlementCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsCrossCurrencyBasis WITH CHECK ADD CONSTRAINT FK_ConventionsCrossCurrencyBasis_RollConvention FOREIGN KEY(RollConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsCrossCurrencyBasis WITH CHECK ADD CONSTRAINT FK_ConventionsCrossCurrencyBasis_FlatIndex FOREIGN KEY(FlatIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsCrossCurrencyBasis WITH CHECK ADD CONSTRAINT FK_ConventionsCrossCurrencyBasis_SpreadIndex FOREIGN KEY(SpreadIndex)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsCrossCurrencyBasis WITH CHECK ADD CONSTRAINT FK_ConventionsCrossCurrencyBasis_EOM FOREIGN KEY(EOM)
REFERENCES TypesBool (value)

CREATE TABLE ConventionsSwapIndex (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	Conventions varchar(128) COLLATE Latin1_General_CS_AS NOT NULL
CONSTRAINT PK_ConventionsSwapIndex PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsSwapIndex WITH CHECK ADD CONSTRAINT FK_ConventionsSwapIndex_Conventions FOREIGN KEY(Conventions)
REFERENCES ConventionsSwap (Id)

CREATE TABLE ConventionsInflationSwap (
	Id varchar(128) COLLATE Latin1_General_CS_AS NOT NULL,
	FixCalendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	FixConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	DayCounter varchar(30) COLLATE Latin1_General_CS_AS NOT NULL,
	IndexName varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	Interpolated varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
	ObservationLag varchar(8) COLLATE Latin1_General_CS_AS NOT NULL,
	AdjustInflationObservationDates varchar(5) COLLATE Latin1_General_CS_AS NOT NULL,
	InflationCalendar varchar(20) COLLATE Latin1_General_CS_AS NOT NULL,
	InflationConvention varchar(20) COLLATE Latin1_General_CS_AS NOT NULL
CONSTRAINT PK_ConventionsInflationSwap PRIMARY KEY CLUSTERED (
	Id ASC
))
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_FixCalendar FOREIGN KEY(FixCalendar)
REFERENCES TypesCalendar (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_FixConvention FOREIGN KEY(FixConvention)
REFERENCES TypesBusinessDayConvention (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_DayCounter FOREIGN KEY(DayCounter)
REFERENCES TypesDayCounter (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_IndexName FOREIGN KEY(IndexName)
REFERENCES TypesIndexName (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_Interpolated FOREIGN KEY(Interpolated)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_AdjustInflationObservationDates FOREIGN KEY(AdjustInflationObservationDates)
REFERENCES TypesBool (value)
ALTER TABLE ConventionsInflationSwap WITH CHECK ADD CONSTRAINT FK_ConventionsInflationSwap_InflationCalendar FOREIGN KEY(InflationCalendar)
REFERENCES TypesCalendar (value)
