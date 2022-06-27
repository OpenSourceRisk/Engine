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

#include <ored/portfolio/trade.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

void Trade::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Trade");
    tradeType_ = XMLUtils::getChildValue(node, "TradeType", true);
    if (XMLNode* envNode = XMLUtils::getChildNode(node, "Envelope")) {
        envelope_.fromXML(envNode);
    }
    tradeActions_.clear();
    XMLNode* taNode = XMLUtils::getChildNode(node, "TradeActions");
    if (taNode)
        tradeActions_.fromXML(taNode);
}

XMLNode* Trade::toXML(XMLDocument& doc) {
    // Crete Trade Node with Id attribute.
    XMLNode* node = doc.allocNode("Trade");
    QL_REQUIRE(node, "Failed to create trade node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    XMLUtils::addChild(doc, node, "TradeType", tradeType_);
    XMLUtils::appendNode(node, envelope_.toXML(doc));
    if (!tradeActions_.empty())
        XMLUtils::appendNode(node, tradeActions_.toXML(doc));
    return node;
}

Date Trade::addPremiums(std::vector<boost::shared_ptr<Instrument>>& addInstruments, std::vector<Real>& addMultipliers,
                        const Real tradeMultiplier, const PremiumData& premiumData, const Real premiumMultiplier,
                        const Currency& tradeCurrency, const boost::shared_ptr<EngineFactory>& factory,
                        const string& configuration) {

    Date latestPremiumPayDate = Date::minDate();

    for (auto const& d : premiumData.premiumData()) {
        QL_REQUIRE(d.amount != Null<Real>(), "Trade contains invalid premium data.");

        Currency premiumCurrency = parseCurrencyWithMinors(d.ccy);
        Real premiumAmount = convertMinorToMajorCurrency(d.ccy, d.amount);
        auto fee = boost::make_shared<QuantExt::Payment>(premiumAmount, premiumCurrency, d.payDate);

        addMultipliers.push_back(premiumMultiplier);

        Handle<YieldTermStructure> yts = factory->market()->discountCurve(d.ccy, configuration);
        Handle<Quote> fx;
        if (tradeCurrency.code() != d.ccy) {
            fx = factory->market()->fxRate(d.ccy + tradeCurrency.code(), configuration);
        }
        boost::shared_ptr<PricingEngine> discountingEngine(new QuantExt::PaymentDiscountingEngine(yts, fx));
        fee->setPricingEngine(discountingEngine);

        // 1) Add to additional instruments for pricing
        addInstruments.push_back(fee);

        // 2) Add a trade leg for cash flow reporting, divide the amount by the multiplier, because the leg entries
        //    are multiplied with the trade multiplier in the cashflow report (and if used elsewhere)
        legs_.push_back(
            Leg(1, boost::make_shared<SimpleCashFlow>(fee->cashFlow()->amount() * premiumMultiplier / tradeMultiplier,
                                                      fee->cashFlow()->date())));
        legCurrencies_.push_back(fee->currency().code());

        // premium * premiumMultiplier reflects the correct pay direction, set payer to false therefore
        legPayers_.push_back(false);

	// update latest premium pay date
        latestPremiumPayDate = std::max(latestPremiumPayDate, d.payDate);

        DLOG("added fee " << d.amount << " " << d.ccy << " payable on " << d.payDate << " to trade");
    }

    return latestPremiumPayDate;
}

void Trade::validate() const {
    QL_REQUIRE(id_ != "", "Trade id has not been set.");
    QL_REQUIRE(tradeType_ != "", "Trade id has not been set.");
    QL_REQUIRE(instrument_ || legs_.size() > 0,
               "Trade " << id_ << " requires either QuantLib instruments or legs to be created.");
    QL_REQUIRE(npvCurrency_ != "", "NPV currency has not been set for trade " << id_ << ".");
    // QL_REQUIRE(notional_ != Null<Real>(), "Notional has not been set for trade " << id_ << ".");
    // QL_REQUIRE(notionalCurrency_ != "", "Notional currency has not been set for trade " << id_ << ".");
    QL_REQUIRE(maturity_ != Null<Date>(), "Maturity not set for trade " << id_ << ".");
    QL_REQUIRE(!envelope_.empty(), "Envelope not set for trade " << id_ << ".");
    if (legs_.size() > 0) {
        QL_REQUIRE(legs_.size() == legPayers_.size(),
                   "Inconsistent number of pay/receive indicators for legs in trade " << id_ << ".");
        QL_REQUIRE(legs_.size() == legCurrencies_.size(),
                   "Inconsistent number of leg currencies for legs in trade " << id_ << ".");
    }
}

void Trade::reset() {
    // save accumulated timings from wrapper to trade before resetting
    if (instrument_ != nullptr) {
        savedNumberOfPricings_ += instrument_->getNumberOfPricings();
        savedCumulativePricingTime_ += instrument_->getCumulativePricingTime();
    }
    // reset members
    instrument_ = boost::shared_ptr<InstrumentWrapper>();
    legs_.clear();
    legCurrencies_.clear();
    legPayers_.clear();
    npvCurrency_ = "";
    notional_ = Null<Real>();
    notionalCurrency_ = "";
    maturity_ = Date();
    requiredFixings_.clear();
}
    
const std::map<std::string, boost::any>& Trade::additionalData() const { return additionalData_; }

} // namespace data
} // namespace ore
