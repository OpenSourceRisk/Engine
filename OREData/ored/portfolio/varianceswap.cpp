/*
  Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/varianceswap.hpp
  \brief variance swap representation
\ingroup tradedata
*/

#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/varianceswap.hpp>
#include <qle/instruments/varianceswap.hpp>

#include <ored/portfolio/referencedata.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void VarSwap::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    Currency ccy = ore::data::parseCurrency(currency_);
    Position::Type longShort = ore::data::parsePositionType(longShort_);
    start_ = ore::data::parseDate(startDate_);
    Date endDate = ore::data::parseDate(endDate_);
    cal_ = ore::data::parseCalendar(calendar_);
    MomentType momentType = parseMomentType(momentType_);

    // ISDA taxonomy
    if (assetClassUnderlying_ == AssetClass::FX) {
        additionalData_["isdaAssetClass"] = string("Foreign Exchange");
        additionalData_["isdaBaseProduct"] = string("Simple Exotic");
        additionalData_["isdaSubProduct"] = string("Vol/Var");
    } else if (assetClassUnderlying_ == AssetClass::EQ) {
        additionalData_["isdaAssetClass"] = string("Equity");
        additionalData_["isdaBaseProduct"] = string("Swap");
        if (momentType == MomentType::Variance)
            additionalData_["isdaSubProduct"] = string("Parameter Return Variance");
        else
            additionalData_["isdaSubProduct"] = string("Parameter Return Volatility");
    } else if (assetClassUnderlying_ == AssetClass::COM) {
        // guessing, that we should treat Commodities as Equities
        additionalData_["isdaAssetClass"] = string("Commodity");
        additionalData_["isdaBaseProduct"] = string("Swap");
        MomentType momentType = parseMomentType(momentType_);
        if (momentType == MomentType::Variance)
            additionalData_["isdaSubProduct"] = string("Parameter Return Variance");
        else
            additionalData_["isdaSubProduct"] = string("Parameter Return Volatility");
    } else {
        WLOG("ISDA taxonomy not set for trade " << id());
    }
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    if (cal_.empty()) {
        cal_ = ore::data::parseCalendar(ccy.code());
    }

    QL_REQUIRE(strike_ > 0 && !close_enough(strike_, 0.0),
               "VarSwap::build() strike must be positive (" << strike_ << ")");
    QL_REQUIRE(notional_ > 0 || close_enough(notional_, 0.0),
               "VarSwap::build() notional must be non-negative (" << notional_ << ")");

    // Input strike is annualised Vol
    // The quantlib strike and notional are in terms of variance, not volatility, so we convert here.
    Real varianceStrike = strike_ * strike_;
    Real varianceNotional = notional_ / (2 * 100 * strike_);

    QuantLib::ext::shared_ptr<QuantExt::VarianceSwap2> varSwap(new QuantExt::VarianceSwap2(
        longShort, varianceStrike, momentType == MomentType::Variance ? varianceNotional : notional_, start_, endDate,
        cal_, addPastDividends_));

    // Pricing Engine
    QuantLib::ext::shared_ptr<ore::data::EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<VarSwapEngineBuilder> varSwapBuilder = QuantLib::ext::dynamic_pointer_cast<VarSwapEngineBuilder>(builder);

    varSwap->setPricingEngine(varSwapBuilder->engine(name(), ccy, assetClassUnderlying_, momentType));
    setSensitivityTemplate(*varSwapBuilder);

    // set up other Trade details
    instrument_ = QuantLib::ext::shared_ptr<ore::data::InstrumentWrapper>(new ore::data::VanillaInstrument(varSwap));

    npvCurrency_ = currency_;
    notionalCurrency_ = currency_;
    maturity_ = endDate;

    // add required fixings
    for (Date d = cal_.advance(start_, -1 * Days); d <= endDate; d = cal_.advance(d, 1 * Days)) {
        requiredFixings_.addFixingDate(d, indexName_, varSwap->maturityDate());
    }
}

QuantLib::Real VarSwap::notional() const {
    MomentType momentType = parseMomentType(momentType_);
    if (momentType == MomentType::Variance) {
        Real varianceNotional = notional_ / (2 * 100 * strike_);
        return varianceNotional * 10000;
    } else
        return notional_ * 100;
}

void VarSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* vNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    if (!vNode) {
        vNode = XMLUtils::getChildNode(node, "VarianceSwapData");
        oldXml_ = true;
    }
    startDate_ = XMLUtils::getChildValue(vNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(vNode, "EndDate", true);
    currency_ = XMLUtils::getChildValue(vNode, "Currency", true);

    XMLNode* tmp = XMLUtils::getChildNode(vNode, "Underlying");
    if (!tmp) {
        tmp = XMLUtils::getChildNode(vNode, "Name");
        QL_REQUIRE(tmp, "Must provide a valid Underlying or Name node");
    }
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    longShort_ = XMLUtils::getChildValue(vNode, "LongShort", true);
    strike_ = XMLUtils::getChildValueAsDouble(vNode, "Strike", true);
    notional_ = XMLUtils::getChildValueAsDouble(vNode, "Notional", true);
    calendar_ = XMLUtils::getChildValue(vNode, "Calendar", true);
    momentType_ = XMLUtils::getChildValue(vNode, "MomentType", false);
    if (momentType_ == "")
        momentType_ = "Variance";
    string addPastDividendsStr = XMLUtils::getChildValue(vNode, "AddPastDividends", false);
    if (addPastDividendsStr == "")
        addPastDividends_ = false;
    else
        addPastDividends_ = parseBool(addPastDividendsStr);
    initIndexName();
}

XMLNode* VarSwap::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* vNode;
    if (oldXml_) {
        vNode = doc.allocNode("VarianceSwapData");
    } else {
        vNode = doc.allocNode(tradeType() + "Data");
    }
    XMLUtils::appendNode(node, vNode);
    XMLUtils::addChild(doc, vNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, vNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, vNode, "Currency", currency_);
    XMLUtils::appendNode(vNode, underlying_->toXML(doc));
    XMLUtils::addChild(doc, vNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, vNode, "Strike", strike_);
    XMLUtils::addChild(doc, vNode, "Notional", notional_);
    XMLUtils::addChild(doc, vNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, vNode, "MomentType", momentType_);
    XMLUtils::addChild(doc, vNode, "AddPastDividends", addPastDividends_);
    return node;
}

void VarSwap::initIndexName() {
    if (assetClassUnderlying_ == AssetClass::FX)
        indexName_ = "FX-" + name();
    else if (assetClassUnderlying_ == AssetClass::EQ)
        indexName_ = "EQ-" + name();
    else if (assetClassUnderlying_ == AssetClass::COM)
        indexName_ = "COM-" + name();
    else {
        QL_FAIL("asset class " << assetClassUnderlying_ << " not supported.");
    }
}

std::map<AssetClass, std::set<std::string>>
EqVarSwap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({name()})}};
}

std::map<AssetClass, std::set<std::string>>
ComVarSwap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::COM, std::set<std::string>({name()})}};
}

} // namespace data
} // namespace ore
