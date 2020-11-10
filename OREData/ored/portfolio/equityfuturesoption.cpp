/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
 */

#include <boost/make_shared.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

EquityFutureOption::EquityFutureOption(Envelope& env, OptionData option, const string& currency, Real quantity, 
        const boost::shared_ptr<ore::data::Underlying>& underlying, Real strike, Date futureExpiryDate)
    : VanillaOptionTrade(env, AssetClass::EQ, option, underlying->name(), currency, strike, quantity), futureExpiryDate_(futureExpiryDate) {
    tradeType_ = "EquityFutureOption";
}

void EquityFutureOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    QL_REQUIRE(quantity_ > 0, "Commodity option requires a positive quatity");
    assetName_ = name();
    
    // If we are given an explicit future contract expiry date, use it, otherwise use option's expiry.
    // The expiryDate will feed into the Futures Index
    Date expiryDate;
    if (futureExpiryDate_ != Date()) {
        expiryDate = futureExpiryDate_;
    } else {
        // Get the expiry date of the option. This is the expiry date of the commodity future index.
        const vector<string>& expiryDates = option_.exerciseDates();
        QL_REQUIRE(expiryDates.size() == 1,
                   "Expected exactly one expiry date for EquityFutureOption but got " << expiryDates.size() << ".");
        expiryDate = parseDate(expiryDates[0]);
    }

    // FIXME: use index once implemented
    // const boost::shared_ptr<Market>& market = engineFactory->market();
    // Handle<PriceTermStructure> priceCurve =
    //    market->equityPriceCurve(underlying_->name(), engineFactory->configuration(MarketContext::pricing));
    // index_ = boost::make_shared<EquityFuturesIndex>(underlying_->name(), expiryDate, NullCalendar(), priceCurve);

    // FIXME: we set the automatic exercise to false until the Equity Futures Index is implemented
    option_.setAutomaticExercise(false);
    VanillaOptionTrade::build(engineFactory);
}

void EquityFutureOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityFutureOptionData");
    QL_REQUIRE(eqNode, "No EquityFutureOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    LOG("read in " << currency_);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);

    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    UnderlyingBuilder underlyingBuilder("Underlying", "Name");
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    strike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);

    futureExpiryDate_ = Date();
    if (XMLNode* n = XMLUtils::getChildNode(eqNode, "FutureExpiryDate"))
        futureExpiryDate_ = parseDate(XMLUtils::getNodeValue(n));
}

XMLNode* EquityFutureOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityFutureOptionData");
    XMLUtils::appendNode(node, eqNode);
    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);
    
    XMLUtils::appendNode(eqNode, underlying_->toXML(doc));
    
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    if (futureExpiryDate_ != Date())
        XMLUtils::addChild(doc, eqNode, "FutureExpiryDate", to_string(futureExpiryDate_));

    return node;
}

std::map<AssetClass, std::set<std::string>> EquityFutureOption::underlyingIndices() const {
    return {{AssetClass::EQ, std::set<std::string>({name()})}};
}

} // namespace data
} // namespace ore
