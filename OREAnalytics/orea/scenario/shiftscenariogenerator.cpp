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

#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

string ShiftScenarioGenerator::ScenarioDescription::typeString()  const {
    if (type_ == ScenarioDescription::Type::Base)
        return "Base";
    else if (type_ == ScenarioDescription::Type::Up)
        return "Up";
    else if (type_ == ScenarioDescription::Type::Down)
        return "Down";
    else if (type_ == ScenarioDescription::Type::Cross)
        return "Cross";
    else
        QL_FAIL("ScenarioDescription::Type not covered");
}
string ShiftScenarioGenerator::ScenarioDescription::factor1() const {
    ostringstream o;
    if (key1_ != RiskFactorKey()) {
        o << key1_;
        // if (indexDesc1_ != "")
	o << "/" << indexDesc1_;
        return o.str();
    }
    return "";
}
string ShiftScenarioGenerator::ScenarioDescription::factor2() const {
    ostringstream o;
    if (key2_ != RiskFactorKey()) {
        o << key2_;
        //if (indexDesc2_ != "")
	o << "/" << indexDesc2_;
        return o.str();
    }
    return "";
}
string ShiftScenarioGenerator::ScenarioDescription::text() const {
  string t = typeString();
  string f1 = factor1();
  string f2 = factor2();
  string ret = t;
  if (f1 != "")
    ret += ":" + f1;
  if (f2 != "")
    ret += ":" + f2;
  return ret;
}
  
ShiftScenarioGenerator::ShiftScenarioGenerator(
    const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData, 
    const Date& today,
    const boost::shared_ptr<ore::data::Market>& initMarket,
    const std::string& configuration,
    boost::shared_ptr<ScenarioFactory> baseScenarioFactory)
    : baseScenarioFactory_(baseScenarioFactory), simMarketData_(simMarketData), today_(today), initMarket_(initMarket),
      configuration_(configuration), counter_(0) {
    QL_REQUIRE(initMarket_ != NULL, "ShiftScenarioGenerator: initMarket is null");
    if (baseScenarioFactory_ == NULL)
        baseScenarioFactory_ = boost::make_shared<SimpleScenarioFactory>();
    init(initMarket);
}

void ShiftScenarioGenerator::clear() {
    discountCurveKeys_.clear();
    discountCurveCache_.clear();
    indexCurveKeys_.clear();
    indexCurveCache_.clear();
    yieldCurveKeys_.clear();
    yieldCurveCache_.clear();
    fxKeys_.clear();
    fxCache_.clear();
    swaptionVolKeys_.clear();
    swaptionVolCache_.clear();
    fxVolKeys_.clear();
    fxVolCache_.clear();
    optionletVolKeys_.clear();
    optionletVolCache_.clear();
}

void ShiftScenarioGenerator::init(boost::shared_ptr<Market> market) {
    clear();

    numeraireCache_ = 1.0; // assumption that the init numeraire is 1.0
    // Cache discount curve keys and discount factors
    Size n_ccy = simMarketData_->ccys().size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();
    discountCurveKeys_.reserve(n_ccy * n_ten);
    for (Size j = 0; j < n_ccy; j++) {
        std::string ccy = simMarketData_->ccys()[j];
        Handle<YieldTermStructure> ts = market->discountCurve(ccy, configuration_);
        for (Size k = 0; k < n_ten; k++) {
            discountCurveKeys_.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k); // j * n_ten + k
            Period tenor = simMarketData_->yieldCurveTenors()[k];
            Real disc = ts->discount(today_ + tenor);
            discountCurveCache_[discountCurveKeys_[j * n_ten + k]] = disc;
            LOG("cache discount " << disc << " for key " << discountCurveKeys_[j * n_ten + k]);
        }
    }

    // Cache index curve keys
    Size n_indices = simMarketData_->indices().size();
    indexCurveKeys_.reserve(n_indices * n_ten);
    for (Size j = 0; j < n_indices; ++j) {
        Handle<IborIndex> index = market->iborIndex(simMarketData_->indices()[j], configuration_);
        Handle<YieldTermStructure> ts = index->forwardingTermStructure();
        for (Size k = 0; k < n_ten; ++k) {
            indexCurveKeys_.emplace_back(RiskFactorKey::KeyType::IndexCurve, simMarketData_->indices()[j], k);
            Period tenor = simMarketData_->yieldCurveTenors()[k];
            Real disc = ts->discount(today_ + tenor);
            indexCurveCache_[indexCurveKeys_[j * n_ten + k]] = ts->discount(today_ + tenor);
            LOG("cache discount " << disc << " for key " << indexCurveKeys_[j * n_ten + k]);
        }
    }

    // Cache yield curve keys
    Size n_ycnames = simMarketData_->yieldCurveNames().size();
    yieldCurveKeys_.reserve(n_ycnames * n_ten);
    for (Size j = 0; j < n_ycnames; ++j) {
        std::string ycname = simMarketData_->yieldCurveNames()[j];
        Handle<YieldTermStructure> ts = market->yieldCurve(ycname, configuration_);
        for (Size k = 0; k < n_ten; ++k) {
            yieldCurveKeys_.emplace_back(RiskFactorKey::KeyType::YieldCurve, ycname, k);
            Period tenor = simMarketData_->yieldCurveTenors()[k];
            Real disc = ts->discount(today_ + tenor);
            yieldCurveCache_[yieldCurveKeys_[j * n_ten + k]] = ts->discount(today_ + tenor);
            LOG("cache discount " << disc << " for key " << yieldCurveKeys_[j * n_ten + k]);
        }
    }

    // Cache FX rate keys
    fxKeys_.reserve(simMarketData_->fxCcyPairs().size());
    for (Size k = 0; k < simMarketData_->fxCcyPairs().size(); k++) {
        // const string& foreign = simMarketData_->ccys()[k + 1];
        // const string& domestic = simMarketData_->ccys()[0];
        string ccypair = simMarketData_->fxCcyPairs()[k];
        fxKeys_.emplace_back(RiskFactorKey::KeyType::FXSpot, ccypair); // k
        Real fx = market->fxSpot(ccypair, configuration_)->value();
        fxCache_[fxKeys_[k]] = fx;
        LOG("cache FX spot " << fx << " for key " << fxKeys_[k]);
    }

    // Cache Swaption (ATM) vol keys
    Size n_swvol_ccy = simMarketData_->swapVolCcys().size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();
    fxKeys_.reserve(n_swvol_ccy * n_swvol_term * n_swvol_exp);
    Size count = 0;
    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = simMarketData_->swapVolCcys()[i];
        Handle<SwaptionVolatilityStructure> ts = market->swaptionVol(ccy, configuration_);
        Real strike = 0.0; // FIXME
        for (Size j = 0; j < n_swvol_exp; ++j) {
            Period expiry = simMarketData_->swapVolExpiries()[j];
            for (Size k = 0; k < n_swvol_term; ++k) {
                swaptionVolKeys_.emplace_back(RiskFactorKey::KeyType::SwaptionVolatility, ccy, j * n_swvol_term + k);
                Period term = simMarketData_->swapVolTerms()[k];
                Real swvol = ts->volatility(expiry, term, strike);
                swaptionVolCache_[swaptionVolKeys_[count]] = swvol;
                LOG("cache swaption vol " << swvol << " for key " << swaptionVolKeys_[count]);
                count++;
            }
        }
    }

    // Cache FX (ATM) vol keys
    Size n_fxvol_pairs = simMarketData_->fxVolCcyPairs().size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();
    fxVolKeys_.reserve(n_fxvol_pairs * n_fxvol_exp);
    count = 0;
    for (Size j = 0; j < n_fxvol_pairs; ++j) {
        string ccypair = simMarketData_->fxVolCcyPairs()[j];
        Real strike = 0.0; // FIXME
        Handle<BlackVolTermStructure> ts = market->fxVol(ccypair, configuration_);
        for (Size k = 0; k < n_fxvol_exp; ++k) {
            fxVolKeys_.emplace_back(RiskFactorKey::KeyType::FXVolatility, ccypair, k);
            Period expiry = simMarketData_->fxVolExpiries()[k];
            Real fxvol = ts->blackVol(today_ + expiry, strike);
            fxVolCache_[fxVolKeys_[count]] = fxvol;
            LOG("cache FX vol " << fxvol << " for key " << fxVolKeys_[count]);
            count++;
        }
    }

    // Cache CapFloor (Optionlet) vol keys
    Size n_cfvol_ccy = simMarketData_->capFloorVolCcys().size();
    Size n_cfvol_exp = simMarketData_->capFloorVolExpiries().size();
    Size n_cfvol_strikes = simMarketData_->capFloorVolStrikes().size();
    optionletVolKeys_.reserve(n_cfvol_ccy * n_cfvol_strikes * n_cfvol_exp);
    count = 0;
    for (Size i = 0; i < n_cfvol_ccy; ++i) {
        std::string ccy = simMarketData_->capFloorVolCcys()[i];
        Handle<OptionletVolatilityStructure> ts = market->capFloorVol(ccy, configuration_);
        for (Size j = 0; j < n_cfvol_exp; ++j) {
            Period expiry = simMarketData_->capFloorVolExpiries()[j];
            for (Size k = 0; k < n_cfvol_strikes; ++k) {
                optionletVolKeys_.emplace_back(RiskFactorKey::KeyType::OptionletVolatility, ccy,
                                               j * n_cfvol_strikes + k);
                Real strike = simMarketData_->capFloorVolStrikes()[k];
                Real vol = ts->volatility(expiry, strike);
                optionletVolCache_[optionletVolKeys_[count]] = vol;
                LOG("cache optionlet vol " << vol << " for key " << optionletVolKeys_[count]);
                count++;
            }
        }
    }

    LOG("generate base scenario");
    baseScenario_ = baseScenarioFactory_->buildScenario(today_, "BASE");
    addCacheTo(baseScenario_);
    scenarios_.push_back(baseScenario_);
    scenarioDescriptions_.push_back(ScenarioDescription(ScenarioDescription::Type::Base));

    LOG("shift scenario generator, base class initialisation done");
}

boost::shared_ptr<Scenario> ShiftScenarioGenerator::next(const Date& d) {
    QL_REQUIRE(counter_ < scenarios_.size(), "scenario vector size " << scenarios_.size() << " exceeded");
    return scenarios_[counter_++];
}

ShiftScenarioGenerator::ShiftType parseShiftType(const std::string& s) {
    static map<string, ShiftScenarioGenerator::ShiftType> m = {
        { "Absolute", ShiftScenarioGenerator::ShiftType::Absolute },
        { "Relative", ShiftScenarioGenerator::ShiftType::Relative }
    };
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift type " << s << " to ShiftScenarioGenerator::ShiftType");
    }
}

void ShiftScenarioGenerator::addCacheTo(boost::shared_ptr<Scenario> scenario) {
    if (scenario->getNumeraire() == 0.0) // assume zero means numeraire has not been set or updated
        scenario->setNumeraire(numeraireCache_);
    for (auto key : discountCurveKeys_) {
        if (!scenario->has(key))
            scenario->add(key, discountCurveCache_[key]);
    }
    for (auto key : indexCurveKeys_) {
        if (!scenario->has(key))
            scenario->add(key, indexCurveCache_[key]);
    }
    for (auto key : yieldCurveKeys_) {
        if (!scenario->has(key))
            scenario->add(key, yieldCurveCache_[key]);
    }
    for (auto key : fxKeys_) {
        if (!scenario->has(key))
            scenario->add(key, fxCache_[key]);
    }
    if (simMarketData_->simulateFXVols()) {
        for (auto key : fxVolKeys_) {
            if (!scenario->has(key))
                scenario->add(key, fxVolCache_[key]);
        }
    }
    if (simMarketData_->simulateSwapVols()) {
        for (auto key : swaptionVolKeys_) {
            if (!scenario->has(key))
                scenario->add(key, swaptionVolCache_[key]);
        }
    }
    if (simMarketData_->simulateCapFloorVols()) {
        for (auto key : optionletVolKeys_) {
            if (!scenario->has(key))
                scenario->add(key, optionletVolCache_[key]);
        }
    }
}

RiskFactorKey ShiftScenarioGenerator::getFxKey(const std::string& key) {
    for (Size i = 0; i < fxKeys_.size(); ++i) {
        if (fxKeys_[i].name == key)
            return fxKeys_[i];
    }
    QL_FAIL("error locating FX RiskFactorKey for ccy pair " << key);
}

RiskFactorKey ShiftScenarioGenerator::getDiscountKey(const std::string& ccy, Size index) {
    for (Size i = 0; i < discountCurveKeys_.size(); ++i) {
        if (discountCurveKeys_[i].name == ccy && discountCurveKeys_[i].index == index)
            return discountCurveKeys_[i];
    }
    QL_FAIL("error locating Discount RiskFactorKey for " << ccy << ", index " << index);
}

RiskFactorKey ShiftScenarioGenerator::getIndexKey(const std::string& indexName, Size index) {
    for (Size i = 0; i < indexCurveKeys_.size(); ++i) {
        if (indexCurveKeys_[i].name == indexName && indexCurveKeys_[i].index == index)
            return indexCurveKeys_[i];
    }
    QL_FAIL("error locating Index RiskFactorKey for " << indexName << ", index " << index);
}

RiskFactorKey ShiftScenarioGenerator::getYieldKey(const std::string& curveName, Size index) {
    for (Size i = 0; i < yieldCurveKeys_.size(); ++i) {
        if (yieldCurveKeys_[i].name == curveName && yieldCurveKeys_[i].index == index)
            return yieldCurveKeys_[i];
    }
    QL_FAIL("error locating YieldCurve RiskFactorKey for " << curveName << ", index " << index);
}

RiskFactorKey ShiftScenarioGenerator::getSwaptionVolKey(const std::string& ccy, Size index) {
    for (Size i = 0; i < swaptionVolKeys_.size(); ++i) {
        if (swaptionVolKeys_[i].name == ccy && swaptionVolKeys_[i].index == index)
            return swaptionVolKeys_[i];
    }
    QL_FAIL("error locating SwaptionVol RiskFactorKey for " << ccy << ", index " << index);
}

RiskFactorKey ShiftScenarioGenerator::getOptionletVolKey(const std::string& ccy, Size index) {
    for (Size i = 0; i < optionletVolKeys_.size(); ++i) {
        if (optionletVolKeys_[i].name == ccy && optionletVolKeys_[i].index == index)
            return optionletVolKeys_[i];
    }
    QL_FAIL("error locating OptionletVol RiskFactorKey for " << ccy << ", index " << index);
}

RiskFactorKey ShiftScenarioGenerator::getFxVolKey(const std::string& ccypair, Size index) {
    for (Size i = 0; i < fxVolKeys_.size(); ++i) {
        if (fxVolKeys_[i].name == ccypair && fxVolKeys_[i].index == index)
            return fxVolKeys_[i];
    }
    QL_FAIL("error locating FxVol RiskFactorKey for " << ccypair << ", index " << index);
}

void ShiftScenarioGenerator::applyShift(Size j, Real shiftSize, bool up, ShiftType shiftType,
                                        const vector<Time>& tenors, const vector<Real>& values,
                                        const vector<Real>& times, vector<Real>& shiftedValues, bool initialise) {

    QL_REQUIRE(j < tenors.size(), "index j out of range");
    QL_REQUIRE(times.size() == values.size(), "vector size mismatch");
    QL_REQUIRE(shiftedValues.size() == values.size(), "shifted values vector size does not match input");

    Time t1 = tenors[j];
    // FIXME: Handle case where the shift curve is more granular than the original
    ostringstream o_tenors;
    o_tenors << "{";
    for (auto it_tenor : tenors)
        o_tenors << it_tenor << ",";
    o_tenors << "}";
    ostringstream o_times;
    o_times << "{";
    for (auto it_time : times)
        o_times << it_time << ",";
    o_times << "}";
    QL_REQUIRE(tenors.size() <= times.size(),
        "shifted tenor vector " << o_tenors.str() << 
        " cannot be more granular than the base curve tenor vector " 
        << o_times.str());
    auto it = std::find(times.begin(), times.end(), t1);
    QL_REQUIRE(it != times.end(), 
        "shifted tenor node (" << t1 << ") not found in base curve tenor vector " 
        << o_times.str());

    if (initialise) {
        for (Size i = 0; i < values.size(); ++i)
            shiftedValues[i] = values[i];
    }

    if (tenors.size() == 1) { // single shift tenor means parallel shift
        Real w = up ? 1.0 : -1.0;
        for (Size k = 0; k < times.size(); k++) {
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else if (j == 0) { // first shift tenor, flat extrapolation to the left
        Time t2 = tenors[j + 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] <= t1) // full shift
                w = 1.0;
            else if (times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                w = (t2 - times[k]) / (t2 - t1);
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else if (j == tenors.size() - 1) { // last shift tenor, flat extrapolation to the right
        Time t0 = tenors[j - 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                w = (times[k] - t0) / (t1 - t0);
            else if (times[k] > t1) // full shift
                w = 1.0;
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    } else { // intermediate shift tenor
        Time t0 = tenors[j - 1];
        Time t2 = tenors[j + 1];
        for (Size k = 0; k < times.size(); k++) {
            Real w = 0.0;
            if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                w = (times[k] - t0) / (t1 - t0);
            else if (times[k] > t1 && times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                w = (t2 - times[k]) / (t2 - t1);
            if (!up)
                w *= -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedValues[k] += w * shiftSize;
            else
                shiftedValues[k] *= (1.0 + w * shiftSize);
        }
    }
}

void ShiftScenarioGenerator::applyShift(Size i, Size j, Real shiftSize, bool up, ShiftType shiftType,
                                        const vector<Time>& shiftX, const vector<Time>& shiftY,
                                        const vector<Time>& dataX, const vector<Time>& dataY,
                                        const vector<vector<Real> >& data, vector<vector<Real> >& shiftedData,
                                        bool initialise) {
    QL_REQUIRE(shiftX.size() >= 1 && shiftY.size() >= 1, "shift vector size >= 1 reqired");
    QL_REQUIRE(i < shiftX.size(), "index i out of range");
    QL_REQUIRE(j < shiftY.size(), "index j out of range");

    // initalise the shifted data
    if (initialise) {
        for (Size k = 0; k < dataX.size(); ++k) {
            for (Size l = 0; l < dataY.size(); ++l)
                shiftedData[k][l] = data[k][l];
        }
    }

    // single shift point means parallel shift
    if (shiftX.size() == 1 && shiftY.size() == 1) {
        Real w = up ? 1.0 : -1.0;
        for (Size k = 0; k < dataX.size(); ++k) {
            for (Size l = 0; l < dataY.size(); ++l) {
                if (shiftType == ShiftType::Absolute)
                    shiftedData[k][l] += w * shiftSize;
                else
                    shiftedData[k][l] *= (1.0 + w * shiftSize);
            }
        }
        return;
    }

    Size iMax = shiftX.size() - 1;
    Size jMax = shiftY.size() - 1;
    Real tx = shiftX[i];
    Real ty = shiftY[j];
    Real tx1 = i > 0 ? shiftX[i - 1] : QL_MAX_REAL;
    Real ty1 = j > 0 ? shiftY[j - 1] : QL_MAX_REAL;
    Real tx2 = i < iMax ? shiftX[i + 1] : -QL_MAX_REAL;
    Real ty2 = j < jMax ? shiftY[j + 1] : -QL_MAX_REAL;

    for (Size ix = 0; ix < dataX.size(); ++ix) {
        Real x = dataX[ix];
        for (Size iy = 0; iy < dataY.size(); ++iy) {
            Real y = dataY[iy];
            Real wx = 0.0;
            Real wy = 0.0;
            if (x >= tx && x <= tx2 && y >= ty && y <= ty2) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = (ty2 - y) / (ty2 - ty);
            } else if (x >= tx && x <= tx2 && y >= ty1 && y <= ty) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = (y - ty1) / (ty - ty1);
            } else if (x >= tx1 && x <= tx && y >= ty1 && y <= ty) {
                wx = (x - tx1) / (tx - tx1);
                wy = (y - ty1) / (ty - ty1);
            } else if (x >= tx1 && x <= tx && y >= ty && y <= ty2) {
                wx = (x - tx1) / (tx - tx1);
                wy = (ty2 - y) / (ty2 - ty);
            } else if ((x <= tx && i == 0 && y < ty && j == 0) || (x <= tx && i == 0 && y >= ty && j == jMax) ||
                       (x >= tx && i == iMax && y >= ty && j == jMax) || (x >= tx && i == iMax && y < ty && j == 0)) {
                wx = 1.0;
                wy = 1.0;
            } else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty1 && y <= ty) {
                wx = 1.0;
                wy = (y - ty1) / (ty - ty1);
            } else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty && y <= ty2) {
                wx = 1.0;
                wy = (ty2 - y) / (ty2 - ty);
            } else if (x >= tx1 && x <= tx && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
                wx = (x - tx1) / (tx - tx1);
                wy = 1.0;
            } else if (x >= tx && x <= tx2 && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
                wx = (tx2 - x) / (tx2 - tx);
                wy = 1.0;
            }
            QL_REQUIRE(wx >= 0.0 && wx <= 1.0, "wx out of range");
            QL_REQUIRE(wy >= 0.0 && wy <= 1.0, "wy out of range");

            Real w = up ? 1.0 : -1.0;
            if (shiftType == ShiftType::Absolute)
                shiftedData[ix][iy] += w * wx * wy * shiftSize;
            else
                shiftedData[ix][iy] *= (1.0 + w * wx * wy * shiftSize);
        }
    }
}
}
}
