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

#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string/find.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

ReturnConfiguration::ReturnConfiguration()
    : // Default Configuration for risk factor returns
      // For all yield curves we have DFs in the Scenario, for credit we have SurvProbs,
      // so a relative / log change is equivalent to an absolute zero / hazard rate change.
      returnType_({{RiskFactorKey::KeyType::DiscountCurve, ReturnConfiguration::ReturnType::Log},
                   {RiskFactorKey::KeyType::YieldCurve, ReturnConfiguration::ReturnType::Log},
                   {RiskFactorKey::KeyType::IndexCurve, ReturnConfiguration::ReturnType::Log},
                   {RiskFactorKey::KeyType::SwaptionVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::YieldVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::OptionletVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::FXSpot, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::FXVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::EquitySpot, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::EquityVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::DividendYield, ReturnConfiguration::ReturnType::Log},
                   {RiskFactorKey::KeyType::SurvivalProbability, ReturnConfiguration::ReturnType::Log},
                   {RiskFactorKey::KeyType::RecoveryRate, ReturnConfiguration::ReturnType::Absolute},
                   {RiskFactorKey::KeyType::CDSVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::BaseCorrelation, ReturnConfiguration::ReturnType::Absolute},
                   {RiskFactorKey::KeyType::CPIIndex, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::ZeroInflationCurve, ReturnConfiguration::ReturnType::Absolute},
                   {RiskFactorKey::KeyType::YoYInflationCurve, ReturnConfiguration::ReturnType::Absolute},
                   {RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::CommodityCurve, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::CommodityVolatility, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::SecuritySpread, ReturnConfiguration::ReturnType::Relative},
                   {RiskFactorKey::KeyType::Correlation, ReturnConfiguration::ReturnType::Absolute}}) {}

ReturnConfiguration::ReturnConfiguration(const std::map<RiskFactorKey::KeyType, ReturnType>& returnType)
    : returnType_(returnType) {}

Real ReturnConfiguration::returnValue(const RiskFactorKey& key, const Real v1, const Real v2, const QuantLib::Date& d1,
                                      const QuantLib::Date& d2) const {

    // Checks
    check(key);
    
    // Calculate the return
    auto keyType = key.keytype;    
    switch (returnType_.at(keyType)) {
    case ReturnConfiguration::ReturnType::Absolute:
        return v2 - v1;
        break;
    case ReturnConfiguration::ReturnType::Relative:
        if (!close(v1, 0)) {
            return v2 / v1 - 1.0;
        } else {
            ALOG("Cannot calculate the relative return for key " << key << " so just returning 0: (" << d1 << "," << v1
                                                                 << ") to (" << d2 << "," << v2 << ")");
            return 0.0;
        }
        break;
    case ReturnConfiguration::ReturnType::Log:
        if (!close(v1, 0) && v2 / v1 > 0) {
            return std::log(v2 / v1);
        } else {
            ALOG("Cannot calculate the relative return for key " << key << " so just returning 0: (" << d1 << "," << v1
                                                                 << ") to (" << d2 << "," << v2 << ")");
            return 0.0;
        }
        break;
    default:
        QL_FAIL("ReturnConfiguration: return type not covered for key " << key << ".");
    }
}

Real ReturnConfiguration::applyReturn(const RiskFactorKey& key, const Real baseValue, const Real returnValue) const {

    // Checks
    check(key);

    // Apply the return to the base value to generate the return value
    auto keyType = key.keytype;
    Real value;
    switch (returnType_.at(keyType)) {
    case ReturnConfiguration::ReturnType::Absolute:
        value = baseValue + returnValue;
        break;
    case ReturnConfiguration::ReturnType::Relative:
        value = baseValue * (1.0 + returnValue);
        break;
    case ReturnConfiguration::ReturnType::Log:
        value = baseValue * std::exp(returnValue);
        break;
    default:
        QL_FAIL("ReturnConfiguration: return type for key " << key << " not covered");
    }

    // Apply cap / floors to guarantee admissable values
    if ((keyType == RiskFactorKey::KeyType::BaseCorrelation || keyType == RiskFactorKey::KeyType::Correlation) && 
        (value > 1.0 || value < -1.0)) {
        DLOG("Base correlation value, " << value << ", is not in range [-1.0, 1.0]");
        value = std::max(std::min(value, 1.0), -1.0);
        DLOG("Base correlation value amended to " << value);
    }

    if ((keyType == RiskFactorKey::KeyType::RecoveryRate || keyType == RiskFactorKey::KeyType::SurvivalProbability)
        && (value > 1.0 || value < 0.0)) {
        DLOG("Value of risk factor " << key << ", " << value << ", is not in range [0.0, 1.0]");
        value = std::max(std::min(value, 1.0), 0.0);
        DLOG("Value of risk factor " << key << " amended to " << value);
    }

    return value;
}

void ReturnConfiguration::check(const RiskFactorKey& key) const {

    auto keyType = key.keytype;
    QL_REQUIRE(keyType != RiskFactorKey::KeyType::None, "unsupported key type none for key " << key);
    QL_REQUIRE(returnType_.find(keyType) != returnType_.end(),
               "ReturnConfiguration: key type " << keyType << " for key " << key << " not found");
}

HistoricalScenarioGenerator::HistoricalScenarioGenerator(
    const boost::shared_ptr<HistoricalScenarioLoader>& historicalScenarioLoader,
    const boost::shared_ptr<ScenarioFactory>& scenarioFactory, const QuantLib::Calendar& cal, const Size mporDays,
    const bool overlapping, const ReturnConfiguration& returnConfiguration,
    const boost::shared_ptr<ore::data::AdjustmentFactors>& adjFactors, const std::string& labelPrefix)
    : i_(0), historicalScenarioLoader_(historicalScenarioLoader), scenarioFactory_(scenarioFactory), cal_(cal),
      mporDays_(mporDays), overlapping_(overlapping), returnConfiguration_(returnConfiguration),
      adjFactors_(adjFactors), labelPrefix_(labelPrefix) {

    QL_REQUIRE(mporDays > 0, "Invalid mpor days of 0");

    // check they are in order and all before the base scenario
    // we do not make any assumptions here about the length
    for (Size i = 1; i < historicalScenarioLoader_->numScenarios(); ++i) {
        QL_REQUIRE(historicalScenarioLoader_->dates()[i] > historicalScenarioLoader_->dates()[i - 1],
                   "historical scenarios are not ordered");
    }
    setDates();
}

void HistoricalScenarioGenerator::setDates() {
    // construct the vectors of start and end dates
    for (Size i = 0; i < historicalScenarioLoader_->numScenarios();) {
        Date sDate = historicalScenarioLoader_->dates()[i];
        Date eDate = cal_.advance(sDate, mporDays_ * Days);
        auto it =
            std::find(historicalScenarioLoader_->dates().begin(), historicalScenarioLoader_->dates().end(), eDate);
        if (it != historicalScenarioLoader_->dates().end()) {
            startDates_.push_back(sDate);
            endDates_.push_back(eDate);
        }
        if (overlapping_) {
            ++i;
        } else {
            if (it != historicalScenarioLoader_->dates().end()) {
                i = std::distance(historicalScenarioLoader_->dates().begin(), it);
            } else {
                i = std::distance(historicalScenarioLoader_->dates().begin(),
                                  std::upper_bound(historicalScenarioLoader_->dates().begin(),
                                                   historicalScenarioLoader_->dates().end(), eDate));
            }
        }
    }
}

std::pair<boost::shared_ptr<Scenario>, boost::shared_ptr<Scenario>> HistoricalScenarioGenerator::scenarioPair() {
    // Get the two historicals we are using
    QL_REQUIRE(i_ < numScenarios(),
               "Cannot generate any more scenarios (i=" << i_ << " numScenarios=" << numScenarios() << ")");
    boost::shared_ptr<Scenario> s1 = historicalScenarioLoader_->getHistoricalScenario(startDates_[i_]);
    boost::shared_ptr<Scenario> s2 = historicalScenarioLoader_->getHistoricalScenario(endDates_[i_]);
    return std::pair<boost::shared_ptr<Scenario>, boost::shared_ptr<Scenario>>(s1, s2);
}

Real HistoricalScenarioGenerator::adjustedPrice(RiskFactorKey key, Date d, Real price) {
    if (adjFactors_) {
        if (key.keytype == RiskFactorKey::KeyType::EquitySpot) {
            // uses the ORE Fixing name convention
            return price * adjFactors_->getFactor(key.name, d);
        }
    }
    return price;
}

boost::shared_ptr<Scenario> HistoricalScenarioGenerator::next(const Date& d) {

    QL_REQUIRE(baseScenario_ != nullptr, "HistoricalScenarioGenerator: base scenario not set");

    std::pair<boost::shared_ptr<Scenario>, boost::shared_ptr<Scenario>> scens = scenarioPair();
    boost::shared_ptr<Scenario> s1 = scens.first;
    boost::shared_ptr<Scenario> s2 = scens.second;

    // build the scenarios
    QL_REQUIRE(d >= baseScenario_->asof(), "Cannot generate a scenario in the past");
    boost::shared_ptr<Scenario> scen = scenarioFactory_->buildScenario(d, "", 1.0);

    // loop over all keys
    for (auto key : baseScenario_->keys()) {
        Real base = baseScenario_->get(key);
        Real v1 = 1.0, v2 = 1.0;
        if (!s1->has(key) || !s2->has(key)) {
            DLOG("Missing key in historical scenario (" << io::iso_date(s1->asof()) << "," << io::iso_date(s2->asof())
                                                        << "): " << key << " => no move in this factor");
        } else {
            v1 = adjustedPrice(key, s1->asof(), s1->get(key));
            v2 = adjustedPrice(key, s2->asof(), s2->get(key));
        }
        Real value = 0.0;

        // Calculate the returned value
        Real returnVal = returnConfiguration_.returnValue(key, v1, v2, s1->asof(), s2->asof());
        // Adjust return for any scaling
        returnVal = returnVal * scaling(key, returnVal);
        // Calculate the shifted value
        value = returnConfiguration_.applyReturn(key, base, returnVal);
        if (std::isinf(value)) {
            ALOG("Value is inf for " << key << " from date " << s1->asof() << " to " << s2->asof());
        }
        // Add it
        scen->add(key, value);
    }

    // Label the scenario
    string label = labelPrefix_ + ore::data::to_string(io::iso_date(s1->asof())) + "_" +
        ore::data::to_string(io::iso_date(s2->asof()));
    scen->label(label);

    // return it.
    ++i_;
    return scen;
}

Size HistoricalScenarioGenerator::numScenarios() const {
    // We have a start date for each valid scenario
    return startDates_.size();
}

std::vector<std::pair<Date, Date>> HistoricalScenarioGenerator::filteredScenarioDates(const TimePeriod& period) const {
    std::vector<std::pair<Date, Date>> result;
    for (Size s = 0; s < startDates_.size(); ++s) {
        if (period.contains(startDates_[s]) && period.contains(endDates_[s])) {
            result.push_back(std::make_pair(startDates_[s], endDates_[s]));
        }
    }
    return result;
}

boost::shared_ptr<Scenario> HistoricalScenarioGeneratorRandom::next(const Date& d) {

    QL_REQUIRE(baseScenario() != nullptr, "HistoricalScenarioGeneratorRandom: base scenario not set");

    // build the scenarios
    QL_REQUIRE(d >= baseScenario()->asof(),
               "HistoricalScenarioGeneratorRandom: Cannot generate a scenario in the past");
    boost::shared_ptr<Scenario> scen = scenarioFactory_->buildScenario(d, "", 1.0);

    // loop over all keys
    for (auto key : baseScenario()->keys()) {
        Real base = baseScenario()->get(key);
        Real value = 0.0;
        switch (key.keytype) {
        case RiskFactorKey::KeyType::DiscountCurve:
        case RiskFactorKey::KeyType::DividendYield:
        case RiskFactorKey::KeyType::YieldCurve:
        case RiskFactorKey::KeyType::IndexCurve:
        case RiskFactorKey::KeyType::SurvivalProbability:
            if (close_enough(base, 0.0))
                value = 0.0;
            else
                value = 1.0 - (1.0 - base) * (1.0 + normalrng_->next().value * 0.05);
            break;
        case RiskFactorKey::KeyType::ZeroInflationCurve:
        case RiskFactorKey::KeyType::YoYInflationCurve:
            value = base + (normalrng_->next().value * 0.0010);
            break;
        case RiskFactorKey::KeyType::FXSpot:
        case RiskFactorKey::KeyType::EquitySpot:
        case RiskFactorKey::KeyType::SwaptionVolatility:
        case RiskFactorKey::KeyType::YieldVolatility:
        case RiskFactorKey::KeyType::OptionletVolatility:
        case RiskFactorKey::KeyType::CDSVolatility:
        case RiskFactorKey::KeyType::FXVolatility:
        case RiskFactorKey::KeyType::EquityVolatility:
        case RiskFactorKey::KeyType::SecuritySpread:
            value = base * (1.0 + normalrng_->next().value * 0.02);
            break;
        case RiskFactorKey::KeyType::BaseCorrelation:
            value = base + (normalrng_->next().value * 0.05);
            value = std::max(std::min(value, 0.9999), -0.9999);
            break;
        default:
            QL_FAIL("HistoricalScenarioGeneratorRandom: unexpected key type in key " << key);
        }
        scen->add(key, value);
    }

    // return it.
    ++i_;
    return scen;
}

void HistoricalScenarioGeneratorRandom::reset() {
    HistoricalScenarioGenerator::reset();
    normalrng_ = boost::make_shared<QuantLib::PseudoRandom::rng_type>(MersenneTwisterUniformRng(42));
}

boost::shared_ptr<Scenario> HistoricalScenarioGeneratorTransform::next(const Date& d) {
    boost::shared_ptr<Scenario> scenario = hsg_->next(d)->clone();
    const vector<RiskFactorKey>& keys = hsg_->baseScenario()->keys();
    Date asof = hsg_->baseScenario()->asof();
    vector<Period> tenors;
    Date endDate;
    DayCounter dc;

    for (Size k = 0; k < keys.size(); ++k) {
        if ((keys[k].keytype == RiskFactorKey::KeyType::DiscountCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::IndexCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::SurvivalProbability)) {
            Real df = scenario->get(keys[k]);
            Real compound = 1 / df;
            if ((keys[k].keytype == RiskFactorKey::KeyType::DiscountCurve)) {
                dc = simMarket_->discountCurve(keys[k].name)->dayCounter();
                tenors = simMarketConfig_->yieldCurveTenors(keys[k].name);
            } else if (keys[k].keytype == RiskFactorKey::KeyType::IndexCurve) {
                dc = simMarket_->iborIndex(keys[k].name)->dayCounter();
                tenors = simMarketConfig_->yieldCurveTenors(keys[k].name);
            } else if (keys[k].keytype == RiskFactorKey::KeyType::SurvivalProbability) {
                dc = simMarket_->defaultCurve(keys[k].name)->curve()->dayCounter();
                tenors = simMarketConfig_->defaultTenors(keys[k].name);
            }
            endDate = asof + tenors[keys[k].index];
            Real zero = InterestRate::impliedRate(compound, dc, QuantLib::Compounding::Continuous,
                                                  QuantLib::Frequency::Annual, asof, endDate)
                            .rate();
            scenario->add(keys[k], zero);
        }
    }
    return scenario;
}

void HistoricalScenarioGeneratorTransform::reset() { hsg_->reset(); }

HistoricalScenarioGeneratorWithFilteredDates::HistoricalScenarioGeneratorWithFilteredDates(
    const std::vector<TimePeriod>& filter, const boost::shared_ptr<HistoricalScenarioGenerator>& gen)
    : HistoricalScenarioGenerator(gen->scenarioLoader(), gen->scenarioFactory(), gen->cal(), gen->mporDays(),
        gen->overlapping(), gen->returnConfiguration(), gen->adjFactors(), gen->labelPrefix()),
      gen_(gen), i_orig_(0) {

    // set base scenario

    baseScenario_ = gen->baseScenario();

    // filter start / end dates on relevant scenarios

    isRelevantScenario_ = std::vector<bool>(gen->numScenarios(), false);

    std::vector<Date> newStartDates, newEndDates;

    for (Size i = 0; i < startDates_.size(); ++i) {
        isRelevantScenario_[i] = false;
        for (auto const& f : filter) {
            if (f.contains(startDates_[i]) && f.contains(endDates_[i]))
                isRelevantScenario_[i] = true;
        }
        if (isRelevantScenario_[i]) {
            newStartDates.push_back(startDates_[i]);
            newEndDates.push_back(endDates_[i]);
        }
    }

    startDates_ = newStartDates;
    endDates_ = newEndDates;
}

void HistoricalScenarioGeneratorWithFilteredDates::reset() {
    gen_->reset();
    HistoricalScenarioGenerator::reset();
    i_orig_ = 0;
}

boost::shared_ptr<Scenario> HistoricalScenarioGeneratorWithFilteredDates::next(const Date& d) {
    while (i_orig_ < gen_->numScenarios() && !isRelevantScenario_[i_orig_]) {
        gen_->next(d);
        ++i_orig_;
    }
    QL_REQUIRE(i_orig_ < gen_->numScenarios(),
               "HistoricalScenarioGeneratorWithFilteredDates:next(): no more scenarios available");
    ++i_orig_;
    return gen_->next(d);
}

} // namespace analytics
} // namespace ore
