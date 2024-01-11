/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file scenario/sensitivityscenariogenerator.hpp
    \brief Sensitivity scenario generation
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>

#include <map>

namespace ore {
namespace analytics {
using namespace data;

//! Sensitivity Scenario Generator
/*!
  This class builds a vector of sensitivity scenarios based on instructions in SensitivityScenarioData
  and ScenarioSimMarketParameters objects passed.

  The ScenarioSimMarketParameters object determines the scope and structure of a "simulation"
  market (currencies, currency pairs, curve tenor points, vol matrix expiries and terms/strikes etc)
  to which sensitivity scenarios are applied in order to compute their NPV impact.

  The SensitivityScenarioData object determines the structure of shift curves (shift tenor points
  can differ from the simulation market's tenor points), as well as type (relative/absolute) and size
  of shifts applied.

  The generator then produces comprehensive scenarios that can be applied to the simulation market,
  i.e. covering all quotes in the simulation market, possibly filled with "base" scenario values.

  Both UP and DOWN shifts are generated in order to facilitate delta and gamma calculation.

  The generator currently covers the IR/FX asset class, with shifts for the following term
  structure types:
  - FX spot rates
  - FX ATM volatilities
  - Discount curve zero rates
  - Index curve zero rates
  - Yield curve zero rates
  - Swaption ATM volatility matrices
  - Yield volatility matrices
  - Cap/Floor volatility matrices (by expiry and strike)

  For Credit the generator covers shifts to the following termstructure types:
  - Default curve integrated hazard rates.

  To apply shifts to the integrated hazard rates let:
  - S(t) be the survival probability at time t
  - &lambda(t) be the "instantaneous" hazard rate at time t
  - &Lambda(t) be the averaged hazard rate at time t

  The relationship between these three can be expressed as:
  \f[
    S(t) = e^{-&Lambda(t) t}
  \f]
  \f[
    &Lambda(t) = \frac{ \int_{0}^t  &lambda(t) du}{t}
  \f]

  The survival probability quotes from the scenarioSimMarket default curves are then converted as follows:
  \f[
    &Lambda(t) = - \frac{ ln( S(t) )}{t}
  \f]


  Note:
  - For yield curves, the class generates sensitivities in the Zero rate domain only.
  Conversion into par rate sensitivities has to be implemented as a postprocessor step.
  - Likewise, Cap/Floor volatility sensitivities are computed in the optionlet domain.
  Conversion into par (flat cap/floor) volatility sensis has to be implemented as a
  postprocessor step.

  If sensitivityData_->generateSpreadScenarios() = true spread scenarios will be generated for
  supported risk factor types.

  \ingroup scenario
 */
class SensitivityScenarioGenerator : public ShiftScenarioGenerator {
public:
    //! Constructor
    SensitivityScenarioGenerator(const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                 const boost::shared_ptr<Scenario>& baseScenario,
                                 const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                 const boost::shared_ptr<ScenarioSimMarket>& simMarket,
                                 const boost::shared_ptr<ScenarioFactory>& sensiScenarioFactory,
                                 const bool overrideTenors, const bool continueOnError = false,
                                 const boost::shared_ptr<Scenario>& baseScenarioAbsolute = nullptr);
    //! Default destructor
    ~SensitivityScenarioGenerator(){};

    /*! Return the map of absolute shift sizes by risk factor key for this generator

        \warning Where there are tenor specific shifts the shift size is only meaningful
                 if the tenors in the sensitivity configuration line up with the tenors in
                 the simulation market configuration. If this is not the case, an absolute
                 shift size of <code>Null<Real>()</code> is added for the given risk factor
                 key
    */
    const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes() const { return shiftSizes_; }

    /*! Similarly, reeturn the base values for each risk factor */
    const std::map<RiskFactorKey, QuantLib::Real>& baseValues() const { return baseValues_; }

    Size numScenarios() const { return scenarios_.size(); }

    boost::shared_ptr<Scenario> baseScenarioAbsolute() const { return baseScenarioAbsolute_; }

private:
    void generateScenarios();
    void generateYieldCurveScenarios(bool up);
    void generateDiscountCurveScenarios(bool up);
    void generateIndexCurveScenarios(bool up);
    void generateFxScenarios(bool up);
    void generateEquityScenarios(bool up);
    void generateDividendYieldScenarios(bool up);
    void generateSwaptionVolScenarios(bool up);
    void generateYieldVolScenarios(bool up);
    void generateFxVolScenarios(bool up);
    void generateEquityVolScenarios(bool up);
    void generateCapFloorVolScenarios(bool up);
    void generateSurvivalProbabilityScenarios(bool up);
    void generateCdsVolScenarios(bool up);
    void generateZeroInflationScenarios(bool up);
    void generateZeroInflationCapFloorVolScenarios(bool up);
    void generateYoYInflationScenarios(bool up);
    void generateYoYInflationCapFloorVolScenarios(bool up);
    void generateBaseCorrelationScenarios(bool up);
    void generateCommodityCurveScenarios(bool up);
    void generateCommodityVolScenarios(bool up);
    void generateSecuritySpreadScenarios(bool up);
    void generateCorrelationScenarios(bool up);

    // common helper for generateSwaptionVolScenarios(), generateYieldVolScenarios()
    void generateGenericYieldVolScenarios(bool up, RiskFactorKey::KeyType rfType);

    ScenarioDescription discountScenarioDescription(string ccy, Size bucket, bool up);
    ScenarioDescription indexScenarioDescription(string index, Size bucket, bool up);
    ScenarioDescription yieldScenarioDescription(string name, Size bucket, bool up);
    ScenarioDescription fxScenarioDescription(string ccypair, bool up);
    ScenarioDescription fxVolScenarioDescription(string ccypair, Size expiryBucket, Size strikeBucket, bool up);
    ScenarioDescription equityScenarioDescription(string equity, bool up);
    ScenarioDescription dividendYieldScenarioDescription(string equity, Size bucket, bool up);
    ScenarioDescription equityVolScenarioDescription(string equity, Size expiryBucket, Size strikeBucket, bool up);
    ScenarioDescription swaptionVolScenarioDescription(string ccy, Size expiryBucket, Size termBucket,
                                                       Size strikeBucket, bool up);
    ScenarioDescription yieldVolScenarioDescription(string securityId, Size expiryBucket, Size termBucket, bool up);
    ScenarioDescription capFloorVolScenarioDescription(string ccy, Size expiryBucket, Size strikeBucket, bool up,
                                                       bool isAtm);
    ScenarioDescription survivalProbabilityScenarioDescription(string name, Size bucket, bool up);
    ScenarioDescription CdsVolScenarioDescription(string name, Size expiryBucket, Size strikeBucket, bool up);
    ScenarioDescription zeroInflationScenarioDescription(string index, Size bucket, bool up);
    ScenarioDescription yoyInflationScenarioDescription(string index, Size bucket, bool up);
    ScenarioDescription zeroInflationCapFloorVolScenarioDescription(string name, Size expiryBucket, Size strikeBucket,
                                                                    bool up);
    ScenarioDescription yoyInflationCapFloorVolScenarioDescription(string name, Size expiryBucket, Size strikeBucket,
                                                                   bool up);
    ScenarioDescription baseCorrelationScenarioDescription(string indexName, Size lossLevelBucket, Size termBucket,
                                                           bool up);
    ScenarioDescription commodityCurveScenarioDescription(const std::string& commodityName, QuantLib::Size bucket,
                                                          bool up);
    ScenarioDescription commodityVolScenarioDescription(const std::string& commodityName, QuantLib::Size expiryBucket,
                                                        QuantLib::Size strikeBucket, bool up);
    ScenarioDescription securitySpreadScenarioDescription(string bond, bool up);
    ScenarioDescription correlationScenarioDescription(string pair, Size expiryBucket, Size strikeBucket, bool up);

    boost::shared_ptr<SensitivityScenarioData> sensitivityData_;
    boost::shared_ptr<ScenarioFactory> sensiScenarioFactory_;
    const bool overrideTenors_, continueOnError_;

    //! Holds the shift sizes for each risk factor key
    std::map<RiskFactorKey, QuantLib::Real> shiftSizes_;
    //! Holds the base valuesfor each risk factor key
    std::map<RiskFactorKey, QuantLib::Real> baseValues_;

    boost::shared_ptr<Scenario> baseScenarioAbsolute_;
};
} // namespace analytics
} // namespace ore
