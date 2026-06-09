/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/configuration/bondfuturevolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

BondFutureVolatilityConfig::BondFutureVolatilityConfig() : strikeFactor_(1.0) {}

BondFutureVolatilityConfig::BondFutureVolatilityConfig(
    const string& curveId,
    const string& curveDescription,
    string contractName,
    vector<ext::shared_ptr<VolatilityConfig>> volatilityConfig,
    string dayCounter,
    string calendar,
    string yieldCurveId,
    Real strikeFactor,
    string useOnlyPutCall,
    ext::optional<OneDimSolverConfig> solverConfig,
    ext::optional<bool> preferOutOfTheMoney,
    string engineOverride,
    ext::optional<bool> treatAsEuropean)
    : CurveConfig(curveId, curveDescription),
      contractName_(std::move(contractName)),
      volatilityConfig_(std::move(volatilityConfig)),
      dayCounter_(std::move(dayCounter)),
      calendar_(std::move(calendar)),
      yieldCurveId_(std::move(yieldCurveId)),
      strikeFactor_(strikeFactor),
      useOnlyPutCall_(std::move(useOnlyPutCall)),
      solverConfig_(std::move(solverConfig)),
      preferOutOfTheMoney_(std::move(preferOutOfTheMoney)),
      engineOverride_(std::move(engineOverride)),
      treatAsEuropean_(std::move(treatAsEuropean)) {
    populateQuotes();
}

void BondFutureVolatilityConfig::validate() const {
    QL_REQUIRE(!volatilityConfig_.empty(), "BondFutureVolatilityConfig: volatilityConfig for " << curveID() <<
        " is empty.");
    for (const auto& vc : volatilityConfig_) {
        auto vssc = ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc);
        QL_REQUIRE(vssc, "BondFutureVolatilityConfig: volatilityConfig for " << curveID() <<
            " must be of type VolatilityStrikeSurfaceConfig.");
        const auto& quoteType = vssc->quoteType();
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE_LNVOL || quoteType == MarketDatum::QuoteType::PRICE,
            "BondFutureVolatilityConfig: volatilityConfig for " << curveID() << " must have quote type "
            "RATE_LNVOL or PRICE but got " << quoteType << ".");
    }
    if (!useOnlyPutCall_.empty()) {
        QL_REQUIRE(useOnlyPutCall_ == "C" || useOnlyPutCall_ == "P", "BondFutureVolatilityConfig: useOnlyPutCall for "
            << curveID() << " if non-empty must be 'C' or 'P' but got " << useOnlyPutCall_ << ".");
    }
}

void BondFutureVolatilityConfig::populateRequiredIds() const {
    if (!yieldCurveId().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(yieldCurveId())->curveConfigID());
}

const string& BondFutureVolatilityConfig::contractName() const { return contractName_; }

const vector<ext::shared_ptr<VolatilityConfig>>& BondFutureVolatilityConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& BondFutureVolatilityConfig::dayCounter() const { return dayCounter_; }

const string& BondFutureVolatilityConfig::calendar() const { return calendar_; }

const string& BondFutureVolatilityConfig::yieldCurveId() const { return yieldCurveId_; }

Real BondFutureVolatilityConfig::strikeFactor() const { return strikeFactor_; }

const string& BondFutureVolatilityConfig::useOnlyPutCall() const { return useOnlyPutCall_; }

const ext::optional<OneDimSolverConfig>& BondFutureVolatilityConfig::solverConfig() const {
    return solverConfig_;
}

const ext::optional<bool>& BondFutureVolatilityConfig::preferOutOfTheMoney() const {
    return preferOutOfTheMoney_;
}

const string& BondFutureVolatilityConfig::engineOverride() const { return engineOverride_; }

const ext::optional<bool>& BondFutureVolatilityConfig::treatAsEuropean() const {
    return treatAsEuropean_;
}

void BondFutureVolatilityConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BondFutureVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    contractName_ = XMLUtils::getChildValue(node, "ContractName", true);

    VolatilityConfigBuilder vcb;
    vcb.fromXML(node);
    volatilityConfig_ = vcb.volatilityConfig();

    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter", false, "A365");
    calendar_ = XMLUtils::getChildValue(node, "Calendar", false, "NullCalendar");
    yieldCurveId_ = XMLUtils::getChildValue(node, "YieldCurveId", false);

    if (auto n = XMLUtils::getChildNode(node, "StrikeFactor"))
        strikeFactor_ = parseReal(XMLUtils::getNodeValue(n));

    useOnlyPutCall_ = XMLUtils::getChildValue(node, "UseOnlyPutCall", false);

    if (auto n = XMLUtils::getChildNode(node, "OneDimSolverConfig")) {
        solverConfig_ = OneDimSolverConfig();
        solverConfig_->fromXML(n);
    }

    if (auto n = XMLUtils::getChildNode(node, "PreferOutOfTheMoney"))
        preferOutOfTheMoney_ = parseBool(XMLUtils::getNodeValue(n));

    if (auto n = XMLUtils::getChildNode(node, "TreatAsEuropean"))
        treatAsEuropean_ = parseBool(XMLUtils::getNodeValue(n));

    populateQuotes();
}

XMLNode* BondFutureVolatilityConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BondFutureVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "ContractName", contractName_);

    XMLNode* volConfigsNode = doc.allocNode("VolatilityConfig");
    for (auto vc : volatilityConfig_) {
        XMLNode* volConfigNode = vc->toXML(doc);
        XMLUtils::appendNode(volConfigsNode, volConfigNode);
    }
    XMLUtils::appendNode(node, volConfigsNode);

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!yieldCurveId_.empty())
        XMLUtils::addChild(doc, node, "YieldCurveId", yieldCurveId_);
    XMLUtils::addChild(doc, node, "StrikeFactor", strikeFactor_);
    if (!useOnlyPutCall_.empty())
        XMLUtils::addChild(doc, node, "UseOnlyPutCall", useOnlyPutCall_);

    if (solverConfig_)
        XMLUtils::appendNode(node, solverConfig_->toXML(doc));

    if (preferOutOfTheMoney_)
        XMLUtils::addChild(doc, node, "PreferOutOfTheMoney", *preferOutOfTheMoney_);

    if (treatAsEuropean_)
        XMLUtils::addChild(doc, node, "TreatAsEuropean", *treatAsEuropean_);

    return node;
}

void BondFutureVolatilityConfig::populateQuotes() {

    validate();
    for (auto config : volatilityConfig_) {
        // Already checked in validate() that all configs are of this type.
        auto vssc = ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(config);
        string quoteType = to_string<MarketDatum::QuoteType>(vssc->quoteType());
        string quoteStem = "BOND_FUTURE_OPTION/" + quoteType + "/" + contractName_ + "/";
        string suffix = useOnlyPutCall_.empty() ? "" : "/" + useOnlyPutCall_;
        for (const pair<string, string>& p : vssc->quotes()) {
            if (p.first == "*" || p.second == "*") {
                quotes_.push_back(quoteStem + "*");
            } else {
                quotes_.push_back(quoteStem + p.first + "/" + p.second + suffix);
            }
        }
    }
}

} // namespace data
} // namespace ore
