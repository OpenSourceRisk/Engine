/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingengine.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>

#include <boost/algorithm/string/replace.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std::placeholders;

namespace ore {
namespace data {

void ScriptedTrade::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const PremiumData& premiumData,
                          const Real premiumMultiplier) {

    DLOG("ScriptedTrade::build() called for trade " << id());

    auto builder = QuantLib::ext::dynamic_pointer_cast<ScriptedTradeEngineBuilder>(engineFactory->builder("ScriptedTrade"));

    QL_REQUIRE(builder, "no builder found for ScriptedTrade");
    auto engine = builder->engine(id(), *this, engineFactory->referenceData(), engineFactory->iborFallbackConfig());

    simmProductClass_ = builder->simmProductClass();
    scheduleProductClass_ = builder->scheduleProductClass();

    setIsdaTaxonomyFields();

    auto qleInstr = QuantLib::ext::make_shared<ScriptedInstrument>(builder->lastRelevantDate());
    qleInstr->setPricingEngine(engine);

    npvCurrency_ = builder->npvCurrency();
    maturity_ = builder->lastRelevantDate();
    notional_ = Null<Real>(); // is handled by override of notional()
    notionalCurrency_ = "";   // is handled by override of notionalCurrency()
    legs_.clear();
    legCurrencies_.clear();
    legPayers_.clear();

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, 1.0, premiumData,
                                                premiumMultiplier, parseCurrencyWithMinors(npvCurrency_), engineFactory,
                                                builder->configuration(MarketContext::pricing)));

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qleInstr, 1.0, additionalInstruments, additionalMultipliers);
    
    // add required fixings
    for (auto const& f : builder->fixings()) {
        for (auto const& d : f.second) {
            IndexInfo info(f.first);
            if (info.isInf()) {
                QL_DEPRECATED_DISABLE_WARNING
                requiredFixings_.addZeroInflationFixingDate(d, info.infName(), info.inf()->interpolated(),
                                                            info.inf()->frequency(), info.inf()->availabilityLag(),
                                                            CPI::AsIndex, info.inf()->frequency(), Date::maxDate(), false, false);
                QL_DEPRECATED_ENABLE_WARNING
            } else if (info.isFx()) {
                // for FX we do not know if FX-TAG-CCY1-CCY2 or FX-TAG-CCY2-CCY1 is in the history, require both
                requiredFixings_.addFixingDate(d, f.first, Date::maxDate(), false, false);
                requiredFixings_.addFixingDate(d, inverseFxIndex(f.first), Date::maxDate(), false, false);
            } else {
                requiredFixings_.addFixingDate(d, f.first, Date::maxDate(), false, false);
            }
        }
    }

    // set sensitivity template
    setSensitivityTemplate(builder->sensitivityTemplate());
}

void ScriptedTrade::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    ScriptedTrade::build(engineFactory, PremiumData(), 1.0);
}

void ScriptedTrade::setIsdaTaxonomyFields() {
    // ISDA taxonomy, can be overwritten in derived classes
    if (scheduleProductClass_ == "FX") {
        additionalData_["isdaAssetClass"] = string("Foreign Exchange");
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
        additionalData_["isdaTransaction"] = string("");
    } else if (scheduleProductClass_ == "Rates") {
        additionalData_["isdaAssetClass"] = string("Interest Rate");
        additionalData_["isdaBaseProduct"] = string("Exotic");
        additionalData_["isdaSubProduct"] = string("");
        additionalData_["isdaTransaction"] = string("");
    } else if (scheduleProductClass_ == "Equity") {
        additionalData_["isdaAssetClass"] = string("Equity");
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("");
        additionalData_["isdaTransaction"] = string("");
    } else if (scheduleProductClass_ == "Credit") {
        additionalData_["isdaAssetClass"] = string("Credit");
        additionalData_["isdaBaseProduct"] = string("Exotic");
        additionalData_["isdaSubProduct"] = string("Other");
        additionalData_["isdaTransaction"] = string("");
    } else if (scheduleProductClass_ == "Commodity") {
        DLOG("ISDA taxonomy for trade " << id() << " and product class " << scheduleProductClass_
                                        << " follows the Equity template");
        additionalData_["isdaAssetClass"] = string("Commodity");
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("");
        additionalData_["isdaTransaction"] = string("");
    } else {
        DLOG("ISDA taxonomy not set for trade " << id() << " and product class " << scheduleProductClass_);
    }
}

QuantLib::Real ScriptedTrade::notional() const {
    if (instrument_->qlInstrument()->isExpired())
        return 0.0;
    // try to get the notional from the additional results of the instrument
    auto st = QuantLib::ext::dynamic_pointer_cast<ScriptedInstrument>(instrument_->qlInstrument(true));
    QL_REQUIRE(st, "internal error: could not cast to ScriptedInstrument");
    try {
        return st->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        if (st->lastCalculationWasValid()) {
            // calculation was valid, just the result is not provided
            DLOG("notional was not retrieved: " << e.what() << ", return null");
        } else {
            // calculation threw an error, propagate this
            QL_FAIL(e.what());
        }
    }
    // if not provided, return null
    return Null<Real>();
}

std::string ScriptedTrade::notionalCurrency() const {
    if (instrument_->qlInstrument()->isExpired())
        return npvCurrency_;
    // try to get the notional ccy from the additional results of the instrument
    auto st = QuantLib::ext::dynamic_pointer_cast<ScriptedInstrument>(instrument_->qlInstrument(true));
    QL_REQUIRE(st, "internal error: could not cast to ScriptedInstrument");
    try {
        return instrument_->qlInstrument()->result<std::string>("notionalCurrency");
    } catch (const std::exception& e) {
        if (st->lastCalculationWasValid()) {
            // calculation was valid, just the result is not provided
            DLOG("notional ccy was not retrieved: " << e.what() << ", return empty string");
        } else {
            // calculation threw an error, propagate this
            QL_FAIL(e.what());
        }
    }
    // if not provided, return an empty string
    return std::string();
}

void ScriptedTrade::clear() {
    events_.clear();
    numbers_.clear();
    indices_.clear();
    currencies_.clear();
    daycounters_.clear();
    scriptName_ = productTag_ = "";
    script_.clear();
}

namespace {

// some helper functions for freestyle parsing

enum class NodeType { Event, Number, Index, Currency, Daycounter };

std::pair<NodeType, std::string> getNativeTypeAndValue(const std::string& value, const std::string& type) {

    static std::map<std::string, std::map<std::string, std::string>> mappings = {
        {"bool", {{"true", "1"}, {"false", "-1"}}},
        {"optionType", {{"Call", "1"}, {"Put", "-1"}, {"Cap", "1"}, {"Floor", "-1"}}},
        {"longShort", {{"Long", "1"}, {"Short", "-1"}}},
        {"barrierType", {{"DownIn", "1"}, {"UpIn", "2"}, {"DownOut", "3"}, {"UpOut", "4"}}},
    };

    // native types
    if (type == "event")
        return std::make_pair(NodeType::Event, value);
    else if (type == "number")
        return std::make_pair(NodeType::Number, value);
    else if (type == "index")
        return std::make_pair(NodeType::Index, value);
    else if (type == "currency")
        return std::make_pair(NodeType::Currency, value);
    else if (type == "dayCounter")
        return std::make_pair(NodeType::Daycounter, value);
    else {
        // type that maps to number
        auto t = mappings.find(type);
        QL_REQUIRE(t != mappings.end(), "type '" << type << "' not known");
        auto v = t->second.find(value);
        QL_REQUIRE(v != t->second.end(), "value '" << value << "' for type '" << type << "' not known");
        return std::make_pair(NodeType::Number, v->second);
    }
}

} // namespace

std::map<ore::data::AssetClass, std::set<std::string>>
ScriptedTrade::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

    map<ore::data::AssetClass, set<string>> result;

    for (const auto& p : indices_) {
        vector<string> vals;
        if (!p.value().empty())
            vals.push_back(p.value());
        else
            vals = p.values();

        for (auto v : vals) {
            if (!v.empty()) {
                IndexInfo ind(v);
                if (ind.isComm()) {
                    result[ore::data::AssetClass::COM].insert(ind.commName());
                } else if (ind.isEq()) {
                    result[ore::data::AssetClass::EQ].insert(ind.eq()->name());
                }
            }
        }
    }

    return result;
}

void ScriptedTrade::fromXML(XMLNode* node) {
    clear();
    Trade::fromXML(node);

    // if we find a ScriptedTradeData node we use this (native format)

    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, "ScriptedTradeData");
    if (tradeDataNode) {
        XMLNode* dataNode = XMLUtils::getChildNode(tradeDataNode, "Data");
        QL_REQUIRE(dataNode, "ScriptedTradeData/Data node not found");
        vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(dataNode, "");
        for (auto const n : nodes) {
            if (XMLUtils::getNodeName(n) == "Event") {
                events_.push_back(ScriptedTradeEventData());
                events_.back().fromXML(n);
            } else if (XMLUtils::getNodeName(n) == "Number") {
                numbers_.push_back(ScriptedTradeValueTypeData("Number"));
                numbers_.back().fromXML(n);
            } else if (XMLUtils::getNodeName(n) == "Index") {
                indices_.push_back(ScriptedTradeValueTypeData("Index"));
                indices_.back().fromXML(n);
            } else if (XMLUtils::getNodeName(n) == "Currency") {
                currencies_.push_back(ScriptedTradeValueTypeData("Currency"));
                currencies_.back().fromXML(n);
            } else if (XMLUtils::getNodeName(n) == "Daycounter") {
                daycounters_.push_back(ScriptedTradeValueTypeData("Daycounter"));
                daycounters_.back().fromXML(n);
            }
        }
        if (XMLNode* scriptName = XMLUtils::getChildNode(tradeDataNode, "ScriptName")) {
            scriptName_ = XMLUtils::getNodeValue(scriptName);
        } else if (XMLUtils::getChildNode(tradeDataNode, "Script")) {
            productTag_ = XMLUtils::getChildValue(tradeDataNode, "ProductTag", false);
            auto sn = XMLUtils::getChildrenNodes(tradeDataNode, "Script");
            for (auto const& n : sn) {
                ScriptedTradeScriptData s;
                s.fromXML(n);
                std::string purpose = XMLUtils::getAttribute(n, "purpose");
                script_[purpose] = s;
            }
        } else {
            QL_FAIL("either Script or ScriptName expected");
        }
        return;
    }

    // otherwise we look for a xxxData node and interpret the xxx as the script name (freestyle parsing)

    for (XMLNode* child = XMLUtils::getChildNode(node); child; child = XMLUtils::getNextSibling(child)) {
        std::string name = XMLUtils::getNodeName(child);
        if (name.size() > 4 && name.substr(name.size() - 4, 4) == "Data") {
            QL_REQUIRE(!tradeDataNode, "multiple child nodes xxxData found");
            tradeDataNode = child;
            scriptName_ = name.substr(0, name.size() - 4);
        }
    }

    QL_REQUIRE(tradeDataNode, "expected ScriptedTradeData or xxxData node");

    // now loop over the child nodes and populate the script data
    for (XMLNode* child = XMLUtils::getChildNode(tradeDataNode); child; child = XMLUtils::getNextSibling(child)) {

        // the name of the node will be the name of the script variable
        std::string varName = XMLUtils::getNodeName(child);
        std::string type = XMLUtils::getAttribute(child, "type");
        QL_REQUIRE(!type.empty(), "no type given for node '" << varName << "'");

        std::string scalarValue = XMLUtils::getNodeValue(child);
        if (!scalarValue.empty()) {
            // if we have a value, this is a scalar
            auto native = getNativeTypeAndValue(scalarValue, type);
            if (native.first == NodeType::Event) {
                events_.push_back(ScriptedTradeEventData(varName, native.second));
            } else if (native.first == NodeType::Number) {
                numbers_.push_back(ScriptedTradeValueTypeData("Number", varName, native.second));
            } else if (native.first == NodeType::Index) {
                indices_.push_back(ScriptedTradeValueTypeData("Index", varName, native.second));
            } else if (native.first == NodeType::Currency) {
                currencies_.push_back(ScriptedTradeValueTypeData("Currency", varName, native.second));
            } else if (native.first == NodeType::Daycounter) {
                daycounters_.push_back(ScriptedTradeValueTypeData("Daycounter", varName, native.second));
            } else {
                QL_FAIL("unexpected node type");
            }
        } else {
            // if we don't have a value, this is a vector given by some sub node
            if (XMLNode* v = XMLUtils::getChildNode(child, "ScheduleData")) {
                ScheduleData sched;
                sched.fromXML(v);
                events_.push_back(ScriptedTradeEventData(varName, sched));
            } else if (XMLNode* v = XMLUtils::getChildNode(child, "DerivedSchedule")) {
                events_.push_back(ScriptedTradeEventData(varName, XMLUtils::getChildValue(v, "BaseSchedule", true),
                                                         XMLUtils::getChildValue(v, "Shift", true),
                                                         XMLUtils::getChildValue(v, "Calendar", true),
                                                         XMLUtils::getChildValue(v, "Convention", true)));
            } else if (XMLNode* v = XMLUtils::getChildNode(child, "Value")) {
                auto native = getNativeTypeAndValue(XMLUtils::getNodeValue(v), type);
                QL_REQUIRE(native.first != NodeType::Event, "unexpected even array under node '" << varName << "'");
                std::vector<std::string> arrayValues;
                for (XMLNode* val = XMLUtils::getChildNode(child, "Value"); val;
                     val = XMLUtils::getNextSibling(val, "Value")) {
                    auto v = getNativeTypeAndValue(XMLUtils::getNodeValue(val), type);
                    arrayValues.push_back(v.second);
                }
                if (native.first == NodeType::Number) {
                    numbers_.push_back(ScriptedTradeValueTypeData("Number", varName, arrayValues));
                } else if (native.first == NodeType::Index) {
                    indices_.push_back(ScriptedTradeValueTypeData("Index", varName, arrayValues));
                } else if (native.first == NodeType::Currency) {
                    currencies_.push_back(ScriptedTradeValueTypeData("Currency", varName, arrayValues));
                } else if (native.first == NodeType::Daycounter) {
                    daycounters_.push_back(ScriptedTradeValueTypeData("Daycounter", varName, arrayValues));
                } else {
                    QL_FAIL("unexpected node type");
                }
            } else {
                QL_FAIL("unexpected content under node '" << varName << "'");
            }
        }
    }
}

XMLNode* ScriptedTrade::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeDataNode = doc.allocNode("ScriptedTradeData");
    XMLUtils::appendNode(node, tradeDataNode);
    if (!scriptName_.empty())
        XMLUtils::addChild(doc, tradeDataNode, "ScriptName", scriptName_);
    else {
        XMLUtils::addChild(doc, tradeDataNode, "ProductTag", productTag_);
        for (auto& s : script_) {
            XMLNode* n = s.second.toXML(doc);
            XMLUtils::addAttribute(doc, n, "purpose", s.first);
            XMLUtils::appendNode(tradeDataNode, n);
        }
    }
    XMLNode* dataNode = doc.allocNode("Data");
    XMLUtils::appendNode(tradeDataNode, dataNode);
    for (auto& x : events_)
        XMLUtils::appendNode(dataNode, x.toXML(doc));
    for (auto& x : numbers_)
        XMLUtils::appendNode(dataNode, x.toXML(doc));
    for (auto& x : indices_)
        XMLUtils::appendNode(dataNode, x.toXML(doc));
    for (auto& x : currencies_)
        XMLUtils::appendNode(dataNode, x.toXML(doc));
    for (auto& x : daycounters_)
        XMLUtils::appendNode(dataNode, x.toXML(doc));
    return node;
}

const ScriptedTradeScriptData& ScriptedTrade::script(const std::string& purpose,
                                                     const bool fallBackOnEmptyPurpose) const {
    auto s1 = script_.find(purpose);
    if (s1 != script_.end())
        return s1->second;
    if (fallBackOnEmptyPurpose) {
        auto s2 = script_.find("");
        if (s2 != script_.end())
            return s2->second;
    }
    QL_FAIL("ScriptedTrade::script(): script with purpose '"
            << purpose << "' not found, fall back on empty purpose was " << std::boolalpha << fallBackOnEmptyPurpose);
}

void ScriptedTradeEventData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Event");
    name_ = XMLUtils::getChildValue(node, "Name", true);
    if (XMLNode* v = XMLUtils::getChildNode(node, "Value")) {
        type_ = Type::Value;
        value_ = XMLUtils::getNodeValue(v);
    } else if (XMLNode* v = XMLUtils::getChildNode(node, "ScheduleData")) {
        type_ = Type::Array;
        schedule_.fromXML(v);
    } else if (XMLNode* v = XMLUtils::getChildNode(node, "DerivedSchedule")) {
        type_ = Type::Derived;
        baseSchedule_ = XMLUtils::getChildValue(v, "BaseSchedule", true);
        shift_ = XMLUtils::getChildValue(v, "Shift", true);
        calendar_ = XMLUtils::getChildValue(v, "Calendar", true);
        convention_ = XMLUtils::getChildValue(v, "Convention", true);
    } else {
        QL_FAIL("Expected Value or ScheduleData node");
    }
}

XMLNode* ScriptedTradeEventData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Event");
    XMLUtils::addChild(doc, n, "Name", name_);
    if (type_ == Type::Value) {
        XMLUtils::addChild(doc, n, "Value", value_);
    } else if (type_ == Type::Array) {
        XMLUtils::appendNode(n, schedule_.toXML(doc));
    } else if (type_ == Type::Derived) {
        XMLNode* d = doc.allocNode("DerivedSchedule");
        XMLUtils::addChild(doc, d, "BaseSchedule", baseSchedule_);
        XMLUtils::addChild(doc, d, "Shift", shift_);
        XMLUtils::addChild(doc, d, "Calendar", calendar_);
        XMLUtils::addChild(doc, d, "Convention", convention_);
        XMLUtils::appendNode(n, d);
    } else {
        QL_FAIL("ScriptedTradeEventData::toXML(): unexpected ScriptedTradeEventData::Type");
    }
    return n;
}

const bool ScriptedTradeEventData::hasData() {
    if (type_ == Type::Array) {
        return schedule_.hasData();
    } else if (type_ == Type::Derived) {
        return !baseSchedule_.empty() && !shift_.empty() && !calendar_.empty() && !convention_.empty();
    } else if (type_ == Type::Value) {
        return !value_.empty();
    } else {
        // If type_ is not defined, then the ScriptedTradeEventData object is not defined
        return false;
    }
}

void ScriptedTradeValueTypeData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, nodeName_);
    name_ = XMLUtils::getChildValue(node, "Name", true);
    if (XMLNode* v = XMLUtils::getChildNode(node, "Value")) {
        isArray_ = false;
        value_ = XMLUtils::getNodeValue(v);
    } else if (XMLUtils::getChildNode(node, "Values")) {
        isArray_ = true;
        values_ = XMLUtils::getChildrenValues(node, "Values", "Value");
    } else {
        QL_FAIL("Expected Value or Values node");
    }
}

XMLNode* ScriptedTradeValueTypeData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode(nodeName_);
    XMLUtils::addChild(doc, n, "Name", name_);
    if (!isArray_) {
        XMLUtils::addChild(doc, n, "Value", value_);
    } else {
        XMLUtils::addChildren(doc, n, "Values", "Value", values_);
    }
    return n;
}

void ScriptedTradeScriptData::NewScheduleData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NewSchedule");
    name_ = XMLUtils::getChildValue(node, "Name", true);
    operation_ = XMLUtils::getChildValue(node, "Operation", true);
    sourceSchedules_ = XMLUtils::getChildrenValues(node, "Schedules", "Schedule");
}

XMLNode* ScriptedTradeScriptData::NewScheduleData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("NewSchedule");
    XMLUtils::addChild(doc, n, "Name", name_);
    XMLUtils::addChild(doc, n, "Operation", operation_);
    XMLUtils::addChildren(doc, n, "Schedules", "Schedule", sourceSchedules_);
    return n;
}

void ScriptedTradeScriptData::CalibrationData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Calibration");
    index_ = XMLUtils::getChildValue(node, "Index", true);
    strikes_ = XMLUtils::getChildrenValues(node, "Strikes", "Strike", true);
}

XMLNode* ScriptedTradeScriptData::CalibrationData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Calibration");
    XMLUtils::addChild(doc, n, "Index", index_);
    XMLUtils::addChildren(doc, n, "Strikes", "Strike", strikes_);
    return n;
}

void ScriptedTradeScriptData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Script");
    code_ = XMLUtils::getChildValue(node, "Code", true);
    formatCode();
    npv_ = XMLUtils::getChildValue(node, "NPV", true);
    std::vector<std::string> attributes;
    std::vector<std::string> values =
        XMLUtils::getChildrenValuesWithAttributes(node, "Results", "Result", "rename", attributes);
    for (Size i = 0; i < values.size(); ++i) {
        // result name is identical to script variable expect the rename attribute is filled
        results_.push_back(std::make_pair(attributes[i].empty() ? values[i] : attributes[i], values[i]));
    }
    schedulesEligibleForCoarsening_ = XMLUtils::getChildrenValues(node, "ScheduleCoarsening", "EligibleSchedule");
    if (XMLNode* ns = XMLUtils::getChildNode(node, "NewSchedules")) {
        std::vector<XMLNode*> newSchedules = XMLUtils::getChildrenNodes(ns, "NewSchedule");
        for (auto const& n : newSchedules) {
            NewScheduleData tmp;
            tmp.fromXML(n);
            newSchedules_.push_back(tmp);
        }
    }
    if (XMLNode* ns = XMLUtils::getChildNode(node, "CalibrationSpec")) {
        std::vector<XMLNode*> calibrations = XMLUtils::getChildrenNodes(ns, "Calibration");
        for (auto const& n : calibrations) {
            CalibrationData tmp;
            tmp.fromXML(n);
            calibrationSpec_.push_back(tmp);
        }
    }
    stickyCloseOutStates_ = XMLUtils::getChildrenValues(node, "StickyCloseOutStates", "StickyCloseOutState");
    if (XMLNode* ns = XMLUtils::getChildNode(node, "ConditionalExpectation")) {
        conditionalExpectationModelStates_ = XMLUtils::getChildrenValues(ns, "ModelStates", "ModelState", false);
    }
}

XMLNode* ScriptedTradeScriptData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Script");
    XMLUtils::addChildAsCdata(doc, n, "Code", code_);
    XMLUtils::addChild(doc, n, "NPV", npv_);
    std::vector<std::string> values, attributes;
    for (auto const& r : results_) {
        attributes.push_back(r.first);
        values.push_back(r.second);
    }
    XMLUtils::addChildrenWithAttributes(doc, n, "Results", "Result", values, "rename", attributes);
    XMLUtils::addChildren(doc, n, "ScheduleCoarsening", "EligibleSchedule", schedulesEligibleForCoarsening_);
    XMLNode* newSchedules = doc.allocNode("NewSchedules");
    XMLUtils::appendNode(n, newSchedules);
    for (auto& s : newSchedules_) {
        XMLUtils::appendNode(newSchedules, s.toXML(doc));
    }
    XMLNode* calibrations = doc.allocNode("CalibrationSpec");
    XMLUtils::appendNode(n, calibrations);
    for (auto& c : calibrationSpec_) {
        XMLUtils::appendNode(calibrations, c.toXML(doc));
    }
    XMLUtils::addChildren(doc, n, "StickyCloseOutStates", "StickyCloseOutState", stickyCloseOutStates_);
    XMLNode* condExp = doc.allocNode("ConditionalExpectation");
    XMLUtils::appendNode(n, condExp);
    XMLUtils::addChildren(doc, condExp, "ModelStates", "ModelState", conditionalExpectationModelStates_);
    return n;
}

void ScriptedTradeScriptData::formatCode() {
    // remove carriage returns (e.g. DOS uses \r\n as a line ending, but we want \n only)
    boost::replace_all(code_, "\r", "");
    // untabify
    boost::replace_all(code_, "\t", "    ");
}

ScriptLibraryData::ScriptLibraryData(const ScriptLibraryData& d) { scripts_ = d.scripts_; }

ScriptLibraryData::ScriptLibraryData(ScriptLibraryData&& d) { scripts_ = std::move(d.scripts_); }

ScriptLibraryData& ScriptLibraryData::operator=(const ScriptLibraryData& d) {
    scripts_ = d.scripts_;
    return *this;
}

ScriptLibraryData& ScriptLibraryData::operator=(ScriptLibraryData&& d) {
    scripts_ = std::move(d.scripts_);
    return *this;
}

bool ScriptLibraryData::has(const std::string& scriptName, const std::string& purpose,
                            const bool fallBackOnEmptyPurpose) const {
    auto s = scripts_.find(scriptName);
    if (s != scripts_.end()) {
        return s->second.second.find(purpose) != s->second.second.end() ||
               (fallBackOnEmptyPurpose && has(scriptName, "", false));
    }
    return false;
}

std::pair<std::string, ScriptedTradeScriptData> ScriptLibraryData::get(const std::string& scriptName,
                                                                       const std::string& purpose,
                                                                       const bool fallBackOnEmptyPurpose) const {
    auto s = scripts_.find(scriptName);
    if (s != scripts_.end()) {
        auto f = s->second.second.find(purpose);
        if (f != s->second.second.end()) {
            return std::make_pair(s->second.first, f->second);
        } else if (fallBackOnEmptyPurpose) {
            auto f = s->second.second.find("");
            if (f != s->second.second.end())
                return std::make_pair(s->second.first, f->second);
        }
    }
    QL_FAIL("ScriptedTradeScriptData::get(): script '" << scriptName << "' with purpose '" << purpose
                                                       << "' not found, fallBackOnEmptyPurpose was " << std::boolalpha
                                                       << fallBackOnEmptyPurpose);
}

void ScriptLibraryData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ScriptLibrary");
    auto childs = XMLUtils::getChildrenNodes(node, "Script");
    std::set<std::string> loadedNames;
    for (auto c : childs) {
        std::string name = XMLUtils::getChildValue(c, "Name");
        QL_REQUIRE(std::find(loadedNames.begin(), loadedNames.end(), name) == loadedNames.end(),
                   "duplicate script with name '" << name << "'");
        loadedNames.insert(name);
        std::string productTag = XMLUtils::getChildValue(c, "ProductTag", false);
        scripts_[name].first = productTag;
        auto sn = XMLUtils::getChildrenNodes(c, "Script");
        QL_REQUIRE(!sn.empty(), "no node Script found for '" << name << "'");
        for (auto& n : sn) {
            ScriptedTradeScriptData d;
            d.fromXML(n);
            std::string purpose = XMLUtils::getAttribute(n, "purpose");
            scripts_[name].second[purpose] = d;
            TLOG("loaded script '" << name << "' (purpose='" << purpose << "', productTag='" << productTag << "')");
        }
    }
}

XMLNode* ScriptLibraryData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("ScriptLibrary");
    for (auto& s : scripts_) {
        XMLNode* c = XMLUtils::addChild(doc, n, "Script");
        XMLUtils::addChild(doc, c, "Name", s.first);
        XMLUtils::addChild(doc, c, "ProductTag", s.second.first);
        for (auto& t : s.second.second) {
            XMLNode* n = t.second.toXML(doc);
            XMLUtils::addAttribute(doc, n, "purpose", t.first);
            XMLUtils::appendNode(c, n);
        }
    }
    return n;
}

const ScriptLibraryData& ScriptLibraryStorage::get() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return data_;
}

void ScriptLibraryStorage::set(const ScriptLibraryData& data) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_ = data;
}

void ScriptLibraryStorage::set(ScriptLibraryData&& data) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_ = std::move(data);
}

void ScriptLibraryStorage::clear() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_ = ScriptLibraryData();
}

} // namespace data
} // namespace ore
