/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#ifndef orea_scenariosimmarketparameters.i
#define orea_scenariosimmarketparameters.i

%include stl.i
%include types.i
%include ored_xmlutils.i

%{
using ore::analytics::ScenarioSimMarketParameters;
using ore::analytics::RiskFactorKey;

using ore::data::XMLSerializable;
%}

%shared_ptr(ScenarioSimMarketParameters)
class ScenarioSimMarketParameters {
public:
    ScenarioSimMarketParameters();
    void fromXMLString(const std::string& xmlString);
    void fromFile(const std::string& xmlFileName);
    void toFile(const std::string& filename) const;
    std::string toXMLString() const;

    const std::string& baseCcy() const;
    const std::vector<std::string>& ccys() const;
    std::vector<std::string> paramsLookup(RiskFactorKey::KeyType k) const;
    bool hasParamsName(RiskFactorKey::KeyType kt, std::string name) const;
    void addParamsName(RiskFactorKey::KeyType kt, std::vector<std::string> names);
    bool paramsSimulate(RiskFactorKey::KeyType kt) const;
    void setParamsSimulate(RiskFactorKey::KeyType kt, bool simulate);

    std::vector<std::string> discountCurveNames() const;

    std::vector<std::string> yieldCurveNames() const;
    const map<std::string, std::string>& yieldCurveCurrencies() const;
    const std::vector<QuantLib::Period>& yieldCurveTenors(const std::string& key) const;
    bool hasYieldCurveTenors(const std::string& key) const;
    std::vector<std::string> indices() const;
    const map<std::string, std::string>& swapIndices() const;
    void setSwapIndex(const std::string& key, const std::string& ind);
    const std::string& interpolation() const;
    const std::string& extrapolation() const;
    const map<std::string, std::vector<QuantLib::Period>>& yieldCurveTenors() const;

    bool simulateFxSpots() const;
    std::vector<std::string> fxCcyPairs() const;

    bool simulateSwapVols() const;
    bool swapVolIsCube(const std::string& key) const;
    bool simulateSwapVolATMOnly() const;
    const std::vector<QuantLib::Period>& swapVolTerms(const std::string& key) const;
    const std::vector<QuantLib::Period>& swapVolExpiries(const std::string& key) const;
    std::vector<std::string> swapVolKeys() const;
    void setSwapVolDecayMode(const std::string& s) ;
    const std::vector<QuantLib::Real>& swapVolStrikeSpreads(const std::string& key) const;
    const std::string& swapVolSmileDynamics(const std::string& key) const;

    bool simulateYieldVols() const;
    const std::vector<QuantLib::Period>& yieldVolTerms() const;
    const std::vector<QuantLib::Period>& yieldVolExpiries() const;
    std::vector<std::string> yieldVolNames() const;
    const std::string& yieldVolDecayMode() const;
    const std::string& yieldVolSmileDynamics(const std::string& key) const;

    bool simulateCapFloorVols() const;
    std::vector<std::string> capFloorVolKeys() const;
    const std::vector<QuantLib::Period>& capFloorVolExpiries(const std::string& key) const;
    bool hasCapFloorVolExpiries(const std::string& key) const;
    const std::vector<QuantLib::Rate>& capFloorVolStrikes(const std::string& key) const;
    bool capFloorVolIsAtm(const std::string& key) const;
    const std::string& capFloorVolDecayMode() const;
    bool capFloorVolAdjustOptionletPillars() const;
    bool capFloorVolUseCapAtm() const;
    const std::string& capFloorVolSmileDynamics(const std::string& key) const;

    bool simulateYoYInflationCapFloorVols() const;
    std::vector<std::string> yoyInflationCapFloorVolNames() const;
    const std::vector<QuantLib::Period>& yoyInflationCapFloorVolExpiries(const std::string& key) const;
    bool hasYoYInflationCapFloorVolExpiries(const std::string& key) const;
    const std::vector<QuantLib::Real>& yoyInflationCapFloorVolStrikes(const std::string& key) const;
    const std::string& yoyInflationCapFloorVolDecayMode() const;
    const std::string& yoyInflationCapFloorVolSmileDynamics(const std::string& key) const;

    bool simulateZeroInflationCapFloorVols() const;
    std::vector<std::string> zeroInflationCapFloorVolNames() const;
    const std::vector<QuantLib::Period>& zeroInflationCapFloorVolExpiries(const std::string& key) const;
    bool hasZeroInflationCapFloorVolExpiries(const std::string& key) const;
    const std::vector<QuantLib::Real>& zeroInflationCapFloorVolStrikes(const std::string& key) const;
    const std::string& zeroInflationCapFloorVolDecayMode() const;
    const std::string& zeroInflationCapFloorVolSmileDynamics(const std::string& key) const;

    bool simulateSurvivalProbabilities() const;
    bool simulateRecoveryRates() const;
    std::vector<std::string> defaultNames() const;
    const std::string& defaultCurveCalendar(const std::string& key) const;
    const std::vector<QuantLib::Period>& defaultTenors(const std::string& key) const;
    bool hasDefaultTenors(const std::string& key) const;
    const std::string& defaultCurveExtrapolation() const;

    bool simulateCdsVols() const;
    bool simulateCdsVolATMOnly() const;  
    const std::vector<QuantLib::Period>& cdsVolExpiries() const;
    std::vector<std::string> cdsVolNames() const;
    const std::string& cdsVolDecayMode() const;
    const std::string& cdsVolSmileDynamics(const std::string& key) const;

    std::vector<std::string> equityNames() const;
    const std::vector<QuantLib::Period>& equityDividendTenors(const std::string& key) const;
    bool hasEquityDividendTenors(const std::string& key) const;
    std::vector<std::string> equityDividendYields() const;
    bool simulateDividendYield() const;

    bool simulateFXVols() const;
    bool simulateFxVolATMOnly() const;
    bool fxVolIsSurface(const std::string& ccypair) const;
    bool fxUseMoneyness(const std::string& ccypair) const;
    const std::vector<QuantLib::Period>& fxVolExpiries(const std::string& key) const;
    const std::string& fxVolDecayMode() const;
    std::vector<std::string> fxVolCcyPairs() const;
    const std::vector<QuantLib::Real>& fxVolMoneyness(const std::string& ccypair) const;
    const std::vector<QuantLib::Real>& fxVolStdDevs(const std::string& ccypair) const;
    const std::string& fxVolSmileDynamics(const std::string& key) const;
    
    bool simulateEquityVols() const;
    bool simulateEquityVolATMOnly() const;
    bool equityUseMoneyness(const std::string& key) const;
    bool equityVolIsSurface(const std::string& key) const;
    const std::vector<QuantLib::Period>& equityVolExpiries(const std::string& key) const;
    const std::string& equityVolDecayMode() const;
    std::vector<std::string> equityVolNames() const;
    const std::vector<QuantLib::Real>& equityVolMoneyness(const std::string& key) const;
    const std::vector<QuantLib::Real>& equityVolStandardDevs(const std::string& key) const;
    const std::string& equityVolSmileDynamics(const std::string& key) const;

    const std::vector<std::string>& additionalScenarioDataIndices() const;
    const std::vector<std::string>& additionalScenarioDataCcys() const;
    const QuantLib::Size additionalScenarioDataNumberOfCreditStates() const;
    const std::vector<std::string>& additionalScenarioDataSurvivalWeights() const;

    bool securitySpreadsSimulate() const;
    std::vector<std::string> securities() const;

    bool simulateBaseCorrelations() const;
    const std::vector<QuantLib::Period>& baseCorrelationTerms() const;
    const std::vector<QuantLib::Real>& baseCorrelationDetachmentPoints() const;
    std::vector<std::string> baseCorrelationNames() const;

    std::vector<std::string> cpiIndices() const;
    std::vector<std::string> zeroInflationIndices() const;
    const std::vector<QuantLib::Period>& zeroInflationTenors(const std::string& key) const;
    bool hasZeroInflationTenors(const std::string& key) const;
    std::vector<std::string> yoyInflationIndices() const;
    const std::vector<QuantLib::Period>& yoyInflationTenors(const std::string& key) const;
    bool hasYoyInflationTenors(const std::string& key) const;
    
    bool commodityCurveSimulate() const;
    std::vector<std::string> commodityNames() const;
    const std::vector<QuantLib::Period>& commodityCurveTenors(const std::string& commodityName) const;
    bool hasCommodityCurveTenors(const std::string& commodityName) const;

    bool commodityVolSimulate() const;
    const std::string& commodityVolDecayMode() const;
    std::vector<std::string> commodityVolNames() const;
    const std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName) const;
    const std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName) const;
    const std::string& commodityVolSmileDynamics(const std::string& commodityName) const;

    bool simulateCorrelations() const;
    bool correlationIsSurface() const;
    const std::vector<QuantLib::Period>& correlationExpiries() const;
    std::vector<std::string> correlationPairs() const;
    const std::vector<QuantLib::Real>& correlationStrikes() const;

    QuantLib::Size numberOfCreditStates() const;

    const std::map<RiskFactorKey::KeyType, std::pair<bool, std::set<std::string>>>& parameters() const;
        
    std::vector<std::string>& ccys();
    void setDiscountCurveNames(std::vector<std::string> names);
    void setYieldCurveNames(std::vector<std::string> names);
    std::map<std::string, std::string>& yieldCurveCurrencies();
    void setYieldCurveCurrency(const std::string& key, const std::string& ccy);
    void setYieldCurveTenors(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setIndices(std::vector<std::string> names);
    map<std::string, std::string>& swapIndices();

    void setSimulateFxSpots(bool simulate);
    void setFxCcyPairs(std::vector<std::string> names);

    void setSimulateSwapVols(bool simulate);
    void setSimulateSwapVolATMOnly(const bool b);
    void setSwapVolIsCube(const std::string& key, bool isCube);
    void setSwapVolTerms(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setSwapVolKeys(std::vector<std::string> names);
    void setSwapVolExpiries(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setSwapVolStrikeSpreads(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    std::string& swapVolDecayMode();
    void setSwapVolSmileDynamics(const std::string& key, const std::string& smileDynamics);
  
    void setSimulateYieldVols(bool simulate);
    std::vector<QuantLib::Period>& yieldVolTerms();
    void setYieldVolNames(std::vector<std::string> names);
    std::vector<QuantLib::Period>& yieldVolExpiries();
    std::string& yieldVolDecayMode();
    void setYieldVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setSimulateCapFloorVols(bool simulate);
    void setCapFloorVolKeys(std::vector<std::string> names);
    void setCapFloorVolExpiries(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    void setCapFloorVolIsAtm(const std::string& key, bool isAtm);
    std::string& capFloorVolDecayMode();
    void setCapFloorVolAdjustOptionletPillars(bool capFloorVolAdjustOptionletPillars);
    void setCapFloorVolUseCapAtm(bool capFloorVolUseCapAtm);
    void setCapFloorVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setSimulateYoYInflationCapFloorVols(bool simulate);
    void setYoYInflationCapFloorVolNames(std::vector<std::string> names);
    void setYoYInflationCapFloorVolExpiries(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setYoYInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    std::string& yoyInflationCapFloorVolDecayMode();
    void setYoYInflationCapFloorVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setSimulateZeroInflationCapFloorVols(bool simulate);
    void setZeroInflationCapFloorNames(std::vector<std::string> names);
    void setZeroInflationCapFloorVolExpiries(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setZeroInflationCapFloorVolStrikes(const std::string& key, const std::vector<QuantLib::Rate>& strikes);
    std::string& zeroInflationCapFloorVolDecayMode();
    void setZeroInflationCapFloorVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setSimulateSurvivalProbabilities(bool simulate);
    void setSimulateRecoveryRates(bool simulate);
    void setDefaultNames(std::vector<std::string> names);
    void setDefaultTenors(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setDefaultCurveCalendars(const std::string& key, const std::string& p);
    void setDefaultCurveExtrapolation(const std::string& e);

    void setSimulateCdsVols(bool simulate);
    void setSimulateCdsVolsATMOnly(bool simulateATMOnly);
    std::vector<QuantLib::Period>& cdsVolExpiries();
    void setCdsVolNames(std::vector<std::string> names);
    std::string& cdsVolDecayMode();
    void setCdsVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setEquityNames(std::vector<std::string> names);
    void setEquityDividendCurves(std::vector<std::string> names);
    void setEquityDividendTenors(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setSimulateDividendYield(bool simulate);

    void setSimulateFXVols(bool simulate);
    void setSimulateFxVolATMOnly(bool simulateATMOnly);
    void setFxVolIsSurface(const std::string& ccypair, bool val);
    void setFxVolIsSurface(bool val);
    void setFxVolExpiries(const std::string& name, const std::vector<QuantLib::Period>& expiries);
    void setFxVolDecayMode(const std::string& val);
    void setFxVolCcyPairs(std::vector<std::string> names);
    void setFxVolMoneyness(const std::string& ccypair, const std::vector<QuantLib::Real>& moneyness);
    void setFxVolMoneyness(const std::vector<QuantLib::Real>& moneyness);
    void setFxVolStdDevs(const std::string& ccypair, const std::vector<QuantLib::Real>& stdDevs);
    void setFxVolStdDevs(const std::vector<QuantLib::Real>& stdDevs);
    void setFxVolSmileDynamics(const std::string& name, const std::string& smileDynamics);

    void setSimulateEquityVols(bool simulate);
    void setSimulateEquityVolATMOnly(bool simulateATMOnly);
    void setEquityVolIsSurface(const std::string& name, bool isSurface);
    void setEquityVolExpiries(const std::string& name, const std::vector<QuantLib::Period>& expiries);
    void setEquityVolDecayMode(const std::string& val);
    void setEquityVolNames(std::vector<std::string> names);
    void setEquityVolMoneyness(const std::string&name, const std::vector<QuantLib::Real>& moneyness);
    void setEquityVolStandardDevs(const std::string&name, const std::vector<QuantLib::Real>& standardDevs);
    void setEquityVolSmileDynamics(const std::string& name, const std::string& smileDynamics);

    std::vector<std::string>& additionalScenarioDataIndices();
    void setAdditionalScenarioDataIndices(const std::vector<std::string>& asdi);
    std::vector<std::string>& additionalScenarioDataCcys();
    void setAdditionalScenarioDataCcys(const std::vector<std::string>& ccys);
    void setAdditionalScenarioDataNumberOfCreditStates(QuantLib::Size n);
    void setSecuritySpreadsSimulate(bool simulate);
    void setSecurities(std::vector<std::string> names);
    void setRecoveryRates(std::vector<std::string> names);

    void setCprs(const std::vector<std::string>& names);
    void setSimulateCprs(bool simulate);
    bool simulateCprs() const;
    const std::vector<std::string>& cprs() const;

    void setSimulateBaseCorrelations(bool simulate);
    std::vector<QuantLib::Period>& baseCorrelationTerms();
    std::vector<QuantLib::Real>& baseCorrelationDetachmentPoints();
    void setBaseCorrelationNames(std::vector<std::string> names);

    void setCpiIndices(std::vector<std::string> names);
    void setZeroInflationIndices(std::vector<std::string> names);
    void setZeroInflationTenors(const std::string& key, const std::vector<QuantLib::Period>& p);
    void setYoyInflationIndices(std::vector<std::string> names);
    void setYoyInflationTenors(const std::string& key, const std::vector<QuantLib::Period>& p);

    void setCommodityCurveSimulate(bool simulate);
    void setCommodityNames(std::vector<std::string> names);
    void setCommodityCurves(std::vector<std::string> names);
    void setCommodityCurveTenors(const std::string& commodityName, const std::vector<QuantLib::Period>& p);

    void setCommodityVolSimulate(bool simulate);
    std::string& commodityVolDecayMode();
    void setCommodityVolNames(std::vector<std::string> names);
    std::vector<QuantLib::Period>& commodityVolExpiries(const std::string& commodityName);
    std::vector<QuantLib::Real>& commodityVolMoneyness(const std::string& commodityName);
    void setCommodityVolSmileDynamics(const std::string& key, const std::string& smileDynamics);

    void setSimulateCorrelations(bool simulate);
    bool& correlationIsSurface();
    std::vector<QuantLib::Period>& correlationExpiries();
    void setCorrelationPairs(std::vector<std::string> names);
    std::vector<QuantLib::Real>& correlationStrikes();
    void setNumberOfCreditStates(QuantLib::Size numberOfCreditStates);
    bool operator==(const ScenarioSimMarketParameters& rhs);
    bool operator!=(const ScenarioSimMarketParameters& rhs);
    
    %extend {
      void setBaseCcy(std::string ccy) {
          self->baseCcy() = ccy;
      }
    }
    %extend {
      void setInterpolation(std::string interp) {
          self->interpolation() = interp;
      }
    }
    %extend {
      void setExtrapolation(std::string extrap) {
          self->extrapolation() = extrap;
      }
    }
};

#endif
