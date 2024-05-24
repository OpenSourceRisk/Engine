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
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/instruments/fxforward.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Vanilla Option");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    additionalData_["boughtCurrency"] = assetName_;
    additionalData_["boughtAmount"] = quantity_;
    additionalData_["soldCurrency"] = currency_;
    additionalData_["soldAmount"] = quantity_ * strike_.value();

    QuantLib::Date today = Settings::instance().evaluationDate();
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();

    // If automatic exercise, check that we have a non-empty FX index string, parse it and attach curves from market.
    if (option_.isAutomaticExercise()) {

        QL_REQUIRE(!fxIndex_.empty(),
                   "FX option trade " << id() << " has automatic exercise so the FXIndex node needs to be populated.");

        // The strike is the number of units of sold currency (currency_) per unit of bought currency (assetName_).
        // So, the convention here is that the sold currency is domestic and the bought currency is foreign.
        // Note: intentionally use null calendar and 0 day fixing lag here because we will ask the FX index for its
        //       value on the expiry date without adjustment.
        index_ = buildFxIndex(fxIndex_, currency_, assetName_, market,
                              engineFactory->configuration(MarketContext::pricing));

        // Populate the external index name so that fixings work.
        indexName_ = fxIndex_;
    }

    const ext::optional<OptionPaymentData>& opd = option_.paymentData();
    expiryDate_ = parseDate(option_.exerciseDates().front());
    QuantLib::Date paymentDate = expiryDate_;
    if (option_.settlement() == "Physical" && opd) {
        if (opd->rulesBased()) {
            const Calendar& cal = opd->calendar();
            QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
            paymentDate = cal.advance(expiryDate_, opd->lag(), Days, opd->convention());
        } else {
            if (opd->dates().size() > 1)
                ore::data::StructuredTradeWarningMessage(
                    id(), tradeType(), "Trade build", "Found more than 1 payment date. The first one will be used.")
                    .log();
            paymentDate = opd->dates().front();
        }

        QL_REQUIRE(paymentDate >= expiryDate_, "Settlement date must be greater than or equal to expiry date.");

        if (expiryDate_ <= today) {
            // building an fx forward instrument instead of the option
            Date fixingDate;
            Currency boughtCcy = parseCurrency(assetName_);
            Currency soldCcy = parseCurrency(currency_);
            ext::shared_ptr<QuantLib::Instrument> instrument =
                ext::make_shared<QuantExt::FxForward>(quantity_, boughtCcy, soldAmount(), soldCcy, maturity_, false,
                                                        true, paymentDate, soldCcy, fixingDate);
            instrument_.reset(new VanillaInstrument(instrument));
            if (option_.exerciseData()) {
                if (option_.exerciseData()->date() <= expiryDate_) {
                    // option is exercised
                    // fxforward flow are flows from trade data
                    legs_ = {{ext::make_shared<SimpleCashFlow>(quantity_, paymentDate)},
                                {ext::make_shared<SimpleCashFlow>(soldAmount(), paymentDate)}};
                    legCurrencies_ = {assetName_, currency_};
                    legPayers_ = {false, true};
                } else
                    QL_REQUIRE(option_.exerciseData()->date() <= expiryDate_,
                                "Trade build error, exercise after option expiry is not allowed");
            } else {
                // option not exercised
                // set flows = 0
                legs_ = {};
            }
            forwardDate_ = paymentDate;
            paymentDate_ = paymentDate;
        }
        // Build the trade using the shared functionality in the base class.
        VanillaOptionTrade::build(engineFactory);
        maturity_ = paymentDate;
    } else {
        VanillaOptionTrade::build(engineFactory);
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
    strike_ = TradeStrike(soldAmount / boughtAmount, currency_);
    quantity_ = boughtAmount;
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    QL_REQUIRE(boughtAmount > 0.0, "positive BoughtAmount required");
    QL_REQUIRE(soldAmount > 0.0, "positive SoldAmount required");
}

XMLNode* FxOption::toXML(XMLDocument& doc) const {
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
