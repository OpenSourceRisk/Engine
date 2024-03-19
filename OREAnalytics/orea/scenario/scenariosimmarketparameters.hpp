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
        : swapVolIsCube_({{"", false}}), swapVolStrikeSpreads_({{"", {0.0}}}),
          capFloorVolAdjustOptionletPillars_(false), capFloorVolUseCapAtm_(false), cprSimulate_(false),
          correlationIsSurface_(false), correlationStrikes_({0.0}) {
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

    vector<string> yieldCurveNames() const { return paramsLookup(RiskFactorKey::KeyType::YieldCurve); }
    const map<string, string>& yieldCurveCurrencies() const { return yieldCurveCurrencies_; }
    const vector<Period>& yieldCurveTenors(const string& key) const;
    bool hasYieldCurveTenors(const string& key) const { return yieldCurveTenors_.count(key) > 0; }
    vector<string> indices() const { return paramsLookup(RiskFactorKey::KeyType::IndexCurve); }
    const map<string, string>& swapIndices() const { return swapIndices_; }
    const string& interpolation() const { return interpolation_; }
    const string& extrapolation() const { return extrapolation_; }
    const map<string, vector<Period>>& yieldCurveTenors() const { return yieldCurveTenors_; }

    bool simulateFxSpots() const { return paramsSimulate(RiskFactorKey::KeyType::FXSpot); }
    vector<string> fxCcyPairs() const { return paramsLookup(RiskFactorKey::KeyType::FXSpot); }

    bool simulateSwapVols() const { return paramsSimulate(RiskFactorKey::KeyType::SwaptionVolatility); }
    bool swapVolIsCube(const string& key) const;
    bool simulateSwapVolATMOnly() const { return swapVolSimulateATMOnly_; }
    const vector<Period>& swapVolTerms(const string& key) const;
    const vector<Period>& swapVolExpiries(const string& key) const;
    vector<string> swapVolKeys() const { return paramsLookup(RiskFactorKey::KeyType::SwaptionVolatility); }
    const string& swapVolDecayMode() const { return swapVolDecayMode_; }
    const vector<Real>& swapVolStrikeSpreads(const string& key) const;
    const string& swapVolSmileDynamics(const string& key) const;

    bool simulateYieldVols() const { return paramsSimulate(RiskFactorKey::KeyType::YieldVolatility); }
    const vector<Period>& yieldVolTerms() const { return yieldVolTerms_; }
    const vector<Period>& yieldVolExpiries() const { return yieldVolExpiries_; }
    vector<string> yieldVolNames() const { return paramsLookup(RiskFactorKey::KeyType::YieldVolatility); }
    const string& yieldVolDecayMode() const { return yieldVolDecayMode_; }
    const string& yieldVolSmileDynamics(const string& key) const;

    bool simulateCapFloorVols() const { return paramsSimulate(RiskFactorKey::KeyType::OptionletVolatility); }
    vector<string> capFloorVolKeys() const { return paramsLookup(RiskFactorKey::KeyType::OptionletVolatility); }
    const vector<Period>& capFloorVolExpiries(const string& key) const;
    bool hasCapFloorVolExpiries(const string& key) const { return capFloorVolExpiries_.count(key) > 0; }
    const vector<QuantLib::Rate>& capFloorVolStrikes(const std::string& key) const;
    bool capFloorVolIsAtm(const std::string& key) const;
    const string& capFloorVolDecayMode() const { return capFloorVolDecayMode_; }
    /*! If \c true, the \c capFloorVolExpiries are interpreted as cap maturities and the pillars for the optionlet 
        structure are set equal to the fixing date of the last optionlet on the cap. If \c false, the 
        \c capFloorVolExpiries are the pillars for the optionlet structure.
    */
    bool capFloorVolAdjustOptionletPillars() const { return capFloorVolAdjustOptionletPillars_; }
    /*! If \c true, use ATM cap rate when \c capFloorVolIsAtm is \c true when querying the todaysmarket optionlet 
        volatility structure at the configured expiries. Otherwise, use the index forward rate.
    */
    bool capFloorVolUseCapAtm() const { return capFloorVolUseCapAtm_; }
    const string& capFloorVolSmileDynamics(const string& key) const;

    bool simulateYoYInflationCapFloorVols() const {
        return paramsSimulate(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility);
    }
    vector<string> yoyInflationCapFloorVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility);
    }
    const vector<Period>& yoyInflationCapFloorVolExpiries(const string& key) const;
    bool hasYoYInflationCapFloorVolExpiries(const string& key) const {
        return yoyInflationCapFloorVolExpiries_.count(key) > 0;
    }
    const vector<Real>& yoyInflationCapFloorVolStrikes(const std::string& key) const;
    const string& yoyInflationCapFloorVolDecayMode() const { return yoyInflationCapFloorVolDecayMode_; }
    const string& yoyInflationCapFloorVolSmileDynamics(const string& key) const;

    bool simulateZeroInflationCapFloorVols() const {
        return paramsSimulate(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility);
    }
    vector<string> zeroInflationCapFloorVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility);
    }
    const vector<Period>& zeroInflationCapFloorVolExpiries(const string& key) const;
    bool hasZeroInflationCapFloorVolExpiries(const string& key) const {
        return zeroInflationCapFloorVolExpiries_.count(key) > 0;
    }
    const vector<Real>& zeroInflationCapFloorVolStrikes(const string& key) const;
    const string& zeroInflationCapFloorVolDecayMode() const { return zeroInflationCapFloorVolDecayMode_; }
    const string& zeroInflationCapFloorVolSmileDynamics(const string& key) const;

    bool simulateSurvivalProbabilities() const { return paramsSimulate(RiskFactorKey::KeyType::SurvivalProbability); }
    bool simulateRecoveryRates() const { return paramsSimulate(RiskFactorKey::KeyType::RecoveryRate); }
    vector<string> defaultNames() const { return paramsLookup(RiskFactorKey::KeyType::SurvivalProbability); }
    const string& defaultCurveCalendar(const string& key) const;
    const vector<Period>& defaultTenors(const string& key) const;
    bool hasDefaultTenors(const string& key) const { return defaultTenors_.count(key) > 0; }
    const string& defaultCurveExtrapolation() const { return defaultCurveExtrapolation_; }

    bool simulateCdsVols() const { return paramsSimulate(RiskFactorKey::KeyType::CDSVolatility); }
    bool simulateCdsVolATMOnly() const { return cdsVolSimulateATMOnly_; }    
    const vector<Period>& cdsVolExpiries() const { return cdsVolExpiries_; }
    vector<string> cdsVolNames() const { return paramsLookup(RiskFactorKey::KeyType::CDSVolatility); }
    const string& cdsVolDecayMode() const { return cdsVolDecayMode_; }
    const string& cdsVolSmileDynamics(const string& key) const;

    vector<string> equityNames() const { return paramsLookup(RiskFactorKey::KeyType::EquitySpot); }
    const vector<Period>& equityDividendTenors(const string& key) const;
    bool hasEquityDividendTenors(const string& key) const { return equityDividendTenors_.count(key) > 0; }
    vector<string> equityDividendYields() const { return paramsLookup(RiskFactorKey::KeyType::DividendYield); }
    bool simulateDividendYield() const { return paramsSimulate(RiskFactorKey::KeyType::DividendYield); }

    // fxvol getters
    bool simulateFXVols() const { return paramsSimulate(RiskFactorKey::KeyType::FXVolatility); }
    bool simulateFxVolATMOnly() const { return fxVolSimulateATMOnly_; }    
    bool fxVolIsSurface(const std::string& ccypair) const;
    bool fxUseMoneyness(const std::string& ccypair) const;
    const vector<Period>& fxVolExpiries(const string& key) const;
    const string& fxVolDecayMode() const { return fxVolDecayMode_; }
    vector<string> fxVolCcyPairs() const { return paramsLookup(RiskFactorKey::KeyType::FXVolatility); }
    const vector<Real>& fxVolMoneyness(const string& ccypair) const;
    const vector<Real>& fxVolStdDevs(const string& ccypair) const;
    const string& fxVolSmileDynamics(const string& key) const;
    
    bool simulateEquityVols() const { return paramsSimulate(RiskFactorKey::KeyType::EquityVolatility); }
    bool simulateEquityVolATMOnly() const { return equityVolSimulateATMOnly_; }
    bool equityUseMoneyness(const string& key) const;
    bool equityVolIsSurface(const string& key) const;
    const vector<Period>& equityVolExpiries(const string& key) const;
    const string& equityVolDecayMode() const { return equityVolDecayMode_; }
    vector<string> equityVolNames() const { return paramsLookup(RiskFactorKey::KeyType::EquityVolatility); }
    const vector<Real>& equityVolMoneyness(const string& key) const;
    const vector<Real>& equityVolStandardDevs(const string& key) const;
    const string& equityVolSmileDynamics(const string& key) const;

    const vector<string>& additionalScenarioDataIndices() const { return additionalScenarioDataIndices_; }
    const vector<string>& additionalScenarioDataCcys() const { return additionalScenarioDataCcys_; }
    const Size additionalScenarioDataNumberOfCreditStates() const { return additionalScenarioDataNumberOfCreditStates_; }
    const vector<string>& additionalScenarioDataSurvivalWeights() const { return additionalScenarioDataSurvivalWeights_; }

    bool securitySpreadsSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::SecuritySpread); }
    vector<string> securities() const { return paramsLookup(RiskFactorKey::KeyType::SecuritySpread); }

    bool simulateBaseCorrelations() const { return paramsSimulate(RiskFactorKey::KeyType::BaseCorrelation); }
    const vector<Period>& baseCorrelationTerms() const { return baseCorrelationTerms_; }
    const vector<Real>& baseCorrelationDetachmentPoints() const { return baseCorrelationDetachmentPoints_; }
    vector<string> baseCorrelationNames() const { return paramsLookup(RiskFactorKey::KeyType::BaseCorrelation); }

    vector<string> cpiIndices() const { return paramsLookup(RiskFactorKey::KeyType::CPIIndex); }
    vector<string> zeroInflationIndices() const { return paramsLookup(RiskFactorKey::KeyType::ZeroInflationCurve); }
    const vector<Period>& zeroInflationTenors(const string& key) const;
    bool hasZeroInflationTenors(const string& key) const { return zeroInflationTenors_.count(key) > 0; }
    vector<string> yoyInflationIndices() const { return paramsLookup(RiskFactorKey::KeyType::YoYInflationCurve); }
    const vector<Period>& yoyInflationTenors(const string& key) const;
    bool hasYoyInflationTenors(const string& key) const { return yoyInflationTenors_.count(key) > 0; }

    // Commodity price curve data getters
    bool commodityCurveSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::CommodityCurve); }
    std::vector<std::string> commodityNames() const;
    const std::vector<QuantLib::Period>& commodityCurveTenors(const std::string& commodityName) const;
    bool hasCommodityCurveTenors(const std::string& commodityName) const;

    // Commodity volatility data getters
    bool commodityVolSimulate() const { return paramsSimulate(RiskFactorKey::KeyType::CommodityVolatility); }
    const std::string& commodityVolDecayMode() const { return commodityVolDecayMode_; }
    std::vector<std::string> commodityVolNames() const {
        return paramsLookup(RiskFactorKey::KeyType::CommodityVolatility);
    }
    const std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) const;
    const std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) const;
    const string& commodityVolSmileDynamics(const string& commodityName) const;

    bool simulateCorrelations() const { return paramsSimulate(RiskFactorKey::KeyType::Correlation); }
    bool correlationIsSurface() const { return correlationIsSurface_; }
    const vector<Period>& correlationExpiries() const { return correlationExpiries_; }
    vector<std::string> correlationPairs() const { return paramsLookup(RiskFactorKey::KeyType::Correlation); }
    const vector<Real>& correlationStrikes() const { return correlationStrikes_; }

    Size numberOfCreditStates() const { return numberOfCreditStates_; }

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
    void setYieldCurveTenors(const string& key, const vector<Period>& p);
    void setIndices(vector<string> names);
    map<string, string>& swapIndices() { return swapIndices_; }
    string& interpolation() { return interpolation_; }
    string& extrapolation() { return extrapolation_; }

    void setSimulateFxSpots(bool simulate);
    void setFxCcyPairs(vector<string> names);

    void setSimulateSwapVols(bool simulate);
    void setSwapVolIsCube(const string& key, bool isCube);
    bool& simulateSwapVolATMOnly() { return swapVolSimulateATMOnly_; }
    void setSwapVolTerms(const string& key, const vector<Period>& p);
    void setSwapVolKeys(vector<string> names);
    void setSwapVolExpiries(const string& key, const vector<Period>& p);
    void setSwapVolStrikeSpreads(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& swapVolDecayMode() { return swapVolDecayMode_; }
    void setSwapVolSmileDynamics(const string& key, const string& smileDynamics);
  
    void setSimulateYieldVols(bool simulate);
    vector<Period>& yieldVolTerms() { return yieldVolTerms_; }
    void setYieldVolNames(vector<string> names);
    vector<Period>& yieldVolExpiries() { return yieldVolExpiries_; }
    string& yieldVolDecayMode() { return yieldVolDecayMode_; }
    void setYieldVolSmileDynamics(const string& key, const string& smileDynamics);

    void setSimulateCapFloorVols(bool simulate);
    void setCapFloorVolKeys(vector<string> names);
    void setCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    void setCapFloorVolIsAtm(const std::string& key, bool isAtm);
    string& capFloorVolDecayMode() { return capFloorVolDecayMode_; }
    void setCapFloorVolAdjustOptionletPillars(bool capFloorVolAdjustOptionletPillars) {
        capFloorVolAdjustOptionletPillars_ = capFloorVolAdjustOptionletPillars;
    }
    void setCapFloorVolUseCapAtm(bool capFloorVolUseCapAtm) {
        capFloorVolUseCapAtm_ = capFloorVolUseCapAtm;
    }
    void setCapFloorVolSmileDynamics(const string& key, const string& smileDynamics);

    void setSimulateYoYInflationCapFloorVols(bool simulate);
    void setYoYInflationCapFloorVolNames(vector<string> names);
    void setYoYInflationCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setYoYInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& yoyInflationCapFloorVolDecayMode() { return yoyInflationCapFloorVolDecayMode_; }
    void setYoYInflationCapFloorVolSmileDynamics(const string& key, const string& smileDynamics);

    void setSimulateZeroInflationCapFloorVols(bool simulate);
    void setZeroInflationCapFloorNames(vector<string> names);
    void setZeroInflationCapFloorVolExpiries(const string& key, const vector<Period>& p);
    void setZeroInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    string& zeroInflationCapFloorVolDecayMode() { return zeroInflationCapFloorVolDecayMode_; }
    void setZeroInflationCapFloorVolSmileDynamics(const string& key, const string& smileDynamics);

    void setSimulateSurvivalProbabilities(bool simulate);
    void setSimulateRecoveryRates(bool simulate);
    void setDefaultNames(vector<string> names);
    void setDefaultTenors(const string& key, const vector<Period>& p);
    void setDefaultCurveCalendars(const string& key, const string& p);
    void setDefaultCurveExtrapolation(const std::string& e) { defaultCurveExtrapolation_ = e; }

    void setSimulateCdsVols(bool simulate);
    void setSimulateCdsVolsATMOnly(bool simulateATMOnly) { cdsVolSimulateATMOnly_ = simulateATMOnly; }
    vector<Period>& cdsVolExpiries() { return cdsVolExpiries_; }
    void setCdsVolNames(vector<string> names);
    string& cdsVolDecayMode() { return cdsVolDecayMode_; }
    void setCdsVolSmileDynamics(const string& key, const string& smileDynamics);

    void setEquityNames(vector<string> names);
    void setEquityDividendCurves(vector<string> names);
    void setEquityDividendTenors(const string& key, const vector<Period>& p);
    void setSimulateDividendYield(bool simulate);

    // FX volatility data setters
    void setSimulateFXVols(bool simulate);
    void setSimulateFxVolATMOnly(bool simulateATMOnly) { fxVolSimulateATMOnly_ = simulateATMOnly; }
    void setFxVolIsSurface(const string& ccypair, bool val);
    void setFxVolIsSurface(bool val);
    void setFxVolExpiries(const string& name, const vector<Period>& expiries);
    void setFxVolDecayMode(const string& val);
    void setFxVolCcyPairs(vector<string> names);
    void setFxVolMoneyness(const string& ccypair, const vector<Real>& moneyness);
    void setFxVolMoneyness(const vector<Real>& moneyness);
    void setFxVolStdDevs(const string& ccypair, const vector<Real>& stdDevs);
    void setFxVolStdDevs(const vector<Real>& stdDevs);
    void setFxVolSmileDynamics(const string& name, const string& smileDynamics);

    void setSimulateEquityVols(bool simulate);
    void setSimulateEquityVolATMOnly(bool simulateATMOnly) { equityVolSimulateATMOnly_ = simulateATMOnly; }
    void setEquityVolIsSurface(const string& name, bool isSurface);
    void setEquityVolExpiries(const string& name, const vector<Period>& expiries);
    void setEquityVolDecayMode(const string& val) { equityVolDecayMode_ = val; }
    void setEquityVolNames(vector<string> names);
    void setEquityVolMoneyness(const string&name, const vector<Real>& moneyness);
    void setEquityVolStandardDevs(const string&name, const vector<Real>& standardDevs);
    void setEquityVolSmileDynamics(const string& name, const string& smileDynamics);

    vector<string>& additionalScenarioDataIndices() { return additionalScenarioDataIndices_; }
    void setAdditionalScenarioDataIndices(const vector<string>& asdi) { additionalScenarioDataIndices_ = asdi; }
    vector<string>& additionalScenarioDataCcys() { return additionalScenarioDataCcys_; }
    void setAdditionalScenarioDataCcys(const vector<string>& ccys) { additionalScenarioDataCcys_ = ccys; }
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

    void setCpiIndices(vector<string> names);
    void setZeroInflationIndices(vector<string> names);
    void setZeroInflationTenors(const string& key, const vector<Period>& p);
    void setYoyInflationIndices(vector<string> names);
    void setYoyInflationTenors(const string& key, const vector<Period>& p);

    // Commodity price curve data setters
    void setCommodityCurveSimulate(bool simulate);
    void setCommodityNames(vector<string> names);
    void setCommodityCurves(vector<string> names);
    void setCommodityCurveTenors(const std::string& commodityName, const std::vector<QuantLib::Period>& p);

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
    void setCommodityVolSmileDynamics(const string& key, const string& smileDynamics);

    void setSimulateCorrelations(bool simulate);
    bool& correlationIsSurface() { return correlationIsSurface_; }
    vector<Period>& correlationExpiries() { return correlationExpiries_; }
    void setCorrelationPairs(vector<string> names);
    vector<Real>& correlationStrikes() { return correlationStrikes_; }
    void setNumberOfCreditStates(Size numberOfCreditStates) { numberOfCreditStates_ = numberOfCreditStates; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;
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
    vector<string> ccys_; // may or may not include baseCcy;
    map<string, string> yieldCurveCurrencies_;
    map<string, vector<Period>> yieldCurveTenors_;
    map<string, string> swapIndices_;
    string interpolation_;
    string extrapolation_;

    map<string, bool> swapVolIsCube_;
    bool swapVolSimulateATMOnly_ = false;
    map<string, vector<Period>> swapVolTerms_;
    map<string, vector<Period>> swapVolExpiries_;
    map<string, vector<Real>> swapVolStrikeSpreads_;
    string swapVolDecayMode_;
    map<string, string> swapVolSmileDynamics_;

    vector<Period> yieldVolTerms_;
    vector<Period> yieldVolExpiries_;
    string yieldVolDecayMode_;
    map<string, string> yieldVolSmileDynamics_;

    map<string, vector<Period>> capFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> capFloorVolStrikes_;
    map<std::string, bool> capFloorVolIsAtm_;
    string capFloorVolDecayMode_;
    bool capFloorVolAdjustOptionletPillars_;
    bool capFloorVolUseCapAtm_;
    map<string, string> capFloorVolSmileDynamics_;

    map<string, vector<Period>> yoyInflationCapFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> yoyInflationCapFloorVolStrikes_;
    string yoyInflationCapFloorVolDecayMode_;
    map<string, string> yoyInflationCapFloorVolSmileDynamics_;

    map<string, vector<Period>> zeroInflationCapFloorVolExpiries_;
    map<std::string, std::vector<QuantLib::Rate>> zeroInflationCapFloorVolStrikes_;
    string zeroInflationCapFloorVolDecayMode_;
    map<string, string> zeroInflationCapFloorVolSmileDynamics_;

    map<string, string> defaultCurveCalendars_;
    map<string, vector<Period>> defaultTenors_;
    string defaultCurveExtrapolation_;

    bool cdsVolSimulateATMOnly_ = false;
    vector<Period> cdsVolExpiries_;
    string cdsVolDecayMode_;
    map<string, string> cdsVolSmileDynamics_;

    map<string, vector<Period>> equityDividendTenors_;

    // FX volatility data
    bool fxVolSimulateATMOnly_ = false;
    map<std::string, bool> fxVolIsSurface_;
    map<string, vector<Period>> fxVolExpiries_;
    string fxVolDecayMode_;
    map<string, vector<Real>> fxMoneyness_;
    map<string, vector<Real>> fxStandardDevs_;
    map<string, string> fxVolSmileDynamics_;

    bool equityVolSimulateATMOnly_ = false;
    map<string, bool> equityVolIsSurface_;
    map<string, vector<Period>> equityVolExpiries_;
    string equityVolDecayMode_;
    map<string, vector<Real>> equityMoneyness_;
    map<string, vector<Real>> equityStandardDevs_;
    map<string, string> equityVolSmileDynamics_;

    vector<string> additionalScenarioDataIndices_;
    vector<string> additionalScenarioDataCcys_;
    Size additionalScenarioDataNumberOfCreditStates_ = 0;
    vector<string> additionalScenarioDataSurvivalWeights_;

    bool cprSimulate_;
    vector<string> cprs_;

    vector<Period> baseCorrelationTerms_;
    vector<Real> baseCorrelationDetachmentPoints_;

    map<string, vector<Period>> zeroInflationTenors_;
    map<string, vector<Period>> yoyInflationTenors_;

    // Commodity price curve data
    std::map<std::string, std::vector<QuantLib::Period>> commodityCurveTenors_;

    // Commodity volatility data
    std::string commodityVolDecayMode_;
    std::map<std::string, std::vector<QuantLib::Period>> commodityVolExpiries_;
    std::map<std::string, std::vector<QuantLib::Real>> commodityVolMoneyness_;
    map<string, string> commodityVolSmileDynamics_;

    bool correlationIsSurface_;
    vector<Period> correlationExpiries_;
    vector<Real> correlationStrikes_;
    Size numberOfCreditStates_ = 0;

    // Store sim market params as a map from RiskFactorKey::KeyType to a pair,
    // boolean of whether to simulate and a set of curve names
    std::map<RiskFactorKey::KeyType, std::pair<bool, std::set<std::string>>> params_;
};
} // namespace analytics
} // namespace ore
