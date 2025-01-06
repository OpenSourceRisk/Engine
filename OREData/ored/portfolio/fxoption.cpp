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
#include <qle/termstructures/blackdeltautilities.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void FxOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();

    auto it = engineFactory->engineData()->globalParameters().find("RunType");
    if (it != engineFactory->engineData()->globalParameters().end() && it->second != "PortfolioAnalyser" && delta_!=0.0) {

        std::string conventionName = assetName_ + "-" + currency_ + "-FXOPTION";
        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
        auto fxOptConv = QuantLib::ext::dynamic_pointer_cast<FxOptionConvention>(conventions->get(conventionName));
        QL_REQUIRE(fxOptConv, "unable to cast convention '" << conventionName << "' into FxOptionConvention");

        auto dt = fxOptConv->deltaType();
        auto fxConvId = fxOptConv->fxConventionID();
        auto fxConv = QuantLib::ext::dynamic_pointer_cast<FXConvention>(conventions->get(fxConvId));

        auto pairSpot = market->fxSpot(assetName_ + currency_);
        auto domYts = market->discountCurve(assetName_).currentLink();
        auto forYts = market->discountCurve(currency_).currentLink();
        auto volSurface = market->fxVol(assetName_ + currency_).currentLink();


        auto t = volSurface->timeFromReference(parseDate(option_.exerciseDates().front()));

        auto spotCalendar_ = parseCalendar(assetName_ + "," + currency_);
        Date d = parseDate(option_.exerciseDates().front());
        auto spotDays_ = fxConv->spotDays();
        Date settl = spotCalendar_.advance(Settings::instance().evaluationDate(), spotDays_ * Days);
        Date settlFwd = spotCalendar_.advance(d, spotDays_ * Days);

        auto domDisc = domYts->discount(settlFwd) / domYts->discount(settl);
        auto forDisc = forYts->discount(settlFwd) / forYts->discount(settl);
        if (option_.callPut() == "Call"){
            auto strikeFromDelta = QuantExt::getStrikeFromDelta(Option::Call, delta_, dt, pairSpot->value(), domDisc,
                                                                forDisc,
                                         volSurface, t);
            strike_.setValue(strikeFromDelta);

        } else {
            auto strikeFromDelta = QuantExt::getStrikeFromDelta(Option::Put, -delta_, dt, pairSpot->value(), domDisc,
                                                                forDisc,
                                                       volSurface, t);
            strike_.setValue(strikeFromDelta);
        }
        
    }

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
                ore::data::StructuredTradeWarningMessage(id(), tradeType(), "Trade build",
                                                         "Found more than 1 payment date. The first one will be used.")
                    .log();
            paymentDate = opd->dates().front();
        }

        QL_REQUIRE(paymentDate >= expiryDate_, "Settlement date must be greater than or equal to expiry date.");

        forwardDate_ = paymentDate;
        paymentDate_ = paymentDate;

        if (expiryDate_ <= today) {

            // option is expired, build an fx forward representing the physical underlying if exercised or null if not

            Currency boughtCcy = parseCurrency(assetName_);
            Currency soldCcy = parseCurrency(currency_);
            ext::shared_ptr<QuantLib::Instrument> qlinstr;
            if (option_.exerciseData()) {
                QL_REQUIRE(option_.exerciseData()->date() <= expiryDate_,
                           "Trade build error, exercise after option expiry is not allowed");
                // option is exercised
                legs_ = {{ext::make_shared<SimpleCashFlow>(quantity_, paymentDate)},
                         {ext::make_shared<SimpleCashFlow>(soldAmount(), paymentDate)}};
                legCurrencies_ = {assetName_, currency_};
                legPayers_ = {false, true};
                qlinstr = ext::make_shared<QuantExt::FxForward>(quantity_, boughtCcy, soldAmount(), soldCcy,
                                                                paymentDate, false, true);
            } else {
                // option not exercised
                legs_ = {};
                qlinstr = ext::make_shared<QuantExt::FxForward>(0.0, boughtCcy, 0.0, soldCcy, paymentDate, false, true);
            }

            QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("FxForward");
            auto fxBuilder = QuantLib::ext::dynamic_pointer_cast<FxForwardEngineBuilderBase>(builder);
            QL_REQUIRE(fxBuilder, "FxOption::build(): internal error: could not cast to FxForwardEngineBuilderBase");
            qlinstr->setPricingEngine(fxBuilder->engine(boughtCcy,soldCcy));
            auto configuration = fxBuilder->configuration(MarketContext::pricing);

            maturity_ = paymentDate;
            maturityType_ = "Payment Date";
            Position::Type positionType = parsePositionType(option_.longShort());
            Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
            Real mult = bsInd;

            std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
            std::vector<Real> additionalMultipliers;
            maturity_ =
                std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, mult,
                                                option_.premiumData(), -bsInd, soldCcy, engineFactory, configuration));
            instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlinstr, mult, additionalInstruments,
                                                                        additionalMultipliers);
            notionalCurrency_ = npvCurrency_ = soldCcy.code();
            notional_ = soldAmount();
            setSensitivityTemplate(*builder);
            addProductModelEngine(*builder);
            return;
        }
    }

    VanillaOptionTrade::build(engineFactory);
}

void FxOption::fromXML(XMLNode* node) {
    VanillaOptionTrade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxOptionData");
    QL_REQUIRE(fxNode, "No FxOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    assetName_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    currency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    double boughtAmount = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    double soldAmount = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", false);
    delta_ = XMLUtils::getChildValueAsDouble(fxNode, "Delta", false);
    if (soldAmount == 0.0) {
        QL_REQUIRE(delta_ > 0.0, "Non null or negative delta required");
    } else {
        QL_REQUIRE(soldAmount > 0.0, "positive SoldAmount required");
    }
    strike_ = TradeStrike(soldAmount / boughtAmount, currency_);
    quantity_ = boughtAmount;
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    QL_REQUIRE(boughtAmount > 0.0, "positive BoughtAmount required");
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
