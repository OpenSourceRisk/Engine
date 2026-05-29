/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#ifndef ored_wrappedmarket_i
#define ored_wrappedmarket_i

%include ored_market.i
%include <std_vector.i>
%include <std_set.i>

%shared_ptr(ore::data::WrappedMarket)
%shared_ptr(ore::data::YieldCurveCalibrationInfo)
%shared_ptr(ore::data::FittedBondCurveCalibrationInfo)
%shared_ptr(ore::data::InflationCurveCalibrationInfo)
%shared_ptr(ore::data::ZeroInflationCurveCalibrationInfo)
%shared_ptr(ore::data::YoYInflationCurveCalibrationInfo)
%shared_ptr(ore::data::CommodityCurveCalibrationInfo)
%shared_ptr(ore::data::FxEqCommVolCalibrationInfo)
%shared_ptr(ore::data::IrVolCalibrationInfo)
%shared_ptr(ore::data::CpiVolCalibrationInfo)
%shared_ptr(ore::data::TodaysMarketCalibrationInfo)

// Ignore members that need types not yet wrapped or that cause duplicate SWIG traits
// rateHelperCashflows requires TradeCashflowReportData (not yet wrapped)
%ignore ore::data::YieldCurveCalibrationInfo::rateHelperCashflows;
%ignore ore::data::InflationCurveCalibrationInfo::rateHelperCashflows;
// rateHelperPillarDates is vector<set<Date>>; declaring DateSetVector would re-generate
// swig::traits<set<Date>> which is already specialised by DateSet in ored_volcurves.i (C2766)
%ignore ore::data::YieldCurveCalibrationInfo::rateHelperPillarDates;

// Container templates needed for calibration info members
// DateSet and PeriodVector are already declared in ored_volcurves.i and date.i respectively
%template(BoolVectorVector) std::vector<std::vector<bool>>;
%template(DoubleVectorVectorVector) std::vector<std::vector<std::vector<double>>>;
%template(BoolVectorVectorVector) std::vector<std::vector<std::vector<bool>>>;

namespace ore {
namespace data {

// ---------------------------------------------------------------------------
// Calibration info structs
// ---------------------------------------------------------------------------

struct YieldCurveCalibrationInfo {
    std::string dayCounter;
    std::string currency;
    std::vector<QuantLib::Date> pillarDates;
    std::vector<double> zeroRates;
    std::vector<double> discountFactors;
    std::vector<double> times;
    std::vector<std::set<QuantLib::Date>> rateHelperPillarDates;
    std::vector<std::string> mdQuoteLabels;
    std::vector<double> mdQuoteValues;
    std::vector<std::string> rateHelperTypes;
    std::vector<double> rateHelperQuoteErrors;
};

struct FittedBondCurveCalibrationInfo : public YieldCurveCalibrationInfo {
    std::string fittingMethod;
    std::vector<double> solution;
    int iterations;
    double costValue;
    double tolerance;
    std::vector<std::string> securities;
    std::vector<QuantLib::Date> securityMaturityDates;
    std::vector<double> marketPrices;
    std::vector<double> modelPrices;
    std::vector<double> marketYields;
    std::vector<double> modelYields;
};

struct InflationCurveCalibrationInfo {
    std::string dayCounter;
    std::string calendar;
    QuantLib::Date baseDate;
    std::vector<QuantLib::Date> pillarDates;
    std::vector<double> times;
    std::vector<std::string> mdQuoteLabels;
    std::vector<double> mdQuoteValues;
    std::vector<std::string> rateHelperTypes;
};

struct ZeroInflationCurveCalibrationInfo : public InflationCurveCalibrationInfo {
    double baseCpi;
    std::vector<double> zeroRates;
    std::vector<double> forwardCpis;
};

struct YoYInflationCurveCalibrationInfo : public InflationCurveCalibrationInfo {
    std::vector<double> yoyRates;
};

struct CommodityCurveCalibrationInfo {
    std::string dayCounter;
    std::string calendar;
    std::string currency;
    std::string interpolationMethod;
    std::vector<QuantLib::Date> pillarDates;
    std::vector<QuantLib::Real> futurePrices;
    std::vector<QuantLib::Real> times;
};

struct FxEqCommVolCalibrationInfo {
    std::string dayCounter;
    std::string calendar;
    std::string atmType;
    std::string deltaType;
    std::string longTermAtmType;
    std::string longTermDeltaType;
    std::string switchTenor;
    std::string riskReversalInFavorOf;
    std::string butterflyStyle;
    bool isArbitrageFree;
    std::vector<QuantLib::Date> expiryDates;
    std::vector<double> times;
    std::vector<std::string> deltas;
    std::vector<double> moneyness;
    std::vector<double> forwards;
    std::vector<std::vector<double>> moneynessGridStrikes;
    std::vector<std::vector<double>> moneynessGridProb;
    std::vector<std::vector<double>> moneynessGridImpliedVolatility;
    std::vector<std::vector<double>> deltaGridStrikes;
    std::vector<std::vector<double>> deltaGridProb;
    std::vector<std::vector<double>> deltaGridImpliedVolatility;
    std::vector<std::vector<double>> deltaCallPrices;
    std::vector<std::vector<double>> deltaPutPrices;
    std::vector<std::vector<double>> moneynessCallPrices;
    std::vector<std::vector<double>> moneynessPutPrices;
    std::vector<std::vector<bool>> moneynessGridCallSpreadArbitrage;
    std::vector<std::vector<bool>> moneynessGridButterflyArbitrage;
    std::vector<std::vector<bool>> moneynessGridCalendarArbitrage;
    std::vector<std::vector<bool>> deltaGridCallSpreadArbitrage;
    std::vector<std::vector<bool>> deltaGridButterflyArbitrage;
    std::vector<std::string> messages;
};

struct IrVolCalibrationInfo {
    std::string dayCounter;
    std::string calendar;
    bool isArbitrageFree;
    std::vector<QuantLib::Date> expiryDates;
    std::vector<QuantLib::Period> underlyingTenors;
    std::string volatilityType;
    std::vector<double> times;
    std::vector<double> strikeSpreads;
    std::vector<double> strikes;
    std::vector<std::vector<double>> forwards;
    std::vector<std::vector<std::vector<double>>> strikeSpreadGridStrikes;
    std::vector<std::vector<std::vector<double>>> strikeSpreadGridProb;
    std::vector<std::vector<std::vector<double>>> strikeSpreadGridImpliedVolatility;
    std::vector<std::vector<std::vector<double>>> strikeGridStrikes;
    std::vector<std::vector<std::vector<double>>> strikeGridProb;
    std::vector<std::vector<std::vector<double>>> strikeGridImpliedVolatility;
    std::vector<std::vector<std::vector<bool>>> strikeSpreadGridCallSpreadArbitrage;
    std::vector<std::vector<std::vector<bool>>> strikeSpreadGridButterflyArbitrage;
    std::vector<std::vector<std::vector<bool>>> strikeGridCallSpreadArbitrage;
    std::vector<std::vector<std::vector<bool>>> strikeGridButterflyArbitrage;
    std::vector<std::string> messages;
};

struct CpiVolCalibrationInfo {
    std::string dayCounter;
    std::string calendar;
    bool isArbitrageFree;
    std::vector<QuantLib::Date> expiryDates;
    std::vector<QuantLib::Date> optionObservationDates;
    std::vector<double> times;
    std::vector<double> optionLifeTimes;
    std::vector<double> forwards;
    std::vector<double> strikes;
    std::vector<std::vector<double>> strikeGridProb;
    std::vector<std::vector<double>> strikeGridImpliedVolatility;
    std::vector<std::vector<bool>> strikeGridCallSpreadArbitrage;
    std::vector<std::vector<bool>> strikeGridButterflyArbitrage;
    std::vector<double> forwardCPI;
    std::vector<std::vector<double>> strikeCPI;
};

struct TodaysMarketCalibrationInfo {
    QuantLib::Date asof;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo>> yieldCurveCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo>> dividendCurveCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InflationCurveCalibrationInfo>> inflationCurveCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::CommodityCurveCalibrationInfo>> commodityCurveCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo>> fxVolCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo>> eqVolCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::IrVolCalibrationInfo>> irVolCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo>> commVolCalibrationInfo;
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::CpiVolCalibrationInfo>> cpiVolCalibrationInfo;
};

// ---------------------------------------------------------------------------
// WrappedMarket: decorator that delegates all requests to an underlying Market.
// In SWIG, declared as a standalone class (Market is abstract; cannot be
// declared as a SWIG base).  Use the %extend constructors below to build a
// WrappedMarket from any existing MarketImpl or TodaysMarket.
// ---------------------------------------------------------------------------

class WrappedMarket {
public:
    %extend {
        WrappedMarket(const QuantLib::ext::shared_ptr<ore::data::MarketImpl>& market,
                      const bool handlePseudoCurrencies) {
            return new ore::data::WrappedMarket(market, handlePseudoCurrencies);
        }
        WrappedMarket(const QuantLib::ext::shared_ptr<ore::data::TodaysMarket>& market,
                      const bool handlePseudoCurrencies) {
            return new ore::data::WrappedMarket(market, handlePseudoCurrencies);
        }
    }

    QuantLib::Date asofDate() const;

    QuantLib::Handle<QuantLib::YieldTermStructure> yieldCurve(
        const ore::data::YieldCurveType& type, const std::string& name,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> yieldCurve(
        const std::string& name,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve(
        const std::string& ccy,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::SwaptionVolatilityStructure> swaptionVol(
        const std::string& ccy,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    const std::string shortSwapIndexBase(
        const std::string& ccy,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    const std::string swapIndexBase(
        const std::string& ccy,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::Quote> fxRate(
        const std::string& ccypair,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote> fxSpot(
        const std::string& ccypair,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> fxVol(
        const std::string& ccypair,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantExt::CreditCurve> defaultCurve(
        const std::string& name,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::Quote> recoveryRate(
        const std::string& name,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::OptionletVolatilityStructure> capFloorVol(
        const std::string& ccy,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::Quote> equitySpot(
        const std::string& eqName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> equityDividendCurve(
        const std::string& eqName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::YieldTermStructure> equityForecastCurve(
        const std::string& eqName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> equityVol(
        const std::string& eqName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::Quote> securitySpread(
        const std::string& securityID,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantExt::PriceTermStructure> commodityPriceCurve(
        const std::string& commodityName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> commodityVolatility(
        const std::string& commodityName,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;

    QuantLib::Handle<QuantExt::CorrelationTermStructure> correlationCurve(
        const std::string& index1, const std::string& index2,
        const std::string& configuration = ore::data::Market::defaultConfiguration) const;
};

} // namespace data
} // namespace ore

%template(StringYieldCurveCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo>>;
%template(StringInflationCurveCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InflationCurveCalibrationInfo>>;
%template(StringCommodityCurveCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::CommodityCurveCalibrationInfo>>;
%template(StringFxEqCommVolCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::FxEqCommVolCalibrationInfo>>;
%template(StringIrVolCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::IrVolCalibrationInfo>>;
%template(StringCpiVolCalibrationInfoMap)
    std::map<std::string, QuantLib::ext::shared_ptr<ore::data::CpiVolCalibrationInfo>>;

#endif
