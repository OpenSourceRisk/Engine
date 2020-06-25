/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file scenario/scenariosimmarketparameters.hpp
    \brief A class to hold Scenario parameters for scenarioSimMarket
    \ingroup scenario
*/

#pragma once

#include <boost/assign/list_of.hpp>

#include <orea/scenario/scenario.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/dynamicstype.hpp>

namespace ore {
namespace analytics {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using namespace QuantLib;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! ScenarioSimMarket description
/*! \ingroup scenario
 */
class ScenarioSimMarketParameters : public XMLSerializable {
public:
    //! Default constructor
    ScenarioSimMarketParameters()
        : extrapolate_(false), swapVolIsCube_({{"", false}}), swapVolSimulateATMOnly_(false),
          swapVolStrikeSpreads_({{"", {0.0}}}), equityIsSurface_(false), equityVolSimulateATMOnly_(true),
          equityMoneyness_({1.0}), cprSimulate_(false), correlationIsSurface_(false), correlationStrikes_({0.0}) {
        setDefaults();
    }

    //! \name Inspectors
    //@{
    const string& baseCcy() const { return baseCcy_; }
    const vector<string>& ccys() const { return ccys_; }
    vector<string> paramsLookup(RiskFactorKey::KeyType k) const;
    bool hasParamsName(RiskFactorKey::KeyType kt, string name) const;
    void addParamsName(RiskFactorKey::KeyType kt, vector<string> names);
    bool paramsSimulate(RiskFactorKey::KeyType kt) const;
    void setParamsSimulate(RiskFactorKey::KeyType kt, bool simulate);

    vector<string> discountCurveNames() const { return paramsLookup(RiskFactorKey::KeyType::DiscountCurve); }

    const string& yieldCurveDayCounter(const string& key) const;
    vector<string> yieldCurveNames() const { return paramsLookup(RiskFactorKey::KeyType::YieldCurve); }
    const map<string, string>& yieldCurveCurrencies() const { return yieldCurveCurrencies_; }
    const vector<Period>& yieldCurveTenors(const string& key) const;
    bool hasYieldCurveTenors(const string& key) const { return yieldCurveTenors_.count(key) > 0; }
    vector<string> indices() const { return paramsLookup(RiskFactorKey::KeyType::IndexCurve); }
    const map<string, string>& swapIndices() const { return swapIndices_; }
    const string& interpolation() const { return interpolation_; }
    bool extrapolate() const { return extrapolate_; }

    bool simulateFxSpots() const { return paramsSimulate(RiskFactorKey::KeyType::FXSpot); }
    vector<string> fxCcyPairs() const { return paramsLookup(RiskFactorKey::KeyType::FXSpot); }

    bool simulateSwapVols() const { return paramsSimulate(RiskFactorKey::KeyType::SwaptionVolatility); }
    bool swapVolIsCube(const string& key) const;
    bool simulateSwapVolATMOnly() const { return swapVolSimulateATMOnly_; }
    const vector<Period>& swapVolTerms(const string& key) const;
    const vector<Period>& swapVolExpiries(const string& key) const;
    vector<string> swapVolCcys() const { return paramsLookup(RiskFactorKey::KeyType::SwaptionVolatility); }
    const string& swapVolDayCounter(const string& key) const;
    const string& swapVolDecayMode() const { return swapVolDecayMode_; }
    const vector<Real>& swapVolStrikeSpreads(const string& key) const;

    bool simulateYieldVols() const { return paramsSimulate(RiskFactorKey::KeyType::YieldVolatility); }
    const vector<Period>& yieldVolTerms() const { return yieldVolTerms_; }
    const vector<Period>& yieldVolExpiries() const { return yieldVolExpiries_; }
    vector<string> yieldVolNames() const { return paramsLookup(RiskFactorKey::KeyType::YieldVolatility); }
    const string& yieldVolDayCounter(const string& key) const;
    const string& yieldVolDecarayMode() const { return yieldVolDecayMode_; }

    bool simulateCapFloorVols() const { return paramsSimulate(RiskFactorKey::KeyType::OptionletVolatility); }
    vector<string> capFloorVolCcys() const { return paramsLookup(RiskFactorKey::KeyType::OptionletVolatility); }
    const string& capFloorVolDayCounter(const string& key) const;
    const vector<Period>& capFloorVolExpiries(const string& key) const;
    bool hasCapFloorVolExpiries(const string& key) const { return capFloorVolExpiries_.count(key) > 0; }
    const vector<QuantLib::Rate>& capFloorVolStrikes(const std::string& key) const;
    bool capFloorVolIsAtm(const std::string& key) const;
    const string& capFloorVolDecayMode() const { return capFloorVolDecayMode_; }

    bool simulateYoYInflationCapFloorVols() const {
        return paramsSimulate(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility);
    }
    vector<string> yoyInflationCapFloorVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility);
    }
    const string& yoyInflationCapFloorVolDayCounter(const string& key) const;
    const vector<Period>& yoyInflationCapFloorVolExpiries(const string& key) const;
    bool hasYoYInflationCapFloorVolExpiries(const string& key) const {
        return yoyInflationCapFloorVolExpiries_.count(key) > 0;
    }
    const vector<Real>& yoyInflationCapFloorVolStrikes(const std::string& key) const;
    const string& yoyInflationCapFloorVolDecayMode() const { return yoyInflationCapFloorVolDecayMode_; }

    bool simulateZeroInflationCapFloorVols() const {
        return paramsSimulate(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility);
    }
    vector<string> zeroInflationCapFloorVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility);
    }
    const string& zeroInflationCapFloorVolDayCounter(const string& key) const;
    const vector<Period>& zeroInflationCapFloorVolExpiries(const string& key) const;
    bool hasZeroInflationCapFloorVolExpiries(const string& key) const {
        return zeroInflationCapFloorVolExpiries_.count(key) > 0;
    }
    const vector<Real>& zeroInflationCapFloorVolStrikes(const string& key) const;
    const string& zeroInflationCapFloorVolDecayMode() const { return zeroInflationCapFloorVolDecayMode_; }

    bool simulateSurvivalProbabilities() const { return paramsSimulate(RiskFactorKey::KeyType::SurvivalProbability); }
    bool simulateRecoveryRates() const { return paramsSimulate(RiskFactorKey::KeyType::RecoveryRate); }
    vector<string> defaultNames() const { return paramsLookup(RiskFactorKey::KeyType::SurvivalProbability); }
    const string& defaultCurveDayCounter(const string& key) const;
    const string& defaultCurveCalendar(const string& key) const;
    const vector<Period>& defaultTenors(const string& key) const;
    bool hasDefaultTenors(const string& key) const { return defaultTenors_.count(key) > 0; }

    bool simulateCdsVols() const { return paramsSimulate(RiskFactorKey::KeyType::CDSVolatility); }
    const vector<Period>& cdsVolExpiries() const { return cdsVolExpiries_; }
    const string& cdsVolDayCounter(const string& key) const;
    vector<string> cdsVolNames() const { return paramsLookup(RiskFactorKey::KeyType::CDSVolatility); }
    const string& cdsVolDecayMode() const { return cdsVolDecayMode_; }

    vector<string> equityNames() const { return paramsLookup(RiskFactorKey::KeyType::EquitySpot); }
    const vector<Period>& equityDividendTenors(const string& key) const;
    bool hasEquityDividendTenors(const string& key) const { return equityDividendTenors_.count(key) > 0; }
    vector<string> equityDividendYields() const { return paramsLookup(RiskFactorKey::KeyType::DividendYield); }
    bool simulateDividendYield() const { return paramsSimulate(RiskFactorKey::KeyType::DividendYield); }

    // fxvol getters
    bool simulateFXVols() const { return paramsSimulate(RiskFactorKey::KeyType::FXVolatility); }
    bool fxVolIsSurface(const std::string& ccypair) const;
    bool fxVolIsSurface() const;
    bool hasFxPairWithSurface() const { return hasFxPairWithSurface_; }
    bool useMoneyness(const std::string& ccypair) const;
    bool useMoneyness() const;
    const vector<Period>& fxVolExpiries() const { return fxVolExpiries_; }
    const string& fxVolDayCounter(const string& key) const;
    const string& fxVolDecayMode() const { return fxVolDecayMode_; }
    vector<string> fxVolCcyPairs() const { return paramsLookup(RiskFactorKey::KeyType::FXVolatility); }
    const vector<Real>& fxVolMoneyness(const string& ccypair) const;
    const vector<Real>& fxVolMoneyness() const;
    const vector<Real>& fxVolStdDevs(const string& ccypair) const;
    const vector<Real>& fxVolStdDevs() const;

    bool simulateEquityVols() const { return paramsSimulate(RiskFactorKey::KeyType::EquityVolatility); }
    bool equityVolIsSurface() const { return equityIsSurface_; }
    bool simulateEquityVolATMOnly() const { return equityVolSimulateATMOnly_; }
    const vector<Period>& equityVolExpiries() const { return equityVolExpiries_; }
    const string& equityVolDayCounter(const string& key) const;
    const string& equityVolDecayMode() const { return equityVolDecayMode_; }
    vector<string> equityVolNames() const { return paramsLookup(RiskFactorKey::KeyType::EquityVolatility); }
    const vector<Real>& equityVolMoneyness() const { return equityMoneyness_; }

    const vector<string>& additionalScenarioDataIndices() const { return additionalScenarioDataIndices_; }
    const vector<string>& additionalScenarioDataCcys() const { return additionalScenarioDataCcys_; }

    bool securitySpreadsSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::SecuritySpread); }
    vector<string> securities() const { return paramsLookup(RiskFactorKey::KeyType::SecuritySpread); }

    bool simulateBaseCorrelations() const { return paramsSimulate(RiskFactorKey::KeyType::BaseCorrelation); }
    const vector<Period>& baseCorrelationTerms() const { return baseCorrelationTerms_; }
    const vector<Real>& baseCorrelationDetachmentPoints() const { return baseCorrelationDetachmentPoints_; }
    vector<string> baseCorrelationNames() const { return paramsLookup(RiskFactorKey::KeyType::BaseCorrelation); }
    const string& baseCorrelationDayCounter(const string& key) const;

    vector<string> cpiIndices() const { return paramsLookup(RiskFactorKey::KeyType::CPIIndex); }
    vector<string> zeroInflationIndices() const { return paramsLookup(RiskFactorKey::KeyType::ZeroInflationCurve); }
    const string& zeroInflationDayCounter(const string& key) const;
    const vector<Period>& zeroInflationTenors(const string& key) const;
    bool hasZeroInflationTenors(const string& key) const { return zeroInflationTenors_.count(key) > 0; }
    const string& yoyInflationDayCounter(const string& key) const;
    vector<string> yoyInflationIndices() const { return paramsLookup(RiskFactorKey::KeyType::YoYInflationCurve); }
    const vector<Period>& yoyInflationTenors(const string& key) const;
    bool hasYoyInflationTenors(const string& key) const { return yoyInflationTenors_.count(key) > 0; }

    // Commodity price curve data getters
    bool commodityCurveSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::CommodityCurve); }
    std::vector<std::string> commodityNames() const;
    const std::vector<QuantLib::Period>& commodityCurveTenors(const std::string& commodityName) const;
    bool hasCommodityCurveTenors(const std::string& commodityName) const;
    const std::string& commodityCurveDayCounter(const std::string& commodityName) const;

    // Commodity volatility data getters
    bool commodityVolSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::CommodityVolatility); }
    const std::string& commodityVolDecayMode() const { return commodityVolDecayMode_; }
    std::vector<std::string> commodityVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::CommodityVolatility);
    }
    const std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) const;
    const std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) const;
    const std::string& commodityVolDayCounter(const std::string& commodityName) const;

    bool simulateCorrelations() const { return paramsSimulate(RiskFactorKey::KeyType::Correlation); }
    bool correlationIsSurface() const { return correlationIsSurface_; }
    const vector<Period>& correlationExpiries() const { return correlationExpiries_; }
    vector<std::string> correlationPairs() const { return paramsLookup(RiskFactorKey::KeyType::Correlation); }
    const string& correlationDayCounter(const string& index1, const string& index2) const;
    const vector<Real>& correlationStrikes() const { return correlationStrikes_; }

    // Get the parameters
    const std::map<RiskFactorKey::KeyType, std::pair<bool, std::set<std::string>>>& parameters() const {
        return params_;
    }
    //@}

    //! \name Setters
    //@{
    string& baseCcy() { return baseCcy_; }
    vector<string>& ccys() { return ccys_; }
    void setDiscountCurveNames(vector<string> names);
    void setYieldCurveNames(vector<string> names);
    map<string, string>& yieldCurveCurrencies() { return yieldCurveCurrencies_; }
    void setYieldCurveDayCounters(const string& key, const string& p);
    void setYieldCurveTenors(const string& key, const vector<Period>& p);
    void setIndices(vector<string> names);
    map<string, string>& swapIndices() { return swapIndices_; }
    string& interpolation() { return interpolation_; }
    bool& extrapolate() { return extrapolate_; }

    void setSimulateFxSpots(bool simulate);
    void setFxCcyPairs(vector<string> names);

    void setSimulateSwapVols(bool simulate);
    void setSwapVolIsCube(const string& key, bool isCube);
    bool& simulateSwapVolATMOnly() { return swapVolSimulateATMOnly_; }
    void setSwapVolTerms(const string& key, const vector<Period>& p);
    void setSwapVolCcys(vector<string> names);
    void setSwapVolExpiries(const string& key, const vector<Period>& p);
    void setSwapVolStrikeSpreads(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& swapVolDecayMode() { return swapVolDecayMode_; }
    void setSwapVolDayCounters(const string& key, const string& p);

    void setSimulateYieldVols(bool simulate);
    vector<Period>& yieldVolTerms() { return yieldVolTerms_; }
    void setYieldVolNames(vector<string> names);
    vector<Period>& yieldVolExpiries() { return yieldVolExpiries_; }
    string& yieldVolDecayMode() { return yieldVolDecayMode_; }
    void setYieldVolDayCounters(const string& key, const string& p);

    void setSimulateCapFloorVols(bool simulate);
    void setCapFloorVolCcys(vector<string> names);
    void setCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    void setCapFloorVolIsAtm(const std::string& key, bool isAtm);
    string& capFloorVolDecayMode() { return capFloorVolDecayMode_; }
    void setCapFloorVolDayCounters(const string& key, const string& p);

    void setSimulateYoYInflationCapFloorVols(bool simulate);
    void setYoYInflationCapFloorVolNames(vector<string> names);
    void setYoYInflationCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setYoYInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& yoyInflationCapFloorVolDecayMode() { return yoyInflationCapFloorVolDecayMode_; }
    void setYoYInflationCapFloorVolDayCounters(const string& key, const string& p);

    void setSimulateZeroInflationCapFloorVols(bool simulate);
    void setZeroInflationCapFloorNames(vector<string> names);
    void setZeroInflationCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setZeroInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& zeroInflationCapFloorVolDecayMode() { return zeroInflationCapFloorVolDecayMode_; }
    void setZeroInflationCapFloorVolDayCounters(const string& key, const string& p);

    void setSimulateSurvivalProbabilities(bool simulate);
    void setSimulateRecoveryRates(bool simulate);
    void setDefaultNames(vector<string> names);
    void setDefaultTenors(const string& key, const vector<Period>& p);
    void setDefaultCurveDayCounters(const string& key, const string& p);
    void setDefaultCurveCalendars(const string& key, const string& p);

    void setSimulateCdsVols(bool simulate);
    vector<Period>& cdsVolExpiries() { return cdsVolExpiries_; }
    void setCdsVolNames(vector<string> names);
    string& cdsVolDecayMode() { return cdsVolDecayMode_; }
    void setCdsVolDayCounters(const string& key, const string& p);

    void setEquityNames(vector<string> names);
    void setEquityDividendCurves(vector<string> names);
    void setEquityDividendTenors(const string& key, const vector<Period>& p);
    void setSimulateDividendYield(bool simulate);

    // FX volatility data setters
    void setSimulateFXVols(bool simulate);
    void setFxVolIsSurface(const string& ccypair, bool val);
    void setFxVolIsSurface(bool val);
    void setHasFxPairWithSurface(bool val);
    void setUseMoneyness(const string& ccypair, bool val);
    void setUseMoneyness(bool val);
    void setFxVolExpiries(const vector<Period>& expiries);
    void setFxVolDecayMode(const string& val);
    void setFxVolCcyPairs(vector<string> names);
    void setFxVolMoneyness(const string& ccypair, const vector<Real>& moneyness);
    void setFxVolMoneyness(const vector<Real>& moneyness);
    void setFxVolStdDevs(const string& ccypair, const vector<Real>& stdDevs);
    void setFxVolStdDevs(const vector<Real>& stdDevs);
    void setFxVolDayCounters(const string& key, const string& p);

    void setSimulateEquityVols(bool simulate);
    bool& equityVolIsSurface() { return equityIsSurface_; }
    bool& simulateEquityVolATMOnly() { return equityVolSimulateATMOnly_; }
    vector<Period>& equityVolExpiries() { return equityVolExpiries_; }
    string& equityVolDecayMode() { return equityVolDecayMode_; }
    void setEquityVolNames(vector<string> names);
    vector<Real>& equityVolMoneyness() { return equityMoneyness_; }
    void setEquityVolDayCounters(const string& key, const string& p);

    vector<string>& additionalScenarioDataIndices() { return additionalScenarioDataIndices_; }
    vector<string>& additionalScenarioDataCcys() { return additionalScenarioDataCcys_; }

    void setSecuritySpreadsSimulate(bool simulate);
    void setSecurities(vector<string> names);
    void setRecoveryRates(vector<string> names);

    void setCprs(const vector<string>& names);
    void setSimulateCprs(bool simulate);
    bool simulateCprs() const { return paramsSimulate(RiskFactorKey::KeyType::CPR); }
    const vector<string>& cprs() const { return cprs_; }

    void setSimulateBaseCorrelations(bool simulate);
    vector<Period>& baseCorrelationTerms() { return baseCorrelationTerms_; }
    vector<Real>& baseCorrelationDetachmentPoints() { return baseCorrelationDetachmentPoints_; }
    void setBaseCorrelationNames(vector<string> names);
    void setBaseCorrelationDayCounters(const string& key, const string& p);

    void setCpiIndices(vector<string> names);
    void setZeroInflationIndices(vector<string> names);
    void setZeroInflationTenors(const string& key, const vector<Period>& p);
    void setZeroInflationDayCounters(const string& key, const string& p);
    void setYoyInflationIndices(vector<string> names);
    void setYoyInflationTenors(const string& key, const vector<Period>& p);
    void setYoyInflationDayCounters(const string& key, const string& p);

    // Commodity price curve data setters
    void setCommodityCurveSimulate(bool simulate);
    void setCommodityNames(vector<string> names);
    void setCommodityCurves(vector<string> names);
    void setCommodityCurveTenors(const std::string& commodityName, const std::vector<QuantLib::Period>& p);
    void setCommodityCurveDayCounter(const std::string& commodityName, const std::string& d);

    // Commodity volatility data setters
    void setCommodityVolSimulate(bool simulate);
    std::string& commodityVolDecayMode() { return commodityVolDecayMode_; }
    void setCommodityVolNames(vector<string> names);
    std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) {
        return commodityVolExpiries_[commodityName];
    }
    std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) {
        return commodityVolMoneyness_[commodityName];
    }
    void setCommodityVolDayCounter(const std::string& commodityName, const std::string& d);

    void setSimulateCorrelations(bool simulate);
    bool& correlationIsSurface() { return correlationIsSurface_; }
    vector<Period>& correlationExpiries() { return correlationExpiries_; }
    void setCorrelationPairs(vector<string> names);
    vector<Real>& correlationStrikes() { return correlationStrikes_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(ore::data::XMLDocument& doc);
    //@}

    //! \name Equality Operators
    //@{
    bool operator==(const ScenarioSimMarketParameters& rhs);
    bool operator!=(const ScenarioSimMarketParameters& rhs);
    //@}

private:
    void setDefaults();

    //! A method used to reset the object to its default state before fromXML is called
    void reset();

    string baseCcy_;
    map<string, string> yieldCurveDayCounters_;
    vector<string> ccys_; // may or may not include baseCcy;
    map<string, string> yieldCurveCurrencies_;
    map<string, vector<Period>> yieldCurveTenors_;
    map<string, string> swapIndices_;
    string interpolation_;
    bool extrapolate_;

    map<string, bool> swapVolIsCube_;
    bool swapVolSimulateATMOnly_;
    map<string, vector<Period>> swapVolTerms_;
    map<string, string> swapVolDayCounters_;
    map<string, vector<Period>> swapVolExpiries_;
    map<string, vector<Real>> swapVolStrikeSpreads_;
    string swapVolDecayMode_;

    vector<Period> yieldVolTerms_;
    map<string, string> yieldVolDayCounters_;
    vector<Period> yieldVolExpiries_;
    string yieldVolDecayMode_;

    map<string, string> capFloorVolDayCounters_;
    map<string, vector<Period>> capFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> capFloorVolStrikes_;
    map<std::string, bool> capFloorVolIsAtm_;
    string capFloorVolDecayMode_;

    map<string, string> yoyInflationCapFloorVolDayCounters_;
    map<string, vector<Period>> yoyInflationCapFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> yoyInflationCapFloorVolStrikes_;
    string yoyInflationCapFloorVolDecayMode_;

    map<string, string> zeroInflationCapFloorVolDayCounters_;
    map<string, vector<Period>> zeroInflationCapFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> zeroInflationCapFloorVolStrikes_;
    string zeroInflationCapFloorVolDecayMode_;

    map<string, string> defaultCurveDayCounters_;
    map<string, string> defaultCurveCalendars_;
    map<string, vector<Period>> defaultTenors_;

    vector<Period> cdsVolExpiries_;
    map<string, string> cdsVolDayCounters_;
    string cdsVolDecayMode_;

    map<string, vector<Period>> equityDividendTenors_;

    // FX volatility data
    bool hasFxPairWithSurface_;
    map<std::string, bool> useMoneyness_;
    map<std::string, bool> fxVolIsSurface_;
    vector<Period> fxVolExpiries_;
    map<string, string> fxVolDayCounters_;
    string fxVolDecayMode_;
    map<string, vector<Real>> fxMoneyness_;
    map<string, vector<Real>> fxStandardDevs_;

    bool equityIsSurface_;
    bool equityVolSimulateATMOnly_;
    vector<Period> equityVolExpiries_;
    map<string, string> equityVolDayCounters_;
    string equityVolDecayMode_;
    vector<Real> equityMoneyness_;

    vector<string> additionalScenarioDataIndices_;
    vector<string> additionalScenarioDataCcys_;

    bool cprSimulate_;
    vector<string> cprs_;

    vector<Period> baseCorrelationTerms_;
    map<string, string> baseCorrelationDayCounters_;
    vector<Real> baseCorrelationDetachmentPoints_;

    map<string, string> zeroInflationDayCounters_;
    map<string, vector<Period>> zeroInflationTenors_;
    map<string, string> yoyInflationDayCounters_;
    map<string, vector<Period>> yoyInflationTenors_;

    // Commodity price curve data
    std::map<std::string, std::vector<QuantLib::Period>> commodityCurveTenors_;
    std::map<std::string, std::string> commodityCurveDayCounters_;

    // Commodity volatility data
    std::string commodityVolDecayMode_;
    std::map<std::string, std::vector<QuantLib::Period>> commodityVolExpiries_;
    std::map<std::string, std::vector<QuantLib::Real>> commodityVolMoneyness_;
    std::map<std::string, std::string> commodityVolDayCounters_;

    bool correlationIsSurface_;
    map<pair<string, string>, string> correlationDayCounters_;
    vector<Period> correlationExpiries_;
    vector<Real> correlationStrikes_;

    // Store sim market params as a map from RiskFactorKey::KeyType to a pair,
    // boolean of whether to simulate and a set of curve names
    std::map<RiskFactorKey::KeyType, std::pair<bool, std::set<std::string>>> params_;
};
} // namespace analytics
} // namespace ore
