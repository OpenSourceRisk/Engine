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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/dynamicstype.hpp>

using QuantLib::Date;
using QuantLib::Period;
using QuantLib::Rate;
using std::vector;
using std::string;
using std::pair;
using ore::data::XMLSerializable;
using ore::data::XMLNode;
using ore::data::XMLUtils;

namespace ore {
namespace analytics {

//! ScenarioSimMarket description
/*! \ingroup scenario
 */
class ScenarioSimMarketParameters : public XMLSerializable {
public:
    //! Default constructor
    ScenarioSimMarketParameters()
        : extrapolate_(false), fxSpotSimulate_(true), swapVolSimulate_(false), swapVolIsCube_(false), swapVolSimulateATMOnly_(true),
          swapVolStrikeSpreads_({0.0}), capFloorVolSimulate_(false), survivalProbabilitySimulate_(false),
          recoveryRateSimulate_(false), cdsVolSimulate_(false), equityForecastCurveSimulate_(true),
          dividendYieldSimulate_(false), fxVolSimulate_(false), fxVolIsSurface_(false), fxMoneyness_({0.0}),
          equityVolSimulate_(false), equityIsSurface_(false), equityVolSimulateATMOnly_(true), equityMoneyness_({1.0}),
          securitySpreadsSimulate_(false), baseCorrelationSimulate_(false), commodityCurveSimulate_(false), commodityVolSimulate_(false) {
        
        // set defaults
        setDefaults();
    }

    //! \name Inspectors
    //@{
    const string& baseCcy() const { return baseCcy_; }
    const vector<string>& ccys() const { return ccys_; }
    const string& yieldCurveDayCounter(const string& key) const;
    const vector<string>& yieldCurveNames() const { return yieldCurveNames_; }
    const vector<string>& yieldCurveCurrencies() const { return yieldCurveCurrencies_; }
    const vector<Period>& yieldCurveTenors(const string& key) const;
    bool hasYieldCurveTenors(const string& key) const { return yieldCurveTenors_.count(key) > 0; }
    const vector<string>& indices() const { return indices_; }
    const map<string, string>& swapIndices() const { return swapIndices_; }
    const string& interpolation() const { return interpolation_; }
    bool extrapolate() const { return extrapolate_; }

    bool simulateFxSpots() const { return fxSpotSimulate_; }
    const vector<string>& fxCcyPairs() const { return fxCcyPairs_; }

    bool simulateSwapVols() const { return swapVolSimulate_; }
    bool swapVolIsCube() const { return swapVolIsCube_; }
    bool simulateSwapVolATMOnly() const { return swapVolSimulateATMOnly_; }
    const vector<Period>& swapVolTerms() const { return swapVolTerms_; }
    const vector<Period>& swapVolExpiries() const { return swapVolExpiries_; }
    const vector<string>& swapVolCcys() const { return swapVolCcys_; }
    const string& swapVolDayCounter(const string& key) const;
    const string& swapVolDecayMode() const { return swapVolDecayMode_; }
    const vector<Real>& swapVolStrikeSpreads() const { return swapVolStrikeSpreads_; }

    bool simulateCapFloorVols() const { return capFloorVolSimulate_; }
    const vector<string>& capFloorVolCcys() const { return capFloorVolCcys_; }
    const string& capFloorVolDayCounter(const string& key) const;
    const vector<Period>& capFloorVolExpiries(const string& key) const;
    bool hasCapFloorVolExpiries(const string& key) const { return capFloorVolExpiries_.count(key) > 0; }
    const vector<Real>& capFloorVolStrikes() const { return capFloorVolStrikes_; }
    const string& capFloorVolDecayMode() const { return capFloorVolDecayMode_; }

    bool simulateSurvivalProbabilities() const { return survivalProbabilitySimulate_; }
    bool simulateRecoveryRates() const { return recoveryRateSimulate_; }
    const vector<string>& defaultNames() const { return defaultNames_; }
    const string& defaultCurveDayCounter(const string& key) const;
    const string& defaultCurveCalendar(const string& key) const;
    const vector<Period>& defaultTenors(const string& key) const;
    bool hasDefaultTenors(const string& key) const { return defaultTenors_.count(key) > 0; }

    bool simulateCdsVols() const { return cdsVolSimulate_; }
    const vector<Period>& cdsVolExpiries() const { return cdsVolExpiries_; }
    const string& cdsVolDayCounter(const string& key) const;
    const vector<string>& cdsVolNames() const { return cdsVolNames_; }
    const string& cdsVolDecayMode() const { return cdsVolDecayMode_; }

    const vector<string>& equityNames() const { return equityNames_; }
    const vector<Period>& equityDividendTenors(const string& key) const;
    bool hasEquityDividendTenors(const string& key) const { return equityDividendTenors_.count(key) > 0; }
    const vector<Period>& equityForecastTenors(const string& key) const;
    bool hasEquityForecastTenors(const string& key) const { return equityForecastTenors_.count(key) > 0; }

    bool simulateFXVols() const { return fxVolSimulate_; }
    bool fxVolIsSurface() const { return fxVolIsSurface_; }
    const vector<Period>& fxVolExpiries() const { return fxVolExpiries_; }
    const string& fxVolDayCounter(const string& key) const;
    const string& fxVolDecayMode() const { return fxVolDecayMode_; }
    const vector<string>& fxVolCcyPairs() const { return fxVolCcyPairs_; }
    const vector<Real>& fxVolMoneyness() const { return fxMoneyness_; }

    bool simulateEquityVols() const { return equityVolSimulate_; }
    bool equityVolIsSurface() const { return equityIsSurface_; }
    bool simulateEquityVolATMOnly() const { return equityVolSimulateATMOnly_; }
    const vector<Period>& equityVolExpiries() const { return equityVolExpiries_; }
    const string& equityVolDayCounter(const string& key) const;
    const string& equityVolDecayMode() const { return equityVolDecayMode_; }
    const vector<string>& equityVolNames() const { return equityVolNames_; }
    const vector<Real>& equityVolMoneyness() const { return equityMoneyness_; }

    const vector<string>& additionalScenarioDataIndices() const { return additionalScenarioDataIndices_; }
    const vector<string>& additionalScenarioDataCcys() const { return additionalScenarioDataCcys_; }

    bool securitySpreadsSimulate() const { return securitySpreadsSimulate_; }
    const vector<string>& securities() const { return securities_; }

    bool simulateBaseCorrelations() const { return baseCorrelationSimulate_; }
    const vector<Period>& baseCorrelationTerms() const { return baseCorrelationTerms_; }
    const vector<Real>& baseCorrelationDetachmentPoints() const { return baseCorrelationDetachmentPoints_; }
    const vector<string>& baseCorrelationNames() const { return baseCorrelationNames_; }
    const string& baseCorrelationDayCounter(const string& key) const;

    const vector<string>& cpiIndices() const { return cpiIndices_; }
    const vector<string>& zeroInflationIndices() const { return zeroInflationIndices_; }
    const string& zeroInflationDayCounter(const string& key) const;
    const vector<Period>& zeroInflationTenors(const string& key) const;
    bool hasZeroInflationTenors(const string& key) const { return zeroInflationTenors_.count(key) > 0; }
    const string& yoyInflationDayCounter(const string& key) const;
    const vector<string>& yoyInflationIndices() const { return yoyInflationIndices_; }
    const vector<Period>& yoyInflationTenors(const string& key) const;
    bool hasYoyInflationTenors(const string& key) const { return yoyInflationTenors_.count(key) > 0; }

    bool simulateEquityForecastCurve() const { return equityForecastCurveSimulate_; }
    bool simulateDividendYield() const { return dividendYieldSimulate_; }

    // Commodity price curve data getters
    bool commodityCurveSimulate() const;
    const std::vector<std::string>& commodityNames() const;
    const std::vector<QuantLib::Period>& commodityCurveTenors(const std::string& commodityName) const;
    bool hasCommodityCurveTenors(const std::string& commodityName) const;
    const std::string& commodityCurveDayCounter(const std::string& commodityName) const;

    // Commodity volatility data getters
    bool commodityVolSimulate() const { return commodityVolSimulate_; }
    const std::string& commodityVolDecayMode() const { return commodityVolDecayMode_; }
    const std::vector<std::string>& commodityVolNames() const { return commodityVolNames_; }
    const std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) const;
    const std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) const;
    const std::string& commodityVolDayCounter(const std::string& commodityName) const;
    //@}

    //! \name Setters
    //@{
    string& baseCcy() { return baseCcy_; }
    vector<string>& ccys() { return ccys_; }
    vector<string>& yieldCurveNames() { return yieldCurveNames_; }
    vector<string>& yieldCurveCurrencies() { return yieldCurveCurrencies_; }
    void setYieldCurveDayCounters(const string& key, const string& p);
    void setYieldCurveTenors(const string& key, const vector<Period>& p);
    vector<string>& indices() { return indices_; }
    map<string, string>& swapIndices() { return swapIndices_; }
    string& interpolation() { return interpolation_; }
    bool& extrapolate() { return extrapolate_; }

    bool& simulateFxSpots() { return fxSpotSimulate_; }
    vector<string>& fxCcyPairs() { return fxCcyPairs_; }

    bool& simulateSwapVols() { return swapVolSimulate_; }
    bool& swapVolIsCube() { return swapVolIsCube_; }
    bool& simulateSwapVolATMOnly() { return swapVolSimulateATMOnly_; }
    vector<Period>& swapVolTerms() { return swapVolTerms_; }
    vector<string>& swapVolCcys() { return swapVolCcys_; }
    vector<Period>& swapVolExpiries() { return swapVolExpiries_; }
    vector<Real>& swapVolStrikeSpreads() { return swapVolStrikeSpreads_; }
    string& swapVolDecayMode() { return swapVolDecayMode_; }
    void setSwapVolDayCounters(const string& key, const string& p);

    bool& simulateCapFloorVols() { return capFloorVolSimulate_; }
    vector<string>& capFloorVolCcys() { return capFloorVolCcys_; }
    void setCapFloorVolExpiries(const string& key, const vector<Period>& p);
    vector<Real>& capFloorVolStrikes() { return capFloorVolStrikes_; }
    string& capFloorVolDecayMode() { return capFloorVolDecayMode_; }
    void setCapFloorVolDayCounters(const string& key, const string& p);

    bool& simulateSurvivalProbabilities() { return survivalProbabilitySimulate_; }
    bool& simulateRecoveryRates() { return recoveryRateSimulate_; }
    vector<string>& defaultNames() { return defaultNames_; }
    void setDefaultTenors(const string& key, const vector<Period>& p);
    void setDefaultCurveDayCounters(const string& key, const string& p);
    void setDefaultCurveCalendars(const string& key, const string& p);

    bool& simulateCdsVols() { return cdsVolSimulate_; }
    vector<Period>& cdsVolExpiries() { return cdsVolExpiries_; }
    vector<string>& cdsVolNames() { return cdsVolNames_; }
    string& cdsVolDecayMode() { return cdsVolDecayMode_; }
    void setCdsVolDayCounters(const string& key, const string& p);

    vector<string>& equityNames() { return equityNames_; }
    void setEquityDividendTenors(const string& key, const vector<Period>& p);
    void setEquityForecastTenors(const string& key, const vector<Period>& p);

    bool& simulateFXVols() { return fxVolSimulate_; }
    bool& fxVolIsSurface() { return fxVolIsSurface_; }
    vector<Period>& fxVolExpiries() { return fxVolExpiries_; }
    string& fxVolDecayMode() { return fxVolDecayMode_; }
    vector<string>& fxVolCcyPairs() { return fxVolCcyPairs_; }
    vector<Real>& fxVolMoneyness() { return fxMoneyness_; }
    void setFxVolDayCounters(const string& key, const string& p);

    bool& simulateEquityVols() { return equityVolSimulate_; }
    bool& equityVolIsSurface() { return equityIsSurface_; }
    bool& simulateEquityVolATMOnly() { return equityVolSimulateATMOnly_; }
    vector<Period>& equityVolExpiries() { return equityVolExpiries_; }
    string& equityVolDecayMode() { return equityVolDecayMode_; }
    vector<string>& equityVolNames() { return equityVolNames_; }
    vector<Real>& equityVolMoneyness() { return equityMoneyness_; }
    void setEquityVolDayCounters(const string& key, const string& p);

    vector<string>& additionalScenarioDataIndices() { return additionalScenarioDataIndices_; }
    vector<string>& additionalScenarioDataCcys() { return additionalScenarioDataCcys_; }

    bool& securitySpreadsSimulate() { return securitySpreadsSimulate_;  }
    vector<string>& securities() { return securities_; }

    bool& simulateBaseCorrelations() { return baseCorrelationSimulate_; }
    vector<Period>& baseCorrelationTerms() { return baseCorrelationTerms_; }
    vector<Real>& baseCorrelationDetachmentPoints() { return baseCorrelationDetachmentPoints_; }
    vector<string>& baseCorrelationNames() { return baseCorrelationNames_; }
    void setBaseCorrelationDayCounters(const string& key, const string& p);

    vector<string>& cpiIndices() { return cpiIndices_; }
    vector<string>& zeroInflationIndices() { return zeroInflationIndices_; }
    void setZeroInflationTenors(const string& key, const vector<Period>& p);
    void setZeroInflationDayCounters(const string& key, const string& p);
    vector<string>& yoyInflationIndices() { return yoyInflationIndices_; }
    void setYoyInflationTenors(const string& key, const vector<Period>& p);
    void setYoyInflationDayCounters(const string& key, const string& p);

    bool& simulateEquityForecastCurve() { return equityForecastCurveSimulate_; }
    bool& simulateDividendYield() { return dividendYieldSimulate_; }

    // Commodity price curve data setters
    bool& commodityCurveSimulate();
    std::vector<std::string>& commodityNames();
    void setCommodityCurveTenors(const std::string& commodityName, const std::vector<QuantLib::Period>& p);
    void setCommodityCurveDayCounter(const std::string& commodityName, const std::string& d);

    // Commodity volatility data setters
    bool& commodityVolSimulate() { return commodityVolSimulate_; }
    std::string& commodityVolDecayMode() { return commodityVolDecayMode_; }
    std::vector<std::string>& commodityVolNames() { return commodityVolNames_; }
    std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) { return commodityVolExpiries_[commodityName]; }
    std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) { return commodityVolMoneyness_[commodityName]; }
    void setCommodityVolDayCounter(const std::string& commodityName, const std::string& d);
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
    string baseCcy_;
    map<string, string> yieldCurveDayCounters_;
    vector<string> ccys_; // may or may not include baseCcy;
    vector<string> yieldCurveNames_;
    vector<string> yieldCurveCurrencies_;
    map<string, vector<Period>> yieldCurveTenors_;
    vector<string> indices_;
    map<string, string> swapIndices_;
    string interpolation_;
    bool extrapolate_;

    bool fxSpotSimulate_;
    vector<string> fxCcyPairs_;

    bool swapVolSimulate_;
    bool swapVolIsCube_;
    bool swapVolSimulateATMOnly_;
    vector<Period> swapVolTerms_;
    vector<string> swapVolCcys_;
    map<string, string> swapVolDayCounters_;
    vector<Period> swapVolExpiries_;
    vector<Real> swapVolStrikeSpreads_;
    string swapVolDecayMode_;

    bool capFloorVolSimulate_;
    vector<string> capFloorVolCcys_;
    map<string, string> capFloorVolDayCounters_;
    map<string, vector<Period>> capFloorVolExpiries_;
    vector<Real> capFloorVolStrikes_;
    string capFloorVolDecayMode_;

    bool survivalProbabilitySimulate_;
    bool recoveryRateSimulate_;
    vector<string> defaultNames_;
    map<string, string> defaultCurveDayCounters_;
    map<string, string> defaultCurveCalendars_;
    map<string, vector<Period>> defaultTenors_;

    bool cdsVolSimulate_;
    vector<string> cdsVolNames_;
    vector<Period> cdsVolExpiries_;
    map<string, string> cdsVolDayCounters_;
    string cdsVolDecayMode_;

    vector<string> equityNames_;
    bool equityForecastCurveSimulate_;
    bool dividendYieldSimulate_;
    map<string, vector<Period>> equityDividendTenors_;
    map<string, vector<Period>> equityForecastTenors_;

    bool fxVolSimulate_;
    bool fxVolIsSurface_;
    vector<Period> fxVolExpiries_;
    map<string, string> fxVolDayCounters_;
    string fxVolDecayMode_;
    vector<string> fxVolCcyPairs_;
    vector<Real> fxMoneyness_;

    bool equityVolSimulate_;
    bool equityIsSurface_;
    bool equityVolSimulateATMOnly_;
    vector<Period> equityVolExpiries_;
    map<string, string> equityVolDayCounters_;
    string equityVolDecayMode_;
    vector<string> equityVolNames_;
    vector<Real> equityMoneyness_;

    vector<string> additionalScenarioDataIndices_;
    vector<string> additionalScenarioDataCcys_;

    bool securitySpreadsSimulate_;
    vector<string> securities_;

    bool baseCorrelationSimulate_;
    vector<string> baseCorrelationNames_;
    vector<Period> baseCorrelationTerms_;
    map<string, string> baseCorrelationDayCounters_;
    vector<Real> baseCorrelationDetachmentPoints_;

    vector<string> cpiIndices_;
    vector<string> zeroInflationIndices_;
    map<string, string> zeroInflationDayCounters_;
    map<string, vector<Period>> zeroInflationTenors_;
    vector<string> yoyInflationIndices_;
    map<string, string> yoyInflationDayCounters_;
    map<string, vector<Period>> yoyInflationTenors_;

    // Commodity price curve data
    bool commodityCurveSimulate_;
    std::vector<std::string> commodityNames_;
    std::map<std::string, std::vector<QuantLib::Period>> commodityCurveTenors_;
    std::map<std::string, std::string> commodityCurveDayCounters_;
    
    // Commodity volatility data
    bool commodityVolSimulate_;
    std::string commodityVolDecayMode_;
    std::vector<std::string> commodityVolNames_;
    std::map<std::string, std::vector<QuantLib::Period>> commodityVolExpiries_;
    std::map<std::string, std::vector<QuantLib::Real>> commodityVolMoneyness_;
    std::map<std::string, std::string> commodityVolDayCounters_;
};
} // namespace analytics
} // namespace ore
