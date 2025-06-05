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

#include <boost/algorithm/string/find.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/scenarioutilities.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/zerotoparscenariogenerator.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/termstructures/scenario.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

ReturnConfiguration::ReturnConfiguration() {
    const static std::vector<std::pair<RiskFactorKey::KeyType, ReturnConfiguration::ReturnType>> defaultConfig = {
        {RiskFactorKey::KeyType::DiscountCurve, ReturnConfiguration::ReturnType::Log},
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
        {RiskFactorKey::KeyType::SecuritySpread, ReturnConfiguration::ReturnType::Absolute},
        {RiskFactorKey::KeyType::Correlation, ReturnConfiguration::ReturnType::Absolute}};

    // Default Configuration for risk factor returns
    // For all yield curves we have DFs in the Scenario, for credit we have SurvProbs,
    // so a relative / log change is equivalent to an absolute zero / hazard rate change.
    for (const auto& [key, rt] : defaultConfig) {
        Return r;
        r.displacement = 0.0;
        r.type = rt;
        RiskFactorConfig data = std::make_pair(r, IndividualRiskFactorConfig());
        returnType_.insert(std::make_pair(key, data));
    }
}

ReturnConfiguration::ReturnConfiguration(
    const std::map<RiskFactorKey::KeyType, ReturnConfiguration::ReturnType>& returnType) {
    for (const auto& [key, rt] : returnType) {
        Return r;
        r.displacement = 0.0;
        r.type = rt;
        RiskFactorConfig data = std::make_pair(r, IndividualRiskFactorConfig());
        returnType_.insert(std::make_pair(key, data));
    }
}

Real ReturnConfiguration::returnValue(const RiskFactorKey& key, const Real v1, const Real v2, const QuantLib::Date& d1,
                                      const QuantLib::Date& d2) const {

    // Checks
    check(key);

    const auto rt = returnType(key);
    // check if we have a config for the key otherwise use

    // Calculate the return
    switch (rt.type) {
    case ReturnConfiguration::ReturnType::Absolute:
        return v2 - v1;
        break;
    case ReturnConfiguration::ReturnType::Relative:
        if (!close(v1 + rt.displacement, 0)) {
            return (v2 + rt.displacement) / (v1 + rt.displacement) - 1.0;
        } else {
            ALOG("Cannot calculate the relative return for key " << key << " so just returning 0: (" << d1 << "," << v1
                                                                 << ") to (" << d2 << "," << v2 << ")");
            return 0.0;
        }
        break;
    case ReturnConfiguration::ReturnType::Log:
        if (!close(v1 + rt.displacement, 0) && (v2 + rt.displacement) / (v1 + rt.displacement) > 0) {
            return std::log((v2 + rt.displacement) / (v1 + rt.displacement));
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
    const auto rt = returnType(key);
    // Apply the return to the base value to generate the return value
    Real value;
    switch (rt.type) {
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
    const auto keyType = key.keytype;
    // Apply cap / floors to guarantee admissable values
    if ((keyType == RiskFactorKey::KeyType::BaseCorrelation || keyType == RiskFactorKey::KeyType::Correlation) &&
        (value > 1.0 || value < -1.0)) {
        DLOG("Base correlation value, " << value << ", is not in range [-1.0, 1.0]");
        value = std::max(std::min(value, 1.0), -1.0);
        DLOG("Base correlation value amended to " << value);
    }

    if ((keyType == RiskFactorKey::KeyType::RecoveryRate || keyType == RiskFactorKey::KeyType::SurvivalProbability) &&
        (value > 1.0 || value < 0.0)) {
        DLOG("Value of risk factor " << key << ", " << value << ", is not in range [0.0, 1.0]");
        value = std::max(std::min(value, 1.0), 0.0);
        DLOG("Value of risk factor " << key << " amended to " << value);
    }

    return value;
}

const ReturnConfiguration::Return& ReturnConfiguration::returnType(const RiskFactorKey& key) const {
    const auto it = returnType_.find(key.keytype);
    QL_REQUIRE(it != returnType_.end(), "No ReturnType found for keyType" << key.keytype);
    const auto& [config, specializedConfigs] = it->second;
    const auto specializedIt = specializedConfigs.find(key.name);
    return (specializedIt == specializedConfigs.end()) ? config : specializedIt->second;
}

std::ostream& operator<<(std::ostream& out, const ReturnConfiguration::ReturnType t) {
    switch (t) {
    case ReturnConfiguration::ReturnType::Absolute:
        return out << "Absolute";
    case ReturnConfiguration::ReturnType::Relative:
        return out << "Relative";
    case ReturnConfiguration::ReturnType::Log:
        return out << "Log";
    default:
        return out << "Unknown ReturnType (" << static_cast<int>(t) << ")";
    }
}

void ReturnConfiguration::check(const RiskFactorKey& key) const {

    auto keyType = key.keytype;
    QL_REQUIRE(keyType != RiskFactorKey::KeyType::None, "unsupported key type none for key " << key);
    QL_REQUIRE(returnType_.find(keyType) != returnType_.end(),
               "ReturnConfiguration: key type " << keyType << " for key " << key << " not found");
}

ReturnConfiguration::ReturnType parseReturnType(const std::string& typeStr) {
    if (typeStr == "Log")
        return ReturnConfiguration::ReturnType::Log;
    else if (typeStr == "Absolute")
        return ReturnConfiguration::ReturnType::Absolute;
    else if (typeStr == "Relative")
        return ReturnConfiguration::ReturnType::Relative;
    else
        QL_FAIL("Unknown ReturnType: " << typeStr);
}

void ReturnConfiguration::fromXML(XMLNode* node) {
    returnType_.clear();
    XMLUtils::checkNode(node, "ReturnConfigurations");
    for (auto* rcNode : XMLUtils::getChildrenNodes(node, "ReturnConfiguration")) {
        // Key für den RiskFactor (z.B. "IR:EUR")
        std::string keyStr = XMLUtils::getAttribute(rcNode, "key");
        auto key = QuantExt::parseRiskFactorKeyType(keyStr);
        // Default Return
        XMLNode* retNode = XMLUtils::getChildNode(rcNode, "Return");
        QL_REQUIRE(retNode, "Return node missing in ReturnConfiguration");
        Return defaultReturn;
        std::string typeStr = XMLUtils::getChildValue(retNode, "Type", true);
        defaultReturn.type = parseReturnType(typeStr);
        defaultReturn.displacement = XMLUtils::getChildValueAsDouble(retNode, "Displacement", false, 0.0);

        // Specialized Configurations
        IndividualRiskFactorConfig specialized;
        XMLNode* specNode = XMLUtils::getChildNode(rcNode, "SpecializedConfigurations");
        if (specNode) {
            for (auto* sRetNode : XMLUtils::getChildrenNodes(specNode, "Return")) {
                std::string sKey = XMLUtils::getAttribute(sRetNode, "key");
                Return specializedReturn = defaultReturn; // Default übernehmen, falls nicht überschrieben
                std::string sTypeStr = XMLUtils::getChildValue(sRetNode, "Type", false);
                if (!sTypeStr.empty()) {
                    specializedReturn.type = parseReturnType(sTypeStr);
                }
                specializedReturn.displacement =
                    XMLUtils::getChildValueAsDouble(sRetNode, "Displacement", false, specializedReturn.displacement);
                specialized[sKey] = specializedReturn;
            }
        }
        returnType_[key] = std::make_pair(defaultReturn, specialized);
    }
}

XMLNode* ReturnConfiguration::toXML(XMLDocument& doc) const {
    XMLNode* root = doc.allocNode("ReturnConfigurations");
    for (const auto& [key, configPair] : returnType_) {
        XMLNode* rcNode = doc.allocNode("ReturnConfiguration");
        // Key als Attribut
        XMLUtils::addAttribute(doc, rcNode, "key", ore::data::to_string(key));
        // Default Return
        const Return& defReturn = configPair.first;
        XMLNode* retNode = doc.allocNode("Return");
        XMLUtils::addChild(doc, retNode, "Type", ore::data::to_string(defReturn.type));
        XMLUtils::addChild(doc, retNode, "Displacement", defReturn.displacement);
        XMLUtils::appendNode(rcNode, retNode);

        // Specialized Configurations
        const IndividualRiskFactorConfig& specialized = configPair.second;
        if (!specialized.empty()) {
            XMLNode* specNode = doc.allocNode("SpecializedConfigurations");
            for (const auto& [sKey, sReturn] : specialized) {
                XMLNode* sRetNode = doc.allocNode("Return");
                XMLUtils::addAttribute(doc, sRetNode, "key", sKey);
                XMLUtils::addChild(doc, sRetNode, "Type", ore::data::to_string(sReturn.type));
                XMLUtils::addChild(doc, sRetNode, "Displacement", sReturn.displacement);
                XMLUtils::appendNode(specNode, sRetNode);
            }
            XMLUtils::appendNode(rcNode, specNode);
        }
        XMLUtils::appendNode(root, rcNode);
    }
    return root;
}

HistoricalScenarioGenerator::HistoricalScenarioGenerator(
    const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& historicalScenarioLoader,
    const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory, const QuantLib::Calendar& cal,
    const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors, const Size mporDays,
    const bool overlapping, const ReturnConfiguration& returnConfiguration, const std::string& labelPrefix,
    const bool generateDifferenceScenarios)
    : i_(0), historicalScenarioLoader_(historicalScenarioLoader), scenarioFactory_(scenarioFactory), 
      cal_(cal), mporDays_(mporDays), adjFactors_(adjFactors), overlapping_(overlapping),
      returnConfiguration_(returnConfiguration), labelPrefix_(labelPrefix), generateDifferenceScenarios_(generateDifferenceScenarios) {

    QL_REQUIRE(mporDays > 0, "Invalid mpor days of 0");
    QL_REQUIRE(historicalScenarioLoader_->numScenarios() > 1,
               "HistoricalScenarioGenerator: require more than 1 scenario from historicalScenarioLoader_");

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
        auto dates = historicalScenarioLoader_->dates();
        Date sDate = dates[i];
        Date eDate = cal_.advance(sDate, mporDays_ * Days);
        auto it = std::find(dates.begin(), dates.end(), eDate);
        if (it != dates.end()) {
            startDates_.push_back(sDate);
            endDates_.push_back(eDate);
        }
        if (overlapping_) {
            ++i;
        } else {
            if (it != dates.end()) {
                i = std::distance(dates.begin(), it);
            } else {
                i = std::distance(dates.begin(),
                                  std::upper_bound(dates.begin(), dates.end(), eDate));
            }
        }
    }
}

HistoricalScenarioGenerator::HistoricalScenarioGenerator(
    const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& historicalScenarioLoader,
    const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory,
    const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors,
    const ReturnConfiguration& returnConfiguration,
    const std::string& labelPrefix, const bool generateDifferenceScenarios)
    : i_(0), historicalScenarioLoader_(historicalScenarioLoader), scenarioFactory_(scenarioFactory),
      adjFactors_(adjFactors), returnConfiguration_(returnConfiguration),
      labelPrefix_(labelPrefix), generateDifferenceScenarios_(generateDifferenceScenarios) {

    QL_REQUIRE(historicalScenarioLoader_->numScenarios() > 1,
               "HistoricalScenarioGenerator: require more than 1 scenario from historicalScenarioLoader_");

    // check they are in order and all before the base scenario and add the dates
    for (Size i = 1; i < historicalScenarioLoader_->numScenarios(); ++i) {
        QL_REQUIRE(historicalScenarioLoader_->dates()[i] > historicalScenarioLoader_->dates()[i - 1],
                   "historical scenarios are not ordered");
        startDates_.push_back(historicalScenarioLoader_->dates()[i - 1]);
        endDates_.push_back(historicalScenarioLoader_->dates()[i]);
    }
}

std::pair<QuantLib::ext::shared_ptr<Scenario>, QuantLib::ext::shared_ptr<Scenario>> HistoricalScenarioGenerator::scenarioPair() {
    // Get the two historicals we are using
    QL_REQUIRE(i_ < numScenarios(),
               "Cannot generate any more scenarios (i=" << i_ << " numScenarios=" << numScenarios() << ")");
    QuantLib::ext::shared_ptr<Scenario> s1 = historicalScenarioLoader_->getScenario(startDates_[i_]);
    QuantLib::ext::shared_ptr<Scenario> s2 = historicalScenarioLoader_->getScenario(endDates_[i_]);
    return std::pair<QuantLib::ext::shared_ptr<Scenario>, QuantLib::ext::shared_ptr<Scenario>>(s1, s2);
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

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioGenerator::next(const Date& d) {

    QL_REQUIRE(baseScenario_ != nullptr, "HistoricalScenarioGenerator: base scenario not set");

    std::pair<QuantLib::ext::shared_ptr<Scenario>, QuantLib::ext::shared_ptr<Scenario>> scens = scenarioPair();
    QuantLib::ext::shared_ptr<Scenario> s1 = scens.first;
    QuantLib::ext::shared_ptr<Scenario> s2 = scens.second;

    // build the scenarios
    QL_REQUIRE(d >= baseScenario_->asof(), "Cannot generate a scenario in the past");
    QuantLib::ext::shared_ptr<Scenario> scen = scenarioFactory_->buildScenario(d, !generateDifferenceScenarios_, false, std::string(), 1.0);

    // loop over all keys
    calculationDetails_.resize(baseScenario_->keys().size());
    Size calcDetailsCounter = 0;
    for (auto const& key : baseScenario_->keys()) {

        if (!s1->has(key) || !s2->has(key))
            DLOG("Missing key in historical scenario (" << io::iso_date(s1->asof()) << "," << io::iso_date(s2->asof())
                                                        << "): " << key << " => no move in this factor");

        if (generateDifferenceScenarios_)
            scen = getDifferenceScenario(s1, s2, d, 1.0); 
        else {
            Real base = baseScenario_->get(key);
            Real v1 = 1.0, v2 = 1.0;
            if (!s1->has(key) || !s2->has(key)) {
                DLOG("Missing key in historical scenario (" << io::iso_date(s1->asof()) << ","
                                                            << io::iso_date(s2->asof()) << "): " << key
                                                            << " => no move in this factor");
            } else {
                v1 = adjustedPrice(key, s1->asof(), s1->get(key));
                v2 = adjustedPrice(key, s2->asof(), s2->get(key));
            }
            Real value = 0.0;

            // Calculate the returned value
            Real returnVal = returnConfiguration_.returnValue(key, v1, v2, s1->asof(), s2->asof());
            // Adjust return for any scaling
            Real scaling = this->scaling(key, returnVal);
            returnVal = returnVal * scaling;
            // Calculate the shifted value
            value = returnConfiguration_.applyReturn(key, base, returnVal);
            if (std::isinf(value)) {
                ALOG("Value is inf for " << key << " from date " << s1->asof() << " to " << s2->asof());
            }
            // Add it
            scen->add(key, value);

            auto returnType = returnConfiguration_.returnType(key);
            // Populate calculation details
            calculationDetails_[calcDetailsCounter].baseValue = base;
            calculationDetails_[calcDetailsCounter].scenarioValue1 = v1;
            calculationDetails_[calcDetailsCounter].scenarioValue2 = v2;
            calculationDetails_[calcDetailsCounter].returnType = returnType.type;
            calculationDetails_[calcDetailsCounter].displacement = returnType.displacement;
            calculationDetails_[calcDetailsCounter].scaling = scaling;
            calculationDetails_[calcDetailsCounter].returnValue = returnVal;
            calculationDetails_[calcDetailsCounter].scenarioValue = value;
            calculationDetails_[calcDetailsCounter].adjustmentFactor1 =
                adjFactors_ ? adjFactors_->getFactor(key.name, s1->asof()) : 1.0;
            calculationDetails_[calcDetailsCounter].adjustmentFactor2 =
                adjFactors_ ? adjFactors_->getFactor(key.name, s2->asof()) : 1.0;
        }
        // Populate calculation details
        calculationDetails_[calcDetailsCounter].scenarioDate1 = s1->asof();
        calculationDetails_[calcDetailsCounter].scenarioDate2 = s2->asof();
        calculationDetails_[calcDetailsCounter].key = key;
        ++calcDetailsCounter;
    }

    // Label the scenario
    string label = labelPrefix_ + ore::data::to_string(io::iso_date(s1->asof())) + "_" +
        ore::data::to_string(io::iso_date(s2->asof()));
    scen->label(label);

    // return it.
    ++i_;
    return scen;
}

const std::vector<HistoricalScenarioGenerator::HistoricalScenarioCalculationDetails>&
HistoricalScenarioGenerator::lastHistoricalScenarioCalculationDetails() const {
    return calculationDetails_;
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

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioGeneratorRandom::next(const Date& d) {

    QL_REQUIRE(baseScenario() != nullptr, "HistoricalScenarioGeneratorRandom: base scenario not set");

    // build the scenarios
    QL_REQUIRE(d >= baseScenario()->asof(),
               "HistoricalScenarioGeneratorRandom: Cannot generate a scenario in the past");
    QuantLib::ext::shared_ptr<Scenario> scen = scenarioFactory_->buildScenario(d, true, false, std::string(), 1.0);

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
    normalrng_ = QuantLib::ext::make_shared<QuantLib::PseudoRandom::rng_type>(MersenneTwisterUniformRng(42));
}

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioGeneratorTransform::next(const Date& d) {
    QuantLib::ext::shared_ptr<Scenario> scenario = HistoricalScenarioGenerator::next(d)->clone();
    const vector<RiskFactorKey>& keys = baseScenario()->keys();
    Date asof = baseScenario()->asof();
    vector<Period> tenors;
    Date endDate;
    DayCounter dc;

    for (Size k = 0; k < keys.size(); ++k) {
        if ((keys[k].keytype == RiskFactorKey::KeyType::DiscountCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::IndexCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::SurvivalProbability)) {
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

            auto toZero = [&dc, &asof, &endDate](double compound) {
                return InterestRate::impliedRate(compound, dc, QuantLib::Compounding::Continuous,
                                                 QuantLib::Frequency::Annual, asof, endDate)
                    .rate();
            };

            Real zero = toZero(1.0 / scenario->get(keys[k]));
            scenario->add(keys[k], zero);
            // update calc details
            calculationDetails_[k].baseValue = toZero(1.0 / calculationDetails_[k].baseValue);
            calculationDetails_[k].scenarioValue1 = toZero(1.0 / calculationDetails_[k].scenarioValue1);
            calculationDetails_[k].scenarioValue2 = toZero(1.0 / calculationDetails_[k].scenarioValue2);
            calculationDetails_[k].returnType = ReturnConfiguration::ReturnType::Absolute;
            calculationDetails_[k].returnValue =
                calculationDetails_[k].scaling *
                (calculationDetails_[k].scenarioValue2 - calculationDetails_[k].scenarioValue1);
            calculationDetails_[k].scenarioValue = toZero(1.0 / calculationDetails_[k].scenarioValue);
        }
    }
    return scenario;
}

HistoricalScenarioGeneratorWithFilteredDates::HistoricalScenarioGeneratorWithFilteredDates(
    const std::vector<TimePeriod>& filter, const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& gen)
    : HistoricalScenarioGenerator(*gen),
      gen_(gen), i_orig_(0) {

    // set base scenario    
    auto zpScen = QuantLib::ext::dynamic_pointer_cast<ZeroToParScenarioGenerator>(gen);
    if (zpScen)
        baseScenario_ = zpScen->baseScenario();
    else
        baseScenario_ = gen->baseScenario();

    for (auto const& f : filter) {
        // Check that backtest and benchmark periods are covered by the historical scenario generator
        Date minDate = *std::min_element(f.startDates().begin(), f.startDates().end());
        Date maxDate = *std::max_element(f.endDates().begin(), f.endDates().end());
         
        QL_REQUIRE(startDates_.front() <= minDate && maxDate <= endDates_.back(),
             "The backtesting period " << f << " is not covered by the historical scenario generator: Required dates = ["
             << ore::data::to_string(minDate) << "," << ore::data::to_string(maxDate)
             << "], Covered dates = [" << startDates_.front() << "," << endDates_.back() << "]");
    }

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

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioGeneratorWithFilteredDates::next(const Date& d) {
    while (i_orig_ < gen_->numScenarios() && !isRelevantScenario_[i_orig_]) {
        gen_->next(d);
        ++i_orig_;
    }
    QL_REQUIRE(i_orig_ < gen_->numScenarios(),
               "HistoricalScenarioGeneratorWithFilteredDates:next(): no more scenarios available");
    ++i_orig_;
    return gen_->next(d);
}

QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> buildHistoricalScenarioGenerator(
    const QuantLib::ext::shared_ptr<ScenarioReader>& hsr,
    const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors, const TimePeriod& period,
    Calendar calendar, Size mporDays,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
    const QuantLib::ext::shared_ptr<TodaysMarketParameters>& marketParams, const bool overlapping) {

    hsr->load(simParams, marketParams);

    auto scenarioFactory = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);

    QuantLib::ext::shared_ptr<HistoricalScenarioLoader> scenarioLoader = QuantLib::ext::make_shared<HistoricalScenarioLoader>(
        hsr, period.startDates().front(), period.endDates().front(), calendar);

    // Create the historical scenario generator
    return QuantLib::ext::make_shared<HistoricalScenarioGenerator>(scenarioLoader, scenarioFactory, calendar, adjFactors,
                                                           mporDays, overlapping, ReturnConfiguration(), "hs_");
}

QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> buildHistoricalScenarioGenerator(
    const QuantLib::ext::shared_ptr<ScenarioReader>& hsr,
    const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors, const std::set<QuantLib::Date>& dates,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
    const QuantLib::ext::shared_ptr<TodaysMarketParameters>& marketParams) {

    hsr->load(simParams, marketParams);

    auto scenarioFactory = QuantLib::ext::make_shared<SimpleScenarioFactory>();

    QuantLib::ext::shared_ptr<HistoricalScenarioLoader> scenarioLoader =
        QuantLib::ext::make_shared<HistoricalScenarioLoader>(
        hsr, dates);

    // Create the historical scenario generator
    return QuantLib::ext::make_shared<HistoricalScenarioGenerator>(scenarioLoader, scenarioFactory, 
        adjFactors, ReturnConfiguration(), "hs_");
}

} // namespace analytics
} // namespace ore
