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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    const boost::shared_ptr<Market>& market = engineFactory->market();

    // If automatic exercise, check that we have a non-empty FX index string, parse it and attach curves from market.
    if (option_.automaticExercise()) {

        QL_REQUIRE(!fxIndex_.empty(),
                   "FX option trade " << id() << " has automatic exercise so the FXIndex node needs to be populated.");

        // The strike is the number of units of sold currency (currency_) per unit of bought currency (assetName_).
        // So, the convention here is that the sold currency is domestic and the bought currency is foreign.
        // Note: intentionally use null calendar and 0 day fixing lag here because we will ask the FX index for its
        //       value on the expiry date without adjustment.
        index_ = buildFxIndex(fxIndex_, currency_, assetName_, market,
                              engineFactory->configuration(MarketContext::pricing), "NullCalendar", 0);

        // Populate the external index name so that fixings work.
        indexName_ = fxIndex_;
    }

    // Build the trade using the shared functionality in the base class.
    VanillaOptionTrade::build(engineFactory);

    // LOG the volatility if the trade expiry date is in the future.
    if (expiryDate_ > Settings::instance().evaluationDate()) {
        const string& ccyPairCode = assetName_ + currency_;
        DLOG("Implied vol for " << tradeType_ << " on " << ccyPairCode << " with expiry " << expiryDate_
                                << " and strike " << strike_ << " is "
                                << market->fxVol(ccyPairCode)->blackVol(expiryDate_, strike_));
    }
}

void FxOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxOptionData");
    QL_REQUIRE(fxNode, "No FxOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    assetName_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    currency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    double boughtAmount = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    double soldAmount = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
    strike_ = soldAmount / boughtAmount;
    quantity_ = boughtAmount;
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
}

XMLNode* FxOption::toXML(XMLDocument& doc) {
    // TODO: Should call parent class to xml?
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency());
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount());
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency());
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount());

    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);

    return node;
}
} // namespace data
} // namespace ore
