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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/currencycheck.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Set the assetName_ as it may have changed after lookup
    assetName_ = equityName();

    // Populate the index_ in case the option is automatic exercise.
    const boost::shared_ptr<Market>& market = engineFactory->market();
    index_ = *market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing));

    // check the equity currency
    Currency equityCurrency =
        market->equityCurve(assetName_, engineFactory->configuration(MarketContext::pricing))->currency();
    QL_REQUIRE(!equityCurrency.empty(), "No equity currency in equityCurve for equity " << assetName_ << ".");

    // Set the strike currency - if we have a minor currency, convert the strike
    if (!strikeCurrency_.empty()) {
        QL_REQUIRE(parseCurrencyWithMinors(strikeCurrency_) == equityCurrency,
                   "Strike currency " << strikeCurrency_ << " does not match equity currency " << equityCurrency
                                      << " for trade " << id() << ".");
        strike_ = convertMinorToMajorCurrency(strikeCurrency_, localStrike_);
    } else {
        // If payoff currency and underlying currency are equivalent (and payoff currency could be a minor currency)
        if (parseCurrencyWithMinors(localCurrency_) == equityCurrency) {
            strike_ = convertMinorToMajorCurrency(localCurrency_, localStrike_);
            TLOG("Setting strike currency to payoff currency " << localCurrency_ << " for trade " << id() << ".");
            strikeCurrency_ = localCurrency_;
        } else {
            // If quanto payoff, then strike currency must be populated to avoid confusion over what the
            // currency of the strike payoff is: can be either underlying currency or payoff currency
            QL_FAIL("Strike currency must be specified for a quanto payoff for trade " << id() << ".");
        }
    }

    // Quanto payoff condition, i.e. currency_ != underlyingCurrency_, will be checked in VanillaOptionTrade::build()
    currency_ = parseCurrencyWithMinors(localCurrency_).code();
    underlyingCurrency_ = equityCurrency.code();

    // Build the trade using the shared functionality in the base class.
    VanillaOptionTrade::build(engineFactory);

    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        DLOG("Implied vol for " << tradeType_ << " on " << assetName_ << " with expiry " << expiryDate_
                                << " and strike " << strike_ << " is "
                                << market->equityVol(assetName_)->blackVol(expiryDate_, strike_));
    }

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = localStrike_;
    additionalData_["strikeCurrency"] = strikeCurrency_;
}

void EquityOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityOptionData");
    QL_REQUIRE(eqNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    localCurrency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    localStrike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);
    strikeCurrency_ = XMLUtils::getChildValue(eqNode, "StrikeCurrency", false);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityOption::toXML(XMLDocument& doc) {
    XMLNode* node = VanillaOptionTrade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", localCurrency_);
    XMLUtils::addChild(doc, eqNode, "Strike", localStrike_);

    Currency ccy = parseCurrencyWithMinors(localCurrency_);
    Currency strikeCcy = parseCurrencyWithMinors(strikeCurrency_);
    if (!strikeCurrency_.empty() && ccy != strikeCcy)
        XMLUtils::addChild(doc, eqNode, "StrikeCurrency", strikeCurrency_);

    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

std::map<AssetClass, std::set<std::string>> EquityOption::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({equityName()})}};
}

} // namespace data
} // namespace ore
