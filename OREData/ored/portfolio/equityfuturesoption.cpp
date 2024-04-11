/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

EquityFutureOption::EquityFutureOption(Envelope& env, OptionData option, const string& currency, Real quantity,
                                       const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying, TradeStrike strike,
                                       QuantLib::Date forwardDate, const QuantLib::ext::shared_ptr<QuantLib::Index>& index,
                                       const std::string& indexName)
    : VanillaOptionTrade(env, AssetClass::EQ, option, underlying->name(), currency, quantity, strike, index, indexName,
                         forwardDate),
      underlying_(underlying) {
    tradeType_ = "EquityFutureOption";
}

void EquityFutureOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    QL_REQUIRE(quantity_ > 0, "Equity futures option requires a positive quantity");
    assetName_ = name();
    // FIXME: use index once implemented
    // const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    // Handle<PriceTermStructure> priceCurve =
    //    market->equityPriceCurve(underlying_->name(), engineFactory->configuration(MarketContext::pricing));
    // index_ = QuantLib::ext::make_shared<EquityFuturesIndex>(underlying_->name(), forwardDate, NullCalendar(), priceCurve);

    // FIXME: we set the automatic exercise to false until the Equity Futures Index is implemented

    QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
    QL_REQUIRE(exerciseType == Exercise::European, "only european option currently supported");
    option_.setAutomaticExercise(false);
    VanillaOptionTrade::build(engineFactory);
}

void EquityFutureOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityFutureOptionData");
    QL_REQUIRE(eqNode, "No EquityFutureOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);

    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    UnderlyingBuilder underlyingBuilder("Underlying", "Name");
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    strike_.fromXML(eqNode);

    forwardDate_ = parseDate(XMLUtils::getChildValue(eqNode, "FutureExpiryDate", true));
}

XMLNode* EquityFutureOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityFutureOptionData");
    XMLUtils::appendNode(node, eqNode);
    XMLUtils::appendNode(eqNode, option_.toXML(doc));

    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    XMLUtils::appendNode(eqNode, underlying_->toXML(doc));

    XMLUtils::appendNode(eqNode, strike_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "FutureExpiryDate", to_string(forwardDate_));

    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityFutureOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({name()})}};
}

} // namespace data
} // namespace ore
