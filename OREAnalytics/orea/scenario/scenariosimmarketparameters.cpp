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

#include <set>

#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

using namespace QuantLib;
using namespace ore::data;
using boost::adaptors::transformed;
using boost::algorithm::join;
using std::set;

namespace ore {
namespace analytics {

namespace {

template <typename T> const vector<T>& lookup(const map<string, vector<T>>& m, const string& k) {
    if (m.count(k) > 0) {
        return m.at(k);
    } else if (m.count(std::string()) > 0) {
        return m.at(std::string());
    } else
        QL_FAIL("ScenarioSimMarketParameters: no vector for key \"" << k << "\" found.");
}

template <typename T> const T& lookup(const map<string, T>& m, const string& k) {
    if (m.count(k) > 0) {
        return m.at(k);
    } else if (m.count(std::string()) > 0) {
        return m.at(std::string());
    } else
        QL_FAIL("ScenarioSimMarketParameters: no result for key \"" << k << "\" found.");
}

} // namespace

vector<string> ScenarioSimMarketParameters::paramsLookup(RiskFactorKey::KeyType kt) const {
    vector<string> names;
    auto it = params_.find(kt);
    if (it != params_.end()) {
        for (auto n : it->second.second)
            names.push_back(n);
    }
    return names;
}

bool ScenarioSimMarketParameters::hasParamsName(RiskFactorKey::KeyType kt, string name) const {
    auto it = params_.find(kt);
    if (it != params_.end()) {
        return std::find(it->second.second.begin(), it->second.second.end(), name) == it->second.second.end() ? false
                                                                                                              : true;
    }
    return false;
}

void ScenarioSimMarketParameters::addParamsName(RiskFactorKey::KeyType kt, vector<string> names) {
    // check if key type exists - if doesn't exist set simulate to true first
    if (names.size() > 0) {
        auto it = params_.find(kt);
        if (it == params_.end())
            params_[kt].first = true;
        for (auto name : names) {
            if (!hasParamsName(kt, name))
                params_[kt].second.insert(name);
        }
    }
}

bool ScenarioSimMarketParameters::paramsSimulate(RiskFactorKey::KeyType kt) const {
    bool simulate = false;
    auto it = params_.find(kt);
    if (it != params_.end())
        simulate = it->second.first;
    return simulate;
}

void ScenarioSimMarketParameters::setParamsSimulate(RiskFactorKey::KeyType kt, bool simulate) {
    params_[kt].first = simulate;
}

void ScenarioSimMarketParameters::setDefaults() {
    // Set default simulate
    setSimulateDividendYield(false);
    setSimulateSwapVols(false);
    setSimulateYieldVols(false);
    setSimulateCapFloorVols(false);
    setSimulateYoYInflationCapFloorVols(false);
    setSimulateZeroInflationCapFloorVols(false);
    setSimulateSurvivalProbabilities(false);
    setSimulateRecoveryRates(false);
    setSimulateCdsVols(false);
    setSimulateFXVols(false);
    setSimulateEquityVols(false);
    setSimulateBaseCorrelations(false);
    setCommodityCurveSimulate(false);
    setCommodityVolSimulate(false);
    setSecuritySpreadsSimulate(false);
    setSimulateFxSpots(true);
    setSimulateCorrelations(false);

    // set default smile dynamics
    setSwapVolSmileDynamics("", "StickyStrike");
    setYieldVolSmileDynamics("", "StickyStrike");
    setCapFloorVolSmileDynamics("", "StickyStrike");
    setYoYInflationCapFloorVolSmileDynamics("", "StickyStrike");
    setZeroInflationCapFloorVolSmileDynamics("", "StickyStrike");
    setCdsVolSmileDynamics("", "StickyStrike");
    setFxVolSmileDynamics("", "StickyStrike");
    setEquityVolSmileDynamics("", "StickyStrike");
    setCommodityVolSmileDynamics("", "StickyStrike");

    // Set default tenors
    defaultTenors_[""] = vector<Period>();
    equityDividendTenors_[""] = vector<Period>();
    zeroInflationTenors_[""] = vector<Period>();
    yoyInflationTenors_[""] = vector<Period>();
    // Default calendars
    defaultCurveCalendars_[""] = "TARGET";
    // Default fxVol params
    fxVolIsSurface_[""] = false;
    // Defaults for simulate atm only
    setSimulateFxVolATMOnly(false);
    setSimulateEquityVolATMOnly(false);
    simulateSwapVolATMOnly() = false;
    setSimulateCdsVolsATMOnly(false);
    // Default interpolation for yield curves
    interpolation_ = "LogLinear";
    extrapolation_ = "FlatFwd";
    defaultCurveExtrapolation_ = "FlatFwd";
}

void ScenarioSimMarketParameters::reset() {
    ScenarioSimMarketParameters ssmp;
    std::swap(*this, ssmp);
}

const vector<Period>& ScenarioSimMarketParameters::yieldCurveTenors(const string& key) const {
    return lookup(yieldCurveTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::capFloorVolExpiries(const string& key) const {
    return lookup(capFloorVolExpiries_, key);
}

const vector<Rate>& ScenarioSimMarketParameters::capFloorVolStrikes(const string& key) const {
    return lookup(capFloorVolStrikes_, key);
}

bool ScenarioSimMarketParameters::capFloorVolIsAtm(const string& key) const { return lookup(capFloorVolIsAtm_, key); }

const vector<Period>& ScenarioSimMarketParameters::yoyInflationCapFloorVolExpiries(const string& key) const {
    return lookup(yoyInflationCapFloorVolExpiries_, key);
}

const vector<Rate>& ScenarioSimMarketParameters::yoyInflationCapFloorVolStrikes(const string& key) const {
    return lookup(yoyInflationCapFloorVolStrikes_, key);
}

const vector<Period>& ScenarioSimMarketParameters::defaultTenors(const string& key) const {
    return lookup(defaultTenors_, key);
}

const string& ScenarioSimMarketParameters::defaultCurveCalendar(const string& key) const {
    return lookup(defaultCurveCalendars_, key);
}

bool ScenarioSimMarketParameters::swapVolIsCube(const string& key) const { return lookup(swapVolIsCube_, key); }

const string& ScenarioSimMarketParameters::swapVolSmileDynamics(const string& key) const {
    return lookup(swapVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::yieldVolSmileDynamics(const string& key) const {
    return lookup(yieldVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::capFloorVolSmileDynamics(const string& key) const {
    return lookup(capFloorVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::yoyInflationCapFloorVolSmileDynamics(const string& key) const {
    return lookup(yoyInflationCapFloorVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::zeroInflationCapFloorVolSmileDynamics(const string& key) const {
    return lookup(zeroInflationCapFloorVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::cdsVolSmileDynamics(const string& key) const {
    return lookup(cdsVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::fxVolSmileDynamics(const string& key) const {
    return lookup(fxVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::equityVolSmileDynamics(const string& key) const {
    return lookup(equityVolSmileDynamics_, key);
}
const string& ScenarioSimMarketParameters::commodityVolSmileDynamics(const string& key) const {
    return lookup(commodityVolSmileDynamics_, key);
}

const vector<Period>& ScenarioSimMarketParameters::swapVolTerms(const string& key) const {
    return lookup(swapVolTerms_, key);
}

const vector<Period>& ScenarioSimMarketParameters::swapVolExpiries(const string& key) const {
    return lookup(swapVolExpiries_, key);
}

const vector<Real>& ScenarioSimMarketParameters::swapVolStrikeSpreads(const string& key) const {
    return lookup(swapVolStrikeSpreads_, key);
}

const vector<Period>& ScenarioSimMarketParameters::zeroInflationCapFloorVolExpiries(const string& key) const {
    return lookup(zeroInflationCapFloorVolExpiries_, key);
}

const vector<Rate>& ScenarioSimMarketParameters::zeroInflationCapFloorVolStrikes(const string& key) const {
    return lookup(zeroInflationCapFloorVolStrikes_, key);
}

const vector<Period>& ScenarioSimMarketParameters::equityDividendTenors(const string& key) const {
    return lookup(equityDividendTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::zeroInflationTenors(const string& key) const {
    return lookup(zeroInflationTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::yoyInflationTenors(const string& key) const {
    return lookup(yoyInflationTenors_, key);
}

vector<string> ScenarioSimMarketParameters::commodityNames() const {
    return paramsLookup(RiskFactorKey::KeyType::CommodityCurve);
}

const vector<Period>& ScenarioSimMarketParameters::commodityCurveTenors(const string& commodityName) const {
    return lookup(commodityCurveTenors_, commodityName);
}

bool ScenarioSimMarketParameters::hasCommodityCurveTenors(const string& commodityName) const {
    return commodityCurveTenors_.count(commodityName) > 0;
}

const vector<Period>& ScenarioSimMarketParameters::commodityVolExpiries(const string& commodityName) const {
    return lookup(commodityVolExpiries_, commodityName);
}

const vector<Real>& ScenarioSimMarketParameters::fxVolMoneyness(const string& ccypair) const {
    return lookup(fxMoneyness_, ccypair);
}

const vector<Real>& ScenarioSimMarketParameters::fxVolStdDevs(const string& ccypair) const {
    return lookup(fxStandardDevs_, ccypair);
}

bool ScenarioSimMarketParameters::fxVolIsSurface(const string& ccypair) const {
    return lookup(fxVolIsSurface_, ccypair);
}

bool ScenarioSimMarketParameters::fxUseMoneyness(const string& ccypair) const {
    try {
        const vector<Real> temp = lookup(fxMoneyness_, ccypair);
        if (temp.size() > 0)
            return true;
    } catch (...) {
    }
    return false;
}

const vector<Real>& ScenarioSimMarketParameters::commodityVolMoneyness(const string& commodityName) const {
    if (commodityVolMoneyness_.count(commodityName) > 0) {
        return commodityVolMoneyness_.at(commodityName);
    } else {
        QL_FAIL("no moneyness for commodity \"" << commodityName << "\" found.");
    }
}

void ScenarioSimMarketParameters::setYieldCurveTenors(const string& key, const std::vector<Period>& p) {
    yieldCurveTenors_[key] = p;
}

void ScenarioSimMarketParameters::setSwapVolIsCube(const string& key, bool isCube) { swapVolIsCube_[key] = isCube; }

void ScenarioSimMarketParameters::setSwapVolSmileDynamics(const string& key, const string& smileDynamics) {
    swapVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setCdsVolSmileDynamics(const string& key, const string& smileDynamics) {
    cdsVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setCapFloorVolSmileDynamics(const string& key, const string& smileDynamics) {
    capFloorVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setYieldVolSmileDynamics(const string& key, const string& smileDynamics) {
    yieldVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setZeroInflationCapFloorVolSmileDynamics(const string& key,
                                                                           const string& smileDynamics) {
    zeroInflationCapFloorVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setYoYInflationCapFloorVolSmileDynamics(const string& key,
                                                                          const string& smileDynamics) {
    yoyInflationCapFloorVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setEquityVolSmileDynamics(const string& key, const string& smileDynamics) {
    equityVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setFxVolSmileDynamics(const string& key, const string& smileDynamics) {
    fxVolSmileDynamics_[key] = smileDynamics;
}
void ScenarioSimMarketParameters::setCommodityVolSmileDynamics(const string& key, const string& smileDynamics) {
    commodityVolSmileDynamics_[key] = smileDynamics;
}

void ScenarioSimMarketParameters::setSwapVolTerms(const string& key, const vector<Period>& p) {
    swapVolTerms_[key] = p;
}

void ScenarioSimMarketParameters::setSwapVolExpiries(const string& key, const vector<Period>& p) {
    swapVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setSwapVolStrikeSpreads(const std::string& key,
                                                          const std::vector<QuantLib::Rate>& strikes) {
    setSwapVolIsCube(key, strikes.size() > 1);
    swapVolStrikeSpreads_[key] = strikes;
}

void ScenarioSimMarketParameters::setCapFloorVolExpiries(const string& key, const std::vector<Period>& p) {
    capFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setCapFloorVolStrikes(const string& key, const vector<Rate>& strikes) {
    // An empty vector of strikes signifies ATM
    capFloorVolIsAtm_[key] = strikes.empty();
    capFloorVolStrikes_[key] = strikes;
}

void ScenarioSimMarketParameters::setCapFloorVolIsAtm(const string& key, bool isAtm) {
    capFloorVolIsAtm_[key] = isAtm;
    if (isAtm) {
        // An empty vector of strikes signifies ATM. If isAtm is false, user is expected to have
        // provided the strikes by calling setCapFloorVolStrikes.
        capFloorVolStrikes_[key] = vector<Rate>();
    }
}

void ScenarioSimMarketParameters::setDefaultTenors(const string& key, const std::vector<Period>& p) {
    defaultTenors_[key] = p;
}

void ScenarioSimMarketParameters::setDefaultCurveCalendars(const string& key, const string& s) {
    defaultCurveCalendars_[key] = s;
}

void ScenarioSimMarketParameters::setEquityDividendTenors(const string& key, const std::vector<Period>& p) {
    equityDividendTenors_[key] = p;
}

void ScenarioSimMarketParameters::setZeroInflationTenors(const string& key, const std::vector<Period>& p) {
    zeroInflationTenors_[key] = p;
}

void ScenarioSimMarketParameters::setYoyInflationTenors(const string& key, const std::vector<Period>& p) {
    yoyInflationTenors_[key] = p;
}

const vector<Period>& ScenarioSimMarketParameters::fxVolExpiries(const string& key) const {
    return lookup(fxVolExpiries_, key);
}

void ScenarioSimMarketParameters::setFxVolIsSurface(const string& key, bool val) { fxVolIsSurface_[key] = val; }

void ScenarioSimMarketParameters::setFxVolIsSurface(bool val) { fxVolIsSurface_[""] = val; }

void ScenarioSimMarketParameters::setFxVolExpiries(const string& key, const vector<Period>& expiries) {
    fxVolExpiries_[key] = expiries;
}

void ScenarioSimMarketParameters::setFxVolDecayMode(const string& val) { fxVolDecayMode_ = val; }

void ScenarioSimMarketParameters::setFxVolMoneyness(const string& ccypair, const vector<Real>& moneyness) {
    fxMoneyness_[ccypair] = moneyness;
}

void ScenarioSimMarketParameters::setFxVolMoneyness(const vector<Real>& moneyness) { fxMoneyness_[""] = moneyness; }

void ScenarioSimMarketParameters::setFxVolStdDevs(const string& ccypair, const vector<Real>& moneyness) {
    fxStandardDevs_[ccypair] = moneyness;
}

void ScenarioSimMarketParameters::setFxVolStdDevs(const vector<Real>& moneyness) { fxStandardDevs_[""] = moneyness; }

void ScenarioSimMarketParameters::setCommodityNames(vector<string> names) { setCommodityCurves(names); }

void ScenarioSimMarketParameters::setCommodityCurveTenors(const string& commodityName, const vector<Period>& p) {
    commodityCurveTenors_[commodityName] = p;
}

void ScenarioSimMarketParameters::setDiscountCurveNames(vector<string> names) {
    ccys_ = names;
    addParamsName(RiskFactorKey::KeyType::DiscountCurve, names);
}

void ScenarioSimMarketParameters::setYieldCurveNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::YieldCurve, names);
}

void ScenarioSimMarketParameters::setIndices(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::IndexCurve, names);
}

void ScenarioSimMarketParameters::setFxCcyPairs(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::FXSpot, names);
}

void ScenarioSimMarketParameters::setSwapVolKeys(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::SwaptionVolatility, names);
}

void ScenarioSimMarketParameters::setYieldVolNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::YieldVolatility, names);
}

void ScenarioSimMarketParameters::setCapFloorVolKeys(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::OptionletVolatility, names);
}

void ScenarioSimMarketParameters::setYoYInflationCapFloorVolNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, names);
}

void ScenarioSimMarketParameters::setZeroInflationCapFloorNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, names);
}

void ScenarioSimMarketParameters::setZeroInflationCapFloorVolExpiries(const string& key, const std::vector<Period>& p) {
    zeroInflationCapFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setZeroInflationCapFloorVolStrikes(const string& key, const vector<Rate>& strikes) {
    zeroInflationCapFloorVolStrikes_[key] = strikes;
}

void ScenarioSimMarketParameters::setDefaultNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::SurvivalProbability, names);
    setRecoveryRates(names);
}

void ScenarioSimMarketParameters::setCdsVolNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::CDSVolatility, names);
}

void ScenarioSimMarketParameters::setEquityNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::EquitySpot, names);
    setEquityDividendCurves(names);
}

void ScenarioSimMarketParameters::setEquityDividendCurves(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::DividendYield, names);
}

void ScenarioSimMarketParameters::setFxVolCcyPairs(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::FXVolatility, names);
}

void ScenarioSimMarketParameters::setEquityVolNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::EquityVolatility, names);
}

void ScenarioSimMarketParameters::setSecurities(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::SecuritySpread, names);
}

void ScenarioSimMarketParameters::setRecoveryRates(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::RecoveryRate, names);
}

void ScenarioSimMarketParameters::setBaseCorrelationNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::BaseCorrelation, names);
}

void ScenarioSimMarketParameters::setCpiIndices(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::CPIIndex, names);
}

void ScenarioSimMarketParameters::setZeroInflationIndices(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::ZeroInflationCurve, names);
}

void ScenarioSimMarketParameters::setYoyInflationIndices(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::YoYInflationCurve, names);
}

void ScenarioSimMarketParameters::setCommodityVolNames(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::CommodityVolatility, names);
}

void ScenarioSimMarketParameters::setCommodityCurves(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::CommodityCurve, names);
}

void ScenarioSimMarketParameters::setCorrelationPairs(vector<string> names) {
    addParamsName(RiskFactorKey::KeyType::Correlation, names);
}

void ScenarioSimMarketParameters::setCprs(const vector<string>& names) {
    addParamsName(RiskFactorKey::KeyType::CPR, names);
}

void ScenarioSimMarketParameters::setSimulateDividendYield(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::DividendYield, simulate);
}

void ScenarioSimMarketParameters::setSimulateSwapVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::SwaptionVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateYieldVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::YieldVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateCapFloorVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::OptionletVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateYoYInflationCapFloorVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, simulate);
}

void ScenarioSimMarketParameters::setYoYInflationCapFloorVolExpiries(const string& key, const vector<Period>& p) {
    yoyInflationCapFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setYoYInflationCapFloorVolStrikes(const string& key, const vector<Rate>& strikes) {
    yoyInflationCapFloorVolStrikes_[key] = strikes;
}

void ScenarioSimMarketParameters::setSimulateZeroInflationCapFloorVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateSurvivalProbabilities(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::SurvivalProbability, simulate);
}

void ScenarioSimMarketParameters::setSimulateRecoveryRates(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::RecoveryRate, simulate);
}

void ScenarioSimMarketParameters::setSimulateCdsVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::CDSVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateFXVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::FXVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateEquityVols(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::EquityVolatility, simulate);
}

void ScenarioSimMarketParameters::setSimulateBaseCorrelations(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::BaseCorrelation, simulate);
}

void ScenarioSimMarketParameters::setCommodityCurveSimulate(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::CommodityCurve, simulate);
}

void ScenarioSimMarketParameters::setCommodityVolSimulate(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::CommodityVolatility, simulate);
}

void ScenarioSimMarketParameters::setSecuritySpreadsSimulate(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::SecuritySpread, simulate);
}

void ScenarioSimMarketParameters::setSimulateFxSpots(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::FXSpot, simulate);
}

void ScenarioSimMarketParameters::setSimulateCorrelations(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::Correlation, simulate);
}

void ScenarioSimMarketParameters::setSimulateCprs(bool simulate) {
    setParamsSimulate(RiskFactorKey::KeyType::CPR, simulate);
}

void ScenarioSimMarketParameters::setEquityVolIsSurface(const string& name, bool isSurface) {
    equityVolIsSurface_[name] = isSurface;
}

void ScenarioSimMarketParameters::setEquityVolExpiries(const string& name, const vector<Period>& expiries) {
    equityVolExpiries_[name] = expiries;
}

void ScenarioSimMarketParameters::setEquityVolMoneyness(const string&name, const vector<Real>& moneyness) {
    equityMoneyness_[name] = moneyness;
}

void ScenarioSimMarketParameters::setEquityVolStandardDevs(const string&name, const vector<Real>& standardDevs) {
    equityStandardDevs_[name] = standardDevs;
}

bool ScenarioSimMarketParameters::equityUseMoneyness(const string& key) const {
    try {
        const vector<Real> temp = lookup(equityMoneyness_, key);
        if (temp.size() > 0)
            return true;
    } catch (...) {}
    return false;
}

bool ScenarioSimMarketParameters::equityVolIsSurface(const string& key) const {
    return lookup(equityVolIsSurface_, key);
}

const vector<Period>& ScenarioSimMarketParameters::equityVolExpiries(const string& key) const {
    return lookup(equityVolExpiries_, key);
}

const vector<Real>& ScenarioSimMarketParameters::equityVolMoneyness(const string& key) const {
    return lookup(equityMoneyness_, key);
}

const vector<Real>& ScenarioSimMarketParameters::equityVolStandardDevs(const string& key) const {
    return lookup(equityStandardDevs_, key);
}

bool ScenarioSimMarketParameters::operator==(const ScenarioSimMarketParameters& rhs) {

    if (baseCcy_ != rhs.baseCcy_ || ccys_ != rhs.ccys_ || params_ != rhs.params_ ||
        yieldCurveCurrencies_ != rhs.yieldCurveCurrencies_ || yieldCurveTenors_ != rhs.yieldCurveTenors_ ||
        swapIndices_ != rhs.swapIndices_ || interpolation_ != rhs.interpolation_ ||
        extrapolation_ != rhs.extrapolation_ || swapVolTerms_ != rhs.swapVolTerms_ ||
        swapVolIsCube_ != rhs.swapVolIsCube_ || swapVolSimulateATMOnly_ != rhs.swapVolSimulateATMOnly_ ||
        swapVolExpiries_ != rhs.swapVolExpiries_ || swapVolStrikeSpreads_ != rhs.swapVolStrikeSpreads_ ||
        swapVolDecayMode_ != rhs.swapVolDecayMode_ || capFloorVolExpiries_ != rhs.capFloorVolExpiries_ ||
        capFloorVolStrikes_ != rhs.capFloorVolStrikes_ ||
        zeroInflationCapFloorVolExpiries_ != rhs.zeroInflationCapFloorVolExpiries_ ||
        zeroInflationCapFloorVolStrikes_ != rhs.zeroInflationCapFloorVolStrikes_ ||
        zeroInflationCapFloorVolDecayMode_ != rhs.zeroInflationCapFloorVolDecayMode_ ||
        capFloorVolIsAtm_ != rhs.capFloorVolIsAtm_ || capFloorVolDecayMode_ != rhs.capFloorVolDecayMode_ ||
        defaultCurveCalendars_ != rhs.defaultCurveCalendars_ || defaultTenors_ != rhs.defaultTenors_ ||
        defaultCurveExtrapolation_ != rhs.defaultCurveExtrapolation_ || cdsVolExpiries_ != rhs.cdsVolExpiries_ ||
        cdsVolDecayMode_ != rhs.cdsVolDecayMode_ || cdsVolSimulateATMOnly_ != rhs.cdsVolSimulateATMOnly_ ||
        equityDividendTenors_ != rhs.equityDividendTenors_ || fxVolIsSurface_ != rhs.fxVolIsSurface_ ||
        fxVolExpiries_ != rhs.fxVolExpiries_ || fxVolDecayMode_ != rhs.fxVolDecayMode_ ||
        fxVolSimulateATMOnly_ != rhs.fxVolSimulateATMOnly_ || equityVolExpiries_ != rhs.equityVolExpiries_ ||
        equityVolDecayMode_ != rhs.equityVolDecayMode_ || equityVolSimulateATMOnly_ != rhs.equityVolSimulateATMOnly_ ||
        equityMoneyness_ != rhs.equityMoneyness_ || equityStandardDevs_ != rhs.equityStandardDevs_ ||
        additionalScenarioDataIndices_ != rhs.additionalScenarioDataIndices_ ||
        additionalScenarioDataCcys_ != rhs.additionalScenarioDataCcys_ ||
        additionalScenarioDataNumberOfCreditStates_ != rhs.additionalScenarioDataNumberOfCreditStates_ ||
        additionalScenarioDataSurvivalWeights_ != rhs.additionalScenarioDataSurvivalWeights_ ||
        baseCorrelationTerms_ != rhs.baseCorrelationTerms_ ||
        baseCorrelationDetachmentPoints_ != rhs.baseCorrelationDetachmentPoints_ ||
        zeroInflationTenors_ != rhs.zeroInflationTenors_ || yoyInflationTenors_ != rhs.yoyInflationTenors_ ||
        commodityCurveTenors_ != rhs.commodityCurveTenors_ || commodityVolDecayMode_ != rhs.commodityVolDecayMode_ ||
        commodityVolExpiries_ != rhs.commodityVolExpiries_ || commodityVolMoneyness_ != rhs.commodityVolMoneyness_ ||
        correlationIsSurface_ != rhs.correlationIsSurface_ || correlationExpiries_ != rhs.correlationExpiries_ ||
        correlationStrikes_ != rhs.correlationStrikes_ || cprSimulate_ != rhs.cprSimulate_ || cprs_ != rhs.cprs_ ||
        yieldVolTerms_ != rhs.yieldVolTerms_ || yieldVolExpiries_ != rhs.yieldVolExpiries_ ||
        yieldVolDecayMode_ != rhs.yieldVolDecayMode_) {
        return false;
    } else {
        return true;
    }
}

bool ScenarioSimMarketParameters::operator!=(const ScenarioSimMarketParameters& rhs) { return !(*this == rhs); }

void ScenarioSimMarketParameters::fromXML(XMLNode* root) {

    // fromXML always uses a "clean" object
    reset();

    DLOG("ScenarioSimMarketParameters::fromXML()");

    XMLNode* sim = XMLUtils::locateNode(root, "Simulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "Market");
    XMLUtils::checkNode(node, "Market");

    // TODO: add in checks (checkNode or QL_REQUIRE) on mandatory nodes
    DLOG("Loading Currencies");
    baseCcy_ = XMLUtils::getChildValue(node, "BaseCurrency");
    setDiscountCurveNames(XMLUtils::getChildrenValues(node, "Currencies", "Currency"));

    DLOG("Loading BenchmarkCurve");
    XMLNode* nodeChild = XMLUtils::getChildNode(node, "BenchmarkCurves");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        vector<string> yields;
        for (XMLNode* n = XMLUtils::getChildNode(nodeChild, "BenchmarkCurve"); n != nullptr;
             n = XMLUtils::getNextSibling(n, "BenchmarkCurve")) {
            yields.push_back(XMLUtils::getChildValue(n, "Name", true));
            yieldCurveCurrencies_[XMLUtils::getChildValue(n, "Name", true)] =
                XMLUtils::getChildValue(n, "Currency", true);
        }
        setYieldCurveNames(yields);
    }

    DLOG("Loading YieldCurves");
    nodeChild = XMLUtils::getChildNode(node, "YieldCurves");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        for (XMLNode* child = XMLUtils::getChildNode(nodeChild, "Configuration"); child;
             child = XMLUtils::getNextSibling(child)) {

            // If there is no attribute "curve", this returns "" i.e. the default
            string label = XMLUtils::getAttribute(child, "curve");
            if (label == "") {
                yieldCurveTenors_[label] = XMLUtils::getChildrenValuesAsPeriods(child, "Tenors", true);
                if (auto n = XMLUtils::getChildNode(child, "Interpolation")) {
                    interpolation_ = XMLUtils::getNodeValue(n);
                }
                if (auto n = XMLUtils::getChildNode(child, "Extrapolation")) {
                    extrapolation_ = XMLUtils::getNodeValue(n);
                }
                // for backwards compatibility, map an extrapolation value that parses to bool to FlatFwd
                bool dummy;
                if (tryParse<bool>(extrapolation_, dummy, &parseBool)) {
                    WLOG("ScenarioSimMarket parameter Extrapolation should be FlatFwd or FlatZero, mapping deprecated "
                         "boolean '"
                         << extrapolation_ << "' to FlatFwd. Please change this in your configuration.");
                    extrapolation_ = "FlatFwd";
                }
            } else {
                if (XMLUtils::getChildNode(child, "Interpolation")) {
                    WLOG("Only one default interpolation value is allowed for yield curves");
                }
                if (XMLUtils::getChildNode(child, "Extrapolation")) {
                    WLOG("Only one default extrapolation value is allowed for yield curves");
                }
                if (XMLUtils::getChildNode(child, "Tenors")) {
                    yieldCurveTenors_[label] = XMLUtils::getChildrenValuesAsPeriods(child, "Tenors", true);
                }
            }
        }
    }

    DLOG("Loading Libor indices");
    setIndices(XMLUtils::getChildrenValues(node, "Indices", "Index"));

    DLOG("Loading swap indices");
    nodeChild = XMLUtils::getChildNode(node, "SwapIndices");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        for (XMLNode* n = XMLUtils::getChildNode(nodeChild, "SwapIndex"); n != nullptr;
             n = XMLUtils::getNextSibling(n, "SwapIndex")) {
            string name = XMLUtils::getChildValue(n, "Name");
            string disc = XMLUtils::getChildValue(n, "DiscountingIndex");
            swapIndices_[name] = disc;
        }
    }

    DLOG("Loading FX Rates");
    nodeChild = XMLUtils::getChildNode(node, "FxRates");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* fxSpotSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (fxSpotSimNode)
            setSimulateFxSpots(ore::data::parseBool(XMLUtils::getNodeValue(fxSpotSimNode)));
        // if currency pairs are specified load these, otherwise infer from currencies list and base currency
        XMLNode* ccyPairsNode = XMLUtils::getChildNode(nodeChild, "CurrencyPairs");
        if (ccyPairsNode) {
            setFxCcyPairs(XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true));
        } else {
            vector<string> ccys;
            for (auto ccy : ccys_) {
                if (ccy != baseCcy_)
                    ccys.push_back(ccy + baseCcy_);
            }
            setFxCcyPairs(ccys);
        }
    } else {
        // spot simulation turned on by default
        setSimulateFxSpots(true);
        vector<string> ccys;
        for (auto ccy : ccys_) {
            if (ccy != baseCcy_)
                ccys.push_back(ccy + baseCcy_);
        }
        setFxCcyPairs(ccys);
    }

    DLOG("Loading SwaptionVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* swapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (swapVolSimNode)
            setSimulateSwapVols(ore::data::parseBool(XMLUtils::getNodeValue(swapVolSimNode)));
        swapVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

	auto ccys = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", false);
	auto keys = XMLUtils::getChildrenValues(nodeChild, "Keys", "Key", false);
	if(!ccys.empty()) {
	    keys.insert(keys.end(), ccys.begin(), ccys.end());
            WLOG("ScenarioSimMarketParameters: SwaptionVolatilities/Currencies is deprecated, use Keys instead.");
        }
        setSwapVolKeys(keys);
        QL_REQUIRE(!keys.empty(), "SwaptionVolatilities needs at least one currency");

        // Get the configured expiries. They are of the form:
        // - <Expiries ccy="CCY">t_1,...,t_n</Expiries> for currency specific expiries
        // - <Expiries>t_1,...,t_n</Expiries> or <Expiries ccy="">t_1,...,t_n</Expiries> for default set of expiries
        // Only need a default expiry set if every currency has not been given an expiry set explicitly
        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        QL_REQUIRE(!expiryNodes.empty(), "SwaptionVolatilities needs at least one Expiries node");
        set<string> keysCheck(keys.begin(),keys.end());
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            // If there is no "key" attribute, getAttribute returns "" which is what we want in any case
            string key = XMLUtils::getAttribute(expiryNode, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(expiryNode, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("ScenarioSimMarketParameters: SwaptionVolatilities/Expiries: 'ccy' attribute is deprecated, "
                         "use 'key' instead.");
                }
            }
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(swapVolExpiries_.insert(make_pair(key, expiries)).second,
                       "SwaptionVolatilities has duplicate expiries for key '" << key << "'");
            keysCheck.erase(key);
            defaultProvided = defaultProvided || key.empty();
        }
        QL_REQUIRE(defaultProvided || keysCheck.empty(), "SwaptionVolatilities has no expiries for "
                                                                 << "keys '" << join(keysCheck, ",")
                                                                 << "' and no default expiry set has been given");

        // Get the configured terms, similar to expiries above
        vector<XMLNode*> termNodes = XMLUtils::getChildrenNodes(nodeChild, "Terms");
        keysCheck = set<string>(keys.begin(), keys.end());
        defaultProvided = false;
        for (XMLNode* termNode : termNodes) {
            // If there is no "key" attribute, getAttribute returns "" which is what we want in any case
            string key = XMLUtils::getAttribute(termNode, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(termNode, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("ScenarioSimMarketParameters: SwaptionVolatilities/Terms: 'ccy' attribute is deprecated, "
                         "use 'key' instead.");
                }
            }
            vector<Period> terms = parseListOfValues<Period>(XMLUtils::getNodeValue(termNode), &parsePeriod);
            QL_REQUIRE(swapVolTerms_.insert(make_pair(key, terms)).second,
                       "SwaptionVolatilities has duplicate terms for key '" << key << "'");
            keysCheck.erase(key);
            defaultProvided = defaultProvided || key.empty();
        }
        QL_REQUIRE(defaultProvided || keysCheck.empty(), "SwaptionVolatilities has no terms for "
                                                             << "keys '" << join(keysCheck, ",")
                                                             << "' and no default term set has been given");

        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            swapVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }

        XMLNode* atmOnlyNode = XMLUtils::getChildNode(nodeChild, "SimulateATMOnly");
        if (atmOnlyNode)
            swapVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(nodeChild, "SimulateATMOnly", true);

        if (!swapVolSimulateATMOnly_) {
            vector<XMLNode*> spreadNodes = XMLUtils::getChildrenNodes(nodeChild, "StrikeSpreads");
            if (spreadNodes.size() > 0) {
                keysCheck = set<string>(keys.begin(), keys.end());
                defaultProvided = false;
                for (XMLNode* spreadNode : spreadNodes) {
                    // If there is no "ccy" attribute, getAttribute returns "" which is what we want in any case
                    string key = XMLUtils::getAttribute(spreadNode, "key");
                    if (key.empty()) {
                        string ccyAttr = XMLUtils::getAttribute(spreadNode, "ccy");
                        if (!ccyAttr.empty()) {
                            key = ccyAttr;
                            ALOG("ScenarioSimMarketParameters: SwaptionVolatilities/StrikeSpreads: 'ccy' attribute is "
                                 "deprecated, use 'key' instead.");
                        }
                    }
                    vector<Rate> strikes;
                    string strStrike = XMLUtils::getNodeValue(spreadNode);
                    if (strStrike == "ATM" || strStrike == "0" || strStrike == "0.0") {
                        // Add a '0' to the strike spreads
                        strikes = {0.0};
                    } else {
                        strikes = parseListOfValues<Rate>(strStrike, &parseReal);
                    }
                    setSwapVolStrikeSpreads(key, strikes);
                    keysCheck.erase(key);
                    defaultProvided = defaultProvided || key.empty();
                }
                QL_REQUIRE(defaultProvided || keysCheck.empty(),
                           "SwaptionVolatilities has no strike spreads for "
                               << "currencies '" << join(keysCheck, ",")
                               << "' and no default strike spreads set has been given");
            }
        }
    }

    DLOG("Loading YieldVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "YieldVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* yieldVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (yieldVolSimNode) {
            setSimulateYieldVols(ore::data::parseBool(XMLUtils::getNodeValue(yieldVolSimNode)));
            yieldVolTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
            yieldVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
            setYieldVolNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
            yieldVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        }
        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            yieldVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading Correlations");
    nodeChild = XMLUtils::getChildNode(node, "Correlations");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* pn = XMLUtils::getChildNode(nodeChild, "Pairs");
        vector<string> pairs;
        if (pn) {
            for (XMLNode* child = XMLUtils::getChildNode(pn, "Pair"); child; child = XMLUtils::getNextSibling(child)) {
                string p = XMLUtils::getNodeValue(child);
                vector<string> tokens = getCorrelationTokens(p);
                QL_REQUIRE(tokens.size() == 2, "not a valid correlation pair: " << p);
                pairs.push_back(p);
            }
        }
        setCorrelationPairs(pairs);
        XMLNode* correlSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (correlSimNode) {
            setSimulateCorrelations(ore::data::parseBool(XMLUtils::getNodeValue(correlSimNode)));
            correlationExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);

            XMLNode* surfaceNode = XMLUtils::getChildNode(nodeChild, "Surface");
            if (surfaceNode) {
                correlationIsSurface_ = true;
                correlationStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(surfaceNode, "Strikes", true);
            } else {
                correlationIsSurface_ = false;
            }
        }
    }

    DLOG("Loading CapFloorVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {

        // Are we simulating caps
        setSimulateCapFloorVols(false);
        XMLNode* capVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (capVolSimNode)
            setSimulateCapFloorVols(ore::data::parseBool(XMLUtils::getNodeValue(capVolSimNode)));

        // All cap floor keys
	auto ccys = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", false);
	auto keys = XMLUtils::getChildrenValues(nodeChild, "Keys", "Key", false);
	if(!ccys.empty()) {
	    keys.insert(keys.end(), ccys.begin(), ccys.end());
            WLOG("ScenarioSimMarketParameters: CapFloorVolatilities/Currencies is deprecated, use Keys instead.");
        }
        setCapFloorVolKeys(keys);
        QL_REQUIRE(!keys.empty(), "CapFloorVolatilities needs at least one entry");

        // Get the configured expiries. They are of the form:
        // - <Expiries key="CCY">t_1,...,t_n</Expiries> for currency specific expiries
        // - <Expiries>t_1,...,t_n</Expiries> or <Expiries key="">t_1,...,t_n</Expiries> for default set of expiries
        // Only need a default expiry set if every currency has not been given an expiry set explicitly
	// instead of key, ccy is supported as an derprecated attribute
        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        QL_REQUIRE(expiryNodes.size() > 0, "CapFloorVolatilities needs at least one Expiries node");
        set<string> keysCheck(keys.begin(), keys.end());
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            // If there is no "key" attribute, getAttribute returns "" which is what we want in any case
            string key = XMLUtils::getAttribute(expiryNode, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(expiryNode, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("ScenarioSimMarketParameters: CapFloorVolatilities/Expiries: 'ccy' attribute is deprecated, "
                         "use 'key' instead.");
                }
            }
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(capFloorVolExpiries_.insert(make_pair(key, expiries)).second,
                       "CapFloorVolatilities has duplicate expiries for key '" << key << "'");
            keysCheck.erase(key);
            defaultProvided = defaultProvided || key.empty();
        }
        QL_REQUIRE(defaultProvided || keysCheck.size() == 0, "CapFloorVolatilities has no expiries for "
                                                                 << "keys '" << join(keysCheck, ",")
                                                                 << "' and no default expiry set has been given");

        // Get the configured strikes. This has the same set up and logic as the Expiries above.
        vector<XMLNode*> strikeNodes = XMLUtils::getChildrenNodes(nodeChild, "Strikes");
        QL_REQUIRE(strikeNodes.size() > 0, "CapFloorVolatilities needs at least one Strikes node");
        keysCheck = std::set<std::string>(keys.begin(), keys.end());
        defaultProvided = false;
        for (XMLNode* strikeNode : strikeNodes) {
            string key = XMLUtils::getAttribute(strikeNode, "key");
            if (key.empty()) {
                string ccyAttr = XMLUtils::getAttribute(strikeNode, "ccy");
                if (!ccyAttr.empty()) {
                    key = ccyAttr;
                    WLOG("ScenarioSimMarketParameters: CapFloorVolatilities/Strikes: 'ccy' attribute is deprecated, "
                         "use 'key' instead.");
                }
            }
            // For the strike value, we allow ATM or a comma separated list of absolute strike values
            // If ATM, the stored strikes vector is left as an empty vector
            vector<Rate> strikes;
            string strStrike = XMLUtils::getNodeValue(strikeNode);
            if (strStrike == "ATM") {
                QL_REQUIRE(capFloorVolIsAtm_.insert(make_pair(key, true)).second,
                           "CapFloorVolatilities has duplicate strikes for key '" << key << "'");
            } else {
                QL_REQUIRE(capFloorVolIsAtm_.insert(make_pair(key, false)).second,
                           "CapFloorVolatilities has duplicate strikes for key '" << key << "'");
                strikes = parseListOfValues<Rate>(strStrike, &parseReal);
            }
            QL_REQUIRE(capFloorVolStrikes_.insert(make_pair(key, strikes)).second,
                       "CapFloorVolatilities has duplicate strikes for key '" << key << "'");
            keysCheck.erase(key);
            defaultProvided = defaultProvided || key.empty();
        }
        QL_REQUIRE(defaultProvided || keysCheck.empty(), "CapFloorVolatilities has no strikes for "
                                                             << "key '" << join(keysCheck, ",")
                                                             << "' and no default strike set has been given");

        capFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

        capFloorVolAdjustOptionletPillars_ = false;
        if (XMLNode* n = XMLUtils::getChildNode(nodeChild, "AdjustOptionletPillars")) {
            capFloorVolAdjustOptionletPillars_ = parseBool(XMLUtils::getNodeValue(n));
        }

        capFloorVolUseCapAtm_ = false;
        if (XMLNode* n = XMLUtils::getChildNode(nodeChild, "UseCapAtm")) {
            capFloorVolUseCapAtm_ = parseBool(XMLUtils::getNodeValue(n));
        }

        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            capFloorVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading YYCapFloorVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "YYCapFloorVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        // Are we simulating yy caps
        setSimulateYoYInflationCapFloorVols(false);
        XMLNode* yoyCapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (yoyCapVolSimNode)
            setSimulateYoYInflationCapFloorVols(ore::data::parseBool(XMLUtils::getNodeValue(yoyCapVolSimNode)));

        // All yy cap indices
        setYoYInflationCapFloorVolNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
        set<string> yyIndices = params_.find(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility)->second.second;
        QL_REQUIRE(yyIndices.size() > 0, "YYCapFloorVolatilities needs at least on index");

        // Get configured expiries
        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        QL_REQUIRE(expiryNodes.size() > 0, "YYCapFloorVolatilities needs at least one Expiries node");
        set<string> indicesCheck = yyIndices;
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            string index = XMLUtils::getAttribute(expiryNode, "name");
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(yoyInflationCapFloorVolExpiries_.insert(make_pair(index, expiries)).second,
                       "YYCapFloorVolatlities has duplicate expiries for key '" << index << "'");
            indicesCheck.erase(index);
            defaultProvided = index == "";
        }
        QL_REQUIRE(defaultProvided || indicesCheck.size() == 0, "YYCapFloorVolatilites has no expiries for indices '"
                                                                    << join(indicesCheck, ";")
                                                                    << "' and no default expiry has been given");

        // Get configured strikes
        vector<XMLNode*> strikeNodes = XMLUtils::getChildrenNodes(nodeChild, "Strikes");
        QL_REQUIRE(strikeNodes.size() > 0, "CapFloorVolatilities needs at least one Strikes node");
        indicesCheck = yyIndices;
        defaultProvided = false;
        for (XMLNode* strikeNode : strikeNodes) {
            string index = XMLUtils::getAttribute(strikeNode, "name");
            // For the strike value, we allow ATM or a comma separated list of absolute strike values
            // If ATM, the stored strikes vector is left as an empty vector
            vector<Rate> strikes;
            string strStrike = XMLUtils::getNodeValue(strikeNode);
            strikes = parseListOfValues<Rate>(strStrike, &parseReal);
            QL_REQUIRE(yoyInflationCapFloorVolStrikes_.insert(make_pair(index, strikes)).second,
                       "YYInflationCapFloorVolatilities has duplicate strikes for key '" << index << "'");
            indicesCheck.erase(index);
            defaultProvided = index == "";
        }
        QL_REQUIRE(defaultProvided || indicesCheck.size() == 0, "YYInflationCapFloorVolatilities has no strikes for "
                                                                    << "currencies '" << join(indicesCheck, ",")
                                                                    << "' and no default strike set has been given");

        yoyInflationCapFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            yoyInflationCapFloorVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading CPICapFloorVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "CPICapFloorVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setSimulateZeroInflationCapFloorVols(false);
        XMLNode* ziCapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (ziCapVolSimNode)
            setSimulateZeroInflationCapFloorVols(ore::data::parseBool(XMLUtils::getNodeValue(ziCapVolSimNode)));

        setZeroInflationCapFloorNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));

        set<string> cpiIndices = params_.find(RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility)->second.second;
        QL_REQUIRE(cpiIndices.size() > 0, "CPICapFloorVolatilities needs at least on index");

        // Get configured expiries
        // zeroInflationCapFloorVolExpiries_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        QL_REQUIRE(expiryNodes.size() > 0, "CPICapFloorVolatilities needs at least one Expiries node");
        set<string> indicesCheck = cpiIndices;
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            string index = XMLUtils::getAttribute(expiryNode, "name");
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(zeroInflationCapFloorVolExpiries_.insert(make_pair(index, expiries)).second,
                       "CPICapFloorVolatlities has duplicate expiries for key '" << index << "'");
            indicesCheck.erase(index);
            defaultProvided = index == "";
        }
        QL_REQUIRE(defaultProvided || indicesCheck.size() == 0, "CPICapFloorVolatilites has no expiries for indices '"
                                                                    << join(indicesCheck, ";")
                                                                    << "' and no default expiry has been given");

        // Get configured strikes
        // zeroInflationCapFloorVolStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "Strikes", true);
        vector<XMLNode*> strikeNodes = XMLUtils::getChildrenNodes(nodeChild, "Strikes");
        QL_REQUIRE(strikeNodes.size() > 0, "CPICapFloorVolatilities needs at least one Strikes node");
        indicesCheck = cpiIndices;
        defaultProvided = false;
        for (XMLNode* strikeNode : strikeNodes) {
            string index = XMLUtils::getAttribute(strikeNode, "name");
            // For the strike value, we allow ATM or a comma separated list of absolute strike values
            // If ATM, the stored strikes vector is left as an empty vector
            vector<Rate> strikes;
            string strStrike = XMLUtils::getNodeValue(strikeNode);
            strikes = parseListOfValues<Rate>(strStrike, &parseReal);
            QL_REQUIRE(zeroInflationCapFloorVolStrikes_.insert(make_pair(index, strikes)).second,
                       "CPIInflationCapFloorVolatilities has duplicate strikes for key '" << index << "'");
            indicesCheck.erase(index);
            defaultProvided = index == "";
        }
        QL_REQUIRE(defaultProvided || indicesCheck.size() == 0, "CPIInflationCapFloorVolatilities has no strikes for "
                                                                    << "currencies '" << join(indicesCheck, ",")
                                                                    << "' and no default strike set has been given");

        zeroInflationCapFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            zeroInflationCapFloorVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading DefaultCurves Rates");
    nodeChild = XMLUtils::getChildNode(node, "DefaultCurves");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setDefaultNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
        defaultTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
        // TODO read other keys
        XMLNode* survivalProbabilitySimNode = XMLUtils::getChildNode(nodeChild, "SimulateSurvivalProbabilities");
        if (survivalProbabilitySimNode)
            setSimulateSurvivalProbabilities(ore::data::parseBool(XMLUtils::getNodeValue(survivalProbabilitySimNode)));
        XMLNode* recoveryRateSimNode = XMLUtils::getChildNode(nodeChild, "SimulateRecoveryRates");
        if (recoveryRateSimNode)
            setSimulateRecoveryRates(ore::data::parseBool(XMLUtils::getNodeValue(recoveryRateSimNode)));

        XMLNode* cal = XMLUtils::getChildNode(nodeChild, "Calendars");
        if (cal) {
            for (XMLNode* child = XMLUtils::getChildNode(cal, "Calendar"); child;
                 child = XMLUtils::getNextSibling(child)) {
                string label = XMLUtils::getAttribute(child, "name");
                defaultCurveCalendars_[label] = XMLUtils::getNodeValue(child);
            }
        }
        QL_REQUIRE(defaultCurveCalendars_.find("") != defaultCurveCalendars_.end(),
                   "default calendar is not set for defaultCurves");
        if (auto n = XMLUtils::getChildNode(nodeChild, "Extrapolation")) {
            defaultCurveExtrapolation_ = XMLUtils::getNodeValue(n);
        }
    }

    DLOG("Loading Equities Rates");
    nodeChild = XMLUtils::getChildNode(node, "Equities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* dividendYieldSimNode = XMLUtils::getChildNode(nodeChild, "SimulateDividendYield");
        if (dividendYieldSimNode)
            setSimulateDividendYield(ore::data::parseBool(XMLUtils::getNodeValue(dividendYieldSimNode)));
        else
            setSimulateDividendYield(false);
        vector<string> equityNames = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        setEquityNames(equityNames);
        equityDividendTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "DividendTenors", true);
    }

    DLOG("Loading CDSVolatilities Rates");
    nodeChild = XMLUtils::getChildNode(node, "CDSVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* cdsVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (cdsVolSimNode)
            setSimulateCdsVols(ore::data::parseBool(XMLUtils::getNodeValue(cdsVolSimNode)));
        cdsVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        setCdsVolNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
        cdsVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

        XMLNode* atmOnlyNode = XMLUtils::getChildNode(nodeChild, "SimulateATMOnly");
        if (atmOnlyNode) {
            cdsVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(nodeChild, "SimulateATMOnly", true);
        }

        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            cdsVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading FXVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "FxVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setSimulateFXVols(false);
        XMLNode* fxVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (fxVolSimNode)
            setSimulateFXVols(ore::data::parseBool(XMLUtils::getNodeValue(fxVolSimNode)));
        fxVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        setFxVolCcyPairs(XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true));

        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        set<string> names = params_.find(RiskFactorKey::KeyType::FXVolatility)->second.second;
        QL_REQUIRE(names.size() > 0, "FXVolatility needs at least one name");
        set<string> namesCheck = names;
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            // If there is no "name" attribute, getAttribute returns "" which is what we want in any case
            string name = XMLUtils::getAttribute(expiryNode, "ccyPair");
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(fxVolExpiries_.insert(make_pair(name, expiries)).second,
                       "FXVolatilities has duplicate expiries for key '" << name << "'");
            namesCheck.erase(name);
            defaultProvided = name == "";
        }
        QL_REQUIRE(defaultProvided || namesCheck.size() == 0, "FXVolatilities has no expiries for "
            << "equities '" << join(namesCheck, ",") << "' and no default expiry set has been given");

        XMLNode* fxSurfaceNode = XMLUtils::getChildNode(nodeChild, "Surface");
        setFxVolIsSurface("", false);
        if (fxSurfaceNode) {
            for (XMLNode* child = XMLUtils::getChildNode(fxSurfaceNode, "Moneyness"); child;
                 child = XMLUtils::getNextSibling(child, "Moneyness")) {
                string label = XMLUtils::getAttribute(child, "ccyPair"); // will be "" if no attr
                setFxVolMoneyness(label, XMLUtils::getNodeValueAsDoublesCompact(child));
                if (fxVolMoneyness(label).size() > 1) {
                    setFxVolIsSurface(label, true);
                }
            }
            for (XMLNode* child = XMLUtils::getChildNode(fxSurfaceNode, "StandardDeviations"); child;
                 child = XMLUtils::getNextSibling(child, "StandardDeviations")) {
                string label = XMLUtils::getAttribute(child, "ccyPair"); // will be "" if no attr
                // We cannot have both moneyness and standard deviations for any label (inclding the default of ""
                // Throw error if this occurs
                if (fxMoneyness_.find(label) != fxMoneyness_.end()) {
                    QL_FAIL(
                        "FX Volatility simulation parameters - both moneyness and standard deviations provided for "
                        "label " << label);
                } else {
                    setFxVolStdDevs(label, XMLUtils::getNodeValueAsDoublesCompact(child));
                    if (fxVolStdDevs(label).size() > 1) {
                        setFxVolIsSurface(label, true);
                    }
                }
            }
        } else {
            XMLNode* atmOnlyNode = XMLUtils::getChildNode(nodeChild, "SimulateATMOnly");
            if (atmOnlyNode) {
                fxVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(nodeChild, "SimulateATMOnly", true);
            }
        }
        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            fxVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading EquityVolatilities");
    nodeChild = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setSimulateEquityVols(XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true));
        equityVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        setEquityVolNames(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
       
        vector<XMLNode*> expiryNodes = XMLUtils::getChildrenNodes(nodeChild, "Expiries");
        set<string> names = params_.find(RiskFactorKey::KeyType::EquityVolatility)->second.second;
        QL_REQUIRE(names.size() > 0, "EquityVolatility needs at least one name");
        set<string> namesCheck = names;
        bool defaultProvided = false;
        for (XMLNode* expiryNode : expiryNodes) {
            // If there is no "name" attribute, getAttribute returns "" which is what we want in any case
            string name = XMLUtils::getAttribute(expiryNode, "name");
            vector<Period> expiries = parseListOfValues<Period>(XMLUtils::getNodeValue(expiryNode), &parsePeriod);
            QL_REQUIRE(equityVolExpiries_.insert(make_pair(name, expiries)).second,
                "EquityVolatilities has duplicate expiries for key '" << name << "'");
            namesCheck.erase(name);
            defaultProvided = name == "";
        }
        QL_REQUIRE(defaultProvided || namesCheck.size() == 0, "EquityVolatilities has no expiries for " <<
            "equities '" << join(namesCheck, ",") << "' and no default expiry set has been given");

        XMLNode* eqSurfaceNode = XMLUtils::getChildNode(nodeChild, "Surface");
        setEquityVolIsSurface("", false);
        if (eqSurfaceNode) {
            for (XMLNode* child = XMLUtils::getChildNode(eqSurfaceNode, "Moneyness"); child;
                child = XMLUtils::getNextSibling(child, "Moneyness")) {
                string label = XMLUtils::getAttribute(child, "name"); // will be "" if no attr
                setEquityVolMoneyness(label, XMLUtils::getNodeValueAsDoublesCompact(child));
                if (equityVolMoneyness(label).size() > 1) {
                    setEquityVolIsSurface(label, true);
                }
            }
            for (XMLNode* child = XMLUtils::getChildNode(eqSurfaceNode, "StandardDeviations"); child;
                child = XMLUtils::getNextSibling(child, "StandardDeviations")) {
                string label = XMLUtils::getAttribute(child, "name"); // will be "" if no attr
                // We cannot have both moneyness and standard deviations for any label (including the default of ""
                // Throw error if this occurs
                if (equityMoneyness_.find(label) != equityMoneyness_.end()){
                    QL_FAIL("Equity Volatility simulation parameters - both moneyness and standard deviations provided for label " << label);
                } else {
                    setEquityVolStandardDevs(label, XMLUtils::getNodeValueAsDoublesCompact(child));
                    if (equityVolStandardDevs(label).size() > 1) {
                        setEquityVolIsSurface(label, true);
                    }
                }
            }
        } else {
            XMLNode* atmOnlyNode = XMLUtils::getChildNode(nodeChild, "SimulateATMOnly");
            if (atmOnlyNode) {
                equityVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(nodeChild, "SimulateATMOnly", true);
            }
        }
        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            equityVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading CpiInflationIndexCurves");
    setCpiIndices(XMLUtils::getChildrenValues(node, "CpiIndices", "Index", false));

    DLOG("Loading ZeroInflationIndexCurves");
    nodeChild = XMLUtils::getChildNode(node, "ZeroInflationIndexCurves");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setZeroInflationIndices(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
        zeroInflationTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    }

    DLOG("Loading YYInflationIndexCurves");

    nodeChild = XMLUtils::getChildNode(node, "YYInflationIndexCurves");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setYoyInflationIndices(XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true));
        yoyInflationTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    }

    DLOG("Loading AggregationScenarioDataIndices");
    if (XMLUtils::getChildNode(node, "AggregationScenarioDataIndices")) {
        additionalScenarioDataIndices_ = XMLUtils::getChildrenValues(node, "AggregationScenarioDataIndices", "Index");
    }

    DLOG("Loading AggregationScenarioDataCurrencies");
    if (XMLUtils::getChildNode(node, "AggregationScenarioDataCurrencies")) {
        additionalScenarioDataCcys_ =
            XMLUtils::getChildrenValues(node, "AggregationScenarioDataCurrencies", "Currency", true);
    }

    DLOG("Loading Securities");
    nodeChild = XMLUtils::getChildNode(node, "Securities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        // TODO 1) this should be renamed to SimulateSpread?
        //      2) add security recovery rates here separate from default curves?
        setSecuritySpreadsSimulate(XMLUtils::getChildValueAsBool(nodeChild, "Simulate", false));
        vector<string> securities = XMLUtils::getChildrenValues(nodeChild, "Names", "Name");
        setSecurities(securities);
    }

    DLOG("Loading CPRs");
    nodeChild = XMLUtils::getChildNode(node, "CPRs");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setSimulateCprs(XMLUtils::getChildValueAsBool(nodeChild, "Simulate", false));
        setCprs(XMLUtils::getChildrenValues(nodeChild, "Names", "Name"));
    }

    DLOG("Loading BaseCorrelations");
    nodeChild = XMLUtils::getChildNode(node, "BaseCorrelations");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setSimulateBaseCorrelations(XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true));
        setBaseCorrelationNames(XMLUtils::getChildrenValues(nodeChild, "IndexNames", "IndexName", true));
        baseCorrelationTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
        baseCorrelationDetachmentPoints_ =
            XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "DetachmentPoints", true);
    }

    DLOG("Loading commodities data");
    nodeChild = XMLUtils::getChildNode(node, "Commodities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        XMLNode* commoditySimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        setCommodityCurveSimulate(commoditySimNode ? parseBool(XMLUtils::getNodeValue(commoditySimNode)) : false);

        vector<string> commodityNames = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        setCommodityNames(commodityNames);

        set<string> names = params_.find(RiskFactorKey::KeyType::CommodityCurve)->second.second;
        QL_REQUIRE(names.size() > 0, "Commodities needs at least one name");

        // Get the configured tenors. They are of the form:
        // - <Tenors name="NAME">t_1,...,t_n</Tenors> for commodity name specific tenors
        // - <Tenors>t_1,...,t_n</Tenors> or <Tenors name="">t_1,...,t_n</Tenors> for a default set of tenors
        // Only need a default tenor set if every commodity name has not been given a tenor set explicitly
        vector<XMLNode*> tenorNodes = XMLUtils::getChildrenNodes(nodeChild, "Tenors");
        QL_REQUIRE(tenorNodes.size() > 0, "Commodities needs at least one Tenors node");
        set<string> namesCheck = names;
        bool defaultProvided = false;
        for (XMLNode* tenorNode : tenorNodes) {
            // If there is no "name" attribute, getAttribute returns "" which is what we want in any case
            string name = XMLUtils::getAttribute(tenorNode, "name");

            // An empty tenor list here means that the scenario simulation market should be set up on the
            // same pillars as the initial t_0 market from which it is sampling its values
            vector<Period> tenors;
            string strTenorList = XMLUtils::getNodeValue(tenorNode);
            if (!strTenorList.empty()) {
                tenors = parseListOfValues<Period>(XMLUtils::getNodeValue(tenorNode), &parsePeriod);
            }

            QL_REQUIRE(commodityCurveTenors_.insert(make_pair(name, tenors)).second,
                       "Commodities has duplicate expiries for key '" << name << "'");
            namesCheck.erase(name);
            defaultProvided = name == "";
        }
        QL_REQUIRE(defaultProvided || namesCheck.size() == 0, "Commodities has no tenors for "
                                                                  << "names '" << join(namesCheck, ",")
                                                                  << "' and no default tenor set has been given");
    }

    DLOG("Loading commodity volatility data");
    nodeChild = XMLUtils::getChildNode(node, "CommodityVolatilities");
    if (nodeChild && XMLUtils::getChildNode(nodeChild)) {
        setCommodityVolSimulate(XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true));
        commodityVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");

        vector<string> names;
        XMLNode* namesNode = XMLUtils::getChildNode(nodeChild, "Names");
        if (namesNode) {
            for (XMLNode* child = XMLUtils::getChildNode(namesNode, "Name"); child;
                 child = XMLUtils::getNextSibling(child)) {
                // Get the vol configuration for each commodity name
                string name = XMLUtils::getAttribute(child, "id");
                names.push_back(name);
                commodityVolExpiries_[name] = XMLUtils::getChildrenValuesAsPeriods(child, "Expiries", true);
                vector<Real> moneyness = XMLUtils::getChildrenValuesAsDoublesCompact(child, "Moneyness", false);
                if (moneyness.empty())
                    moneyness = {1.0};
                commodityVolMoneyness_[name] = moneyness;
            }
        }
        setCommodityVolNames(names);
        // Get smile dynamics
        vector<XMLNode*> smileDynamicsNodes = XMLUtils::getChildrenNodes(nodeChild, "SmileDynamics");
        for (XMLNode* smileDynamicsNode : smileDynamicsNodes) {
            string key = XMLUtils::getAttribute(smileDynamicsNode, "key");
            commodityVolSmileDynamics_.insert(make_pair(key, XMLUtils::getNodeValue(smileDynamicsNode)));
        }
    }

    DLOG("Loading credit states data");
    nodeChild = XMLUtils::getChildNode(node, "CreditStates");
    numberOfCreditStates_ = 0;
    if (nodeChild) {
        numberOfCreditStates_ = XMLUtils::getChildValueAsInt(nodeChild, "NumberOfFactors", true);
    }

    DLOG("Loading AggregationScenarioDataCreditStates");
    nodeChild = XMLUtils::getChildNode(node, "AggregationScenarioDataCreditStates");
    additionalScenarioDataNumberOfCreditStates_ = 0;
    if (nodeChild) {
        additionalScenarioDataNumberOfCreditStates_ = XMLUtils::getChildValueAsInt(nodeChild, "NumberOfFactors", true);
    }

    DLOG("Loading AggregationScenarioDataSurvivalWeights");
    additionalScenarioDataSurvivalWeights_ = XMLUtils::getChildrenValues(node, "AggregationScenarioDataSurvivalWeights", "Name");


    DLOG("Loaded ScenarioSimMarketParameters");
}

XMLNode* ScenarioSimMarketParameters::toXML(XMLDocument& doc) const {

    XMLNode* simulationNode = doc.allocNode("Simulation");
    XMLNode* marketNode = doc.allocNode("Market");

    // currencies
    XMLUtils::addChild(doc, marketNode, "BaseCurrency", baseCcy_);
    XMLUtils::addChildren(doc, marketNode, "Currencies", "Currency", ccys_);

    // yield curves
    DLOG("Writing yield curves data");
    XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, marketNode, "YieldCurves");

    // Take the keys from the yieldCurveDayCounters_ and yieldCurveTenors_ maps
    set<string> keys;
    for (const auto& kv : yieldCurveTenors_) {
        keys.insert(kv.first);
    }
    QL_REQUIRE(keys.count("") > 0, "There is no default yield curve configuration in simulation parameters");

    // Add the yield curve configuration nodes
    for (const auto& key : keys) {
        XMLNode* configNode = doc.allocNode("Configuration");
        XMLUtils::addAttribute(doc, configNode, "curve", key);
        if (yieldCurveTenors_.count(key) > 0) {
            XMLUtils::addGenericChildAsList(doc, configNode, "Tenors", yieldCurveTenors_.at(key));
        }
        if (key == "") {
            XMLUtils::addChild(doc, configNode, "Interpolation", interpolation_);
            XMLUtils::addChild(doc, configNode, "Extrapolation", extrapolation_);
        }
        XMLUtils::appendNode(yieldCurvesNode, configNode);
    }

    // fx rates
    if (fxCcyPairs().size() > 0) {
        DLOG("Writing FX rates");
        XMLNode* fxRatesNode = XMLUtils::addChild(doc, marketNode, "FxRates");
        XMLUtils::addChildren(doc, fxRatesNode, "CurrencyPairs", "CurrencyPair", fxCcyPairs());
    }

    // indices
    if (indices().size() > 0) {
        DLOG("Writing libor indices");
        XMLUtils::addChildren(doc, marketNode, "Indices", "Index", indices());
    }

    // swap indices
    if (swapIndices_.size() > 0) {
        DLOG("Writing swap indices");
        XMLNode* swapIndicesNode = XMLUtils::addChild(doc, marketNode, "SwapIndices");
        for (auto kv : swapIndices_) {
            XMLNode* swapIndexNode = XMLUtils::addChild(doc, swapIndicesNode, "SwapIndex");
            XMLUtils::addChild(doc, swapIndexNode, "Name", kv.first);
            XMLUtils::addChild(doc, swapIndexNode, "DiscountingIndex", kv.second);
        }
    }

    // default curves
    if (!defaultNames().empty()) {
        DLOG("Writing default curves");
        XMLNode* defaultCurvesNode = XMLUtils::addChild(doc, marketNode, "DefaultCurves");
        XMLUtils::addChildren(doc, defaultCurvesNode, "Names", "Name", defaultNames());
        XMLUtils::addGenericChildAsList(doc, defaultCurvesNode, "Tenors", lookup(defaultTenors_, ""));
        XMLUtils::addChild(doc, defaultCurvesNode, "SimulateSurvivalProbabilities", simulateSurvivalProbabilities());
        XMLUtils::addChild(doc, defaultCurvesNode, "SimulateRecoveryRates", simulateRecoveryRates());

        if (defaultCurveCalendars_.size() > 0) {
            XMLNode* node = XMLUtils::addChild(doc, defaultCurvesNode, "Calendars");
            for (auto dc : defaultCurveCalendars_) {
                XMLNode* c = doc.allocNode("Calendar", dc.second);
                XMLUtils::addAttribute(doc, c, "name", dc.first);
                XMLUtils::appendNode(node, c);
            }
        }

        if (!defaultCurveExtrapolation_.empty()) {
            XMLUtils::addChild(doc, defaultCurvesNode, "Extrapolation", defaultCurveExtrapolation_);
        }
    }

    // equities
    if (!equityNames().empty()) {
        DLOG("Writing equities");
        XMLNode* equitiesNode = XMLUtils::addChild(doc, marketNode, "Equities");
        XMLUtils::addChild(doc, equitiesNode, "SimulateDividendYield", simulateDividendYield());
        XMLUtils::addChildren(doc, equitiesNode, "Names", "Name", equityNames());
        XMLUtils::addGenericChildAsList(doc, equitiesNode, "DividendTenors", lookup(equityDividendTenors_, ""));
    }

    // swaption volatilities
    if (!swapVolKeys().empty()) {
        DLOG("Writing swaption volatilities");
        XMLNode* swaptionVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "SwaptionVolatilities");
        XMLUtils::addChild(doc, swaptionVolatilitiesNode, "Simulate", simulateSwapVols());
        XMLUtils::addChild(doc, swaptionVolatilitiesNode, "ReactionToTimeDecay", swapVolDecayMode_);
        XMLUtils::addChildren(doc, swaptionVolatilitiesNode, "Keys", "Key", swapVolKeys());
        for (auto it = swapVolExpiries_.begin(); it != swapVolExpiries_.end(); it++) {
            XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Expiries", swapVolExpiries_.find(it->first)->second,
                                            "key", it->first);
        }
        for (auto it = swapVolTerms_.begin(); it != swapVolTerms_.end(); it++) {
            XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Terms", swapVolTerms_.find(it->first)->second, "key",
                                            it->first);
        }

        if (swapVolSimulateATMOnly_) {
            XMLUtils::addChild(doc, swaptionVolatilitiesNode, "SimulateATMOnly", swapVolSimulateATMOnly_);
        } else {
            for (auto it = swapVolStrikeSpreads_.begin(); it != swapVolStrikeSpreads_.end(); it++) {
                XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "StrikeSpreads",
                                                swapVolStrikeSpreads_.find(it->first)->second, "key", it->first);
            }
        }
        for (auto it = swapVolSmileDynamics_.begin(); it != swapVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, swaptionVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // yield volatilities
    if (!yieldVolNames().empty()) {
        DLOG("Writing yield volatilities");
        XMLNode* yieldVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "YieldVolatilities");
        XMLUtils::addChild(doc, yieldVolatilitiesNode, "Simulate", simulateYieldVols());
        XMLUtils::addChild(doc, yieldVolatilitiesNode, "ReactionToTimeDecay", yieldVolDecayMode_);
        XMLUtils::addChildren(doc, yieldVolatilitiesNode, "Names", "Name", yieldVolNames());
        XMLUtils::addGenericChildAsList(doc, yieldVolatilitiesNode, "Expiries", yieldVolExpiries_);
        XMLUtils::addGenericChildAsList(doc, yieldVolatilitiesNode, "Terms", yieldVolTerms_);
        for (auto it = yieldVolSmileDynamics_.begin(); it != yieldVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, yieldVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // cap/floor volatilities
    if (!capFloorVolKeys().empty()) {
        DLOG("Writing cap/floor volatilities");
        XMLNode* capFloorVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CapFloorVolatilities");
        XMLUtils::addChild(doc, capFloorVolatilitiesNode, "Simulate", simulateCapFloorVols());
        XMLUtils::addChild(doc, capFloorVolatilitiesNode, "ReactionToTimeDecay", capFloorVolDecayMode_);
        XMLUtils::addChildren(doc, capFloorVolatilitiesNode, "Keys", "Key", capFloorVolKeys());

        // Write out cap floor expiries node for each key
        for (auto kv : capFloorVolExpiries_) {
            // If strikes vector is empty, the node value is ATM else it is the comma separated list of strikes
            // No checks here on the string repr of each strike value - dangerous but in lots of places.
            string nodeValue = join(kv.second | transformed([](Period p) { return ore::data::to_string(p); }), ",");
            XMLNode* expiriesNode = doc.allocNode("Expiries", nodeValue);
            XMLUtils::addAttribute(doc, expiriesNode, "key", kv.first);
            XMLUtils::appendNode(capFloorVolatilitiesNode, expiriesNode);
        }

        // Write out cap floor strikes for each currency
        for (auto kv : capFloorVolStrikes_) {
            // If strikes vector is empty, the node value is ATM else it is the comma separated list of strikes
            // No checks here on the string repr of each strike value - dangerous but in lots of places.
            string nodeValue = kv.second.empty()
                                   ? "ATM"
                                   : join(kv.second | transformed([](Rate s) { return ore::data::to_string(s); }), ",");
            XMLNode* strikesNode = doc.allocNode("Strikes", nodeValue);
            XMLUtils::addAttribute(doc, strikesNode, "key", kv.first);
            XMLUtils::appendNode(capFloorVolatilitiesNode, strikesNode);
        }

        XMLUtils::addChild(doc, capFloorVolatilitiesNode, "AdjustOptionletPillars",
            capFloorVolAdjustOptionletPillars_);
        XMLUtils::addChild(doc, capFloorVolatilitiesNode, "UseCapAtm", capFloorVolUseCapAtm_);
        for (auto it = capFloorVolSmileDynamics_.begin(); it != capFloorVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, capFloorVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // zero inflation cap/floor volatilities
    if (!zeroInflationCapFloorVolNames().empty()) {
        DLOG("Writing zero inflation cap/floor volatilities");
        XMLNode* n = XMLUtils::addChild(doc, marketNode, "CPICapFloorVolatilities");
        XMLUtils::addChild(doc, n, "Simulate", simulateZeroInflationCapFloorVols());
        XMLUtils::addChild(doc, n, "ReactionToTimeDecay", zeroInflationCapFloorVolDecayMode());
        XMLUtils::addChildren(doc, n, "Names", "Name", zeroInflationCapFloorVolNames());

        // Write out cap floor expiries node for each currency
        for (auto kv : zeroInflationCapFloorVolExpiries_) {
            string nodeValue = join(kv.second | transformed([](Period p) { return ore::data::to_string(p); }), ",");
            XMLNode* expiriesNode = doc.allocNode("Expiries", nodeValue);
            XMLUtils::addAttribute(doc, expiriesNode, "name", kv.first);
            XMLUtils::appendNode(n, expiriesNode);
        }

        // Write out cap floor strikes for each currency
        for (auto kv : zeroInflationCapFloorVolStrikes_) {
            string nodeValue = kv.second.empty()
                                   ? "ATM"
                                   : join(kv.second | transformed([](Rate s) { return ore::data::to_string(s); }), ",");
            XMLNode* strikesNode = doc.allocNode("Strikes", nodeValue);
            XMLUtils::addAttribute(doc, strikesNode, "name", kv.first);
            XMLUtils::appendNode(n, strikesNode);
        }
        for (auto it = zeroInflationCapFloorVolSmileDynamics_.begin();
             it != zeroInflationCapFloorVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, n, "SmileDynamics", it->second, "key", it->first);
        }
    }

    if (!cdsVolNames().empty()) {
        DLOG("Writing CDS volatilities");
        XMLNode* cdsVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CDSVolatilities");
        XMLUtils::addChild(doc, cdsVolatilitiesNode, "Simulate", simulateCdsVols());
        XMLUtils::addChild(doc, cdsVolatilitiesNode, "ReactionToTimeDecay", cdsVolDecayMode_);
        XMLUtils::addChildren(doc, cdsVolatilitiesNode, "Names", "Name", cdsVolNames());
        XMLUtils::addGenericChildAsList(doc, cdsVolatilitiesNode, "Expiries", cdsVolExpiries_);
        if (cdsVolSimulateATMOnly_) {
            XMLUtils::addChild(doc, cdsVolatilitiesNode, "SimulateATMOnly", cdsVolSimulateATMOnly_);
        }
        for (auto it = cdsVolSmileDynamics_.begin(); it != cdsVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, cdsVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // fx volatilities
    if (!fxVolCcyPairs().empty()) {
        DLOG("Writing FX volatilities");
        XMLNode* fxVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "FxVolatilities");
        XMLUtils::addChild(doc, fxVolatilitiesNode, "Simulate", simulateFXVols());
        XMLUtils::addChild(doc, fxVolatilitiesNode, "ReactionToTimeDecay", fxVolDecayMode_);
        XMLUtils::addChildren(doc, fxVolatilitiesNode, "CurrencyPairs", "CurrencyPair", fxVolCcyPairs());
        for (auto it = fxVolExpiries_.begin(); it != fxVolExpiries_.end(); it++) {
            XMLUtils::addGenericChildAsList(doc, fxVolatilitiesNode, "Expiries", fxVolExpiries_.find(it->first)->second, "ccyPair",
                                            it->first);
        }
        if (fxVolSimulateATMOnly_) {
            XMLUtils::addChild(doc, fxVolatilitiesNode, "SimulateATMOnly", fxVolSimulateATMOnly_);
        }
        if (fxVolSimulateATMOnly_ || fxMoneyness_.size() > 0 || fxStandardDevs_.size() > 0) {
            XMLNode* fxSurfaceNode = XMLUtils::addChild(doc, fxVolatilitiesNode, "Surface");
            for (auto it = fxMoneyness_.begin(); it != fxMoneyness_.end(); it++) {
                XMLUtils::addGenericChildAsList(doc, fxSurfaceNode, "Moneyness", equityMoneyness_.find(it->first)->second, "ccyPair",
                                                it->first);
            }
            for (auto it = fxStandardDevs_.begin(); it != fxStandardDevs_.end(); it++) {
                XMLUtils::addGenericChildAsList(doc, fxSurfaceNode, "StandardDeviations",
                                                fxStandardDevs_.find(it->first)->second, "ccyPair", it->first);
            }
        }
        for (auto it = fxVolSmileDynamics_.begin(); it != fxVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, fxVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // eq volatilities
    if (!equityVolNames().empty()) {
        DLOG("Writing equity volatilities");
        XMLNode* eqVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "EquityVolatilities");
        XMLUtils::addChild(doc, eqVolatilitiesNode, "Simulate", simulateEquityVols());
        XMLUtils::addChild(doc, eqVolatilitiesNode, "ReactionToTimeDecay", equityVolDecayMode_);
        XMLUtils::addChildren(doc, eqVolatilitiesNode, "Names", "Name", equityVolNames());
        for (auto it = equityVolExpiries_.begin(); it != equityVolExpiries_.end(); it++) {
            XMLUtils::addGenericChildAsList(doc, eqVolatilitiesNode, "Expiries", equityVolExpiries_.find(it->first)->second, "name",
                it->first);
        }
        if (equityVolSimulateATMOnly_) {
            XMLUtils::addChild(doc, eqVolatilitiesNode, "SimulateATMOnly", equityVolSimulateATMOnly_);
        }
        if (equityVolSimulateATMOnly_ || equityMoneyness_.size() > 0 || equityStandardDevs_.size() > 0) {
            XMLNode* eqSurfaceNode = XMLUtils::addChild(doc, eqVolatilitiesNode, "Surface");
            for (auto it = equityMoneyness_.begin(); it != equityMoneyness_.end(); it++) {
                XMLUtils::addGenericChildAsList(doc, eqSurfaceNode, "Moneyness", equityMoneyness_.find(it->first)->second, "name",
                    it->first);
            }
            for (auto it = equityStandardDevs_.begin(); it != equityStandardDevs_.end(); it++) {
                XMLUtils::addGenericChildAsList(doc, eqSurfaceNode, "StandardDeviations", equityStandardDevs_.find(it->first)->second, "name",
                    it->first);
            }            
        }
        for (auto it = equityVolSmileDynamics_.begin(); it != equityVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, eqVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // benchmark yield curves
    XMLNode* benchmarkCurvesNode = XMLUtils::addChild(doc, marketNode, "BenchmarkCurves");
    for (Size i = 0; i < yieldCurveNames().size(); ++i) {
        DLOG("Writing benchmark yield curves data");
        XMLNode* benchmarkCurveNode = XMLUtils::addChild(doc, benchmarkCurvesNode, "BenchmarkCurve");
        XMLUtils::addChild(doc, benchmarkCurveNode, "Currency", yieldCurveCurrencies_.find(yieldCurveNames()[i])->second);
        XMLUtils::addChild(doc, benchmarkCurveNode, "Name", yieldCurveNames()[i]);
    }

    // securities
    if (!securities().empty()) {
        DLOG("Writing securities");
        XMLNode* secNode = XMLUtils::addChild(doc, marketNode, "Securities");
        XMLUtils::addChild(doc, secNode, "Simulate", securitySpreadsSimulate());
        XMLUtils::addChildren(doc, secNode, "Names", "Name", securities());
    }

    // cprs
    if (!cprs().empty()) {
        DLOG("Writing cprs");
        XMLNode* cprNode = XMLUtils::addChild(doc, marketNode, "CPRs");
        XMLUtils::addChild(doc, cprNode, "Simulate", simulateCprs());
        XMLUtils::addChildren(doc, cprNode, "Names", "Name", cprs());
    }

    // inflation indices
    if (!cpiIndices().empty()) {
        DLOG("Writing inflation indices");
        XMLUtils::addChildren(doc, marketNode, "CpiIndices", "Index", cpiIndices());
    }

    // zero inflation
    if (!zeroInflationIndices().empty()) {
        DLOG("Writing zero inflation");
        XMLNode* zeroNode = XMLUtils::addChild(doc, marketNode, "ZeroInflationIndexCurves");
        XMLUtils::addChildren(doc, zeroNode, "Names", "Name", zeroInflationIndices());
        XMLUtils::addGenericChildAsList(doc, zeroNode, "Tenors", lookup(zeroInflationTenors_, ""));
    }

    // yoy inflation
    if (!yoyInflationIndices().empty()) {
        DLOG("Writing year-on-year inflation");
        XMLNode* yoyNode = XMLUtils::addChild(doc, marketNode, "YYInflationIndexCurves");
        XMLUtils::addChildren(doc, yoyNode, "Names", "Name", yoyInflationIndices());
        XMLUtils::addGenericChildAsList(doc, yoyNode, "Tenors", lookup(yoyInflationTenors_, ""));
    }

    // yoy cap/floor volatilities
    if (!yoyInflationCapFloorVolNames().empty()) {
        DLOG("Writing inflation cap/floor volatilities");
        XMLNode* n = XMLUtils::addChild(doc, marketNode, "YYCapFloorVolatilities");
        XMLUtils::addChild(doc, n, "Simulate", simulateYoYInflationCapFloorVols());
        XMLUtils::addChild(doc, n, "ReactionToTimeDecay", yoyInflationCapFloorVolDecayMode());
        XMLUtils::addChildren(doc, n, "Names", "Name", yoyInflationCapFloorVolNames());

        // Write out cap floor expiries node for each currency
        for (auto kv : yoyInflationCapFloorVolExpiries_) {
            string nodeValue = join(kv.second | transformed([](Period p) { return ore::data::to_string(p); }), ",");
            XMLNode* expiriesNode = doc.allocNode("Expiries", nodeValue);
            XMLUtils::addAttribute(doc, expiriesNode, "name", kv.first);
            XMLUtils::appendNode(n, expiriesNode);
        }

        // Write out cap floor strikes for each currency
        for (auto kv : yoyInflationCapFloorVolStrikes_) {
            string nodeValue = kv.second.empty()
                                   ? "ATM"
                                   : join(kv.second | transformed([](Rate s) { return ore::data::to_string(s); }), ",");
            XMLNode* strikesNode = doc.allocNode("Strikes", nodeValue);
            XMLUtils::addAttribute(doc, strikesNode, "name", kv.first);
            XMLUtils::appendNode(n, strikesNode);
        }
        for (auto it = yoyInflationCapFloorVolSmileDynamics_.begin(); it != yoyInflationCapFloorVolSmileDynamics_.end();
             it++) {
            XMLUtils::addChild(doc, n, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // Commodity price curves
    if (!commodityNames().empty()) {
        DLOG("Writing commodity price curves");
        XMLNode* commodityPriceNode = XMLUtils::addChild(doc, marketNode, "Commodities");
        XMLUtils::addChild(doc, commodityPriceNode, "Simulate", commodityCurveSimulate());
        XMLUtils::addChildren(doc, commodityPriceNode, "Names", "Name", commodityNames());

        // Write out tenors node for each commodity name
        for (auto kv : commodityCurveTenors_) {
            // Single bar here is a boost range adaptor. Documented here:
            // https://www.boost.org/doc/libs/1_71_0/libs/range/doc/html/range/reference/adaptors/introduction.html
            string nodeValue = join(kv.second | transformed([](Period p) { return ore::data::to_string(p); }), ",");
            XMLNode* tenorsNode = doc.allocNode("Tenors", nodeValue);
            XMLUtils::addAttribute(doc, tenorsNode, "name", kv.first);
            XMLUtils::appendNode(commodityPriceNode, tenorsNode);
        }
    }

    // Commodity volatilities
    if (!commodityVolNames().empty()) {
        DLOG("Writing commodity volatilities");
        XMLNode* commodityVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CommodityVolatilities");
        XMLUtils::addChild(doc, commodityVolatilitiesNode, "Simulate", commodityVolSimulate());
        XMLUtils::addChild(doc, commodityVolatilitiesNode, "ReactionToTimeDecay", commodityVolDecayMode_);
        XMLNode* namesNode = XMLUtils::addChild(doc, commodityVolatilitiesNode, "Names");
        for (const auto& name : commodityVolNames()) {
            XMLNode* nameNode = doc.allocNode("Name");
            XMLUtils::addAttribute(doc, nameNode, "id", name);
            XMLUtils::addGenericChildAsList(doc, nameNode, "Expiries", commodityVolExpiries_.find(name)->second);
            XMLUtils::addGenericChildAsList(doc, nameNode, "Moneyness", commodityVolMoneyness_.find(name)->second);
            XMLUtils::appendNode(namesNode, nameNode);
        }
        for (auto it = commodityVolSmileDynamics_.begin(); it != commodityVolSmileDynamics_.end(); it++) {
            XMLUtils::addChild(doc, commodityVolatilitiesNode, "SmileDynamics", it->second, "key", it->first);
        }
    }

    // additional scenario data currencies
    if (!additionalScenarioDataCcys_.empty()) {
        DLOG("Writing aggregation scenario data currencies");
        XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataCurrencies", "Currency",
                              additionalScenarioDataCcys_);
    }

    // additional scenario data indices
    if (!additionalScenarioDataIndices_.empty()) {
        DLOG("Writing aggregation scenario data indices");
        XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataIndices", "Index",
                              additionalScenarioDataIndices_);
    }

    // Credit States
    DLOG("Writing number of credit states");
    XMLNode* creditStatesNode = XMLUtils::addChild(doc, marketNode, "CreditStates");
    XMLUtils::addChild(doc, creditStatesNode, "NumberOfFactors", int(numberOfCreditStates_));

    DLOG("Writing number of credit states, AggregationScenarioDataCreditStates");
    XMLNode* aggScenDataCreditStatesNode = XMLUtils::addChild(doc, marketNode, "AggregationScenarioDataCreditStates");
    XMLUtils::addChild(doc, aggScenDataCreditStatesNode, "NumberOfFactors",
                       int(additionalScenarioDataNumberOfCreditStates_));

    // Survival Weights
    DLOG("Writing names that need tracking of survival weights");
    if (!additionalScenarioDataSurvivalWeights_.empty()) {
        XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataSurvivalWeights", "Name",
                              additionalScenarioDataSurvivalWeights_);
    }

    // base correlations
    if (!baseCorrelationNames().empty()) {
        DLOG("Writing base correlations");
        XMLNode* bcNode = XMLUtils::addChild(doc, marketNode, "BaseCorrelations");
        XMLUtils::addChild(doc, bcNode, "Simulate", simulateBaseCorrelations());
        XMLUtils::addChildren(doc, bcNode, "IndexNames", "IndexName", baseCorrelationNames());
        XMLUtils::addGenericChildAsList(doc, bcNode, "Terms", baseCorrelationTerms_);
        XMLUtils::addGenericChildAsList(doc, bcNode, "DetachmentPoints", baseCorrelationDetachmentPoints_);
    }

    // correlations
    if (!correlationPairs().empty()) {
        DLOG("Writing correlation");
        XMLNode* correlationsNode = XMLUtils::addChild(doc, marketNode, "Correlations");
        XMLUtils::addChild(doc, correlationsNode, "Simulate", simulateCorrelations());
        XMLUtils::addChildren(doc, correlationsNode, "Pairs", "Pair", correlationPairs());

        XMLUtils::addGenericChildAsList(doc, correlationsNode, "Expiries", correlationExpiries_);
    }

    XMLUtils::appendNode(simulationNode, marketNode);

    return simulationNode;
}
} // namespace analytics
} // namespace ore
