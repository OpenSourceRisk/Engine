/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/commodityspreadoption.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/utilities/marketdata.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/commodityspreadoption.hpp>

using namespace QuantExt;
using namespace QuantLib;
using std::max;
using std::string;
using std::vector;

namespace ore::data {

void CommoditySpreadOption::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    DLOG("CommoditySpreadOption::build() called for trade " << id());
    reset();
    QL_REQUIRE(legData_.size() == 2, "Only two legs supported");
    QL_REQUIRE(legData_[0].currency() == legData_[1].currency(), "Both legs must have same currency");
    QL_REQUIRE(legData_[0].isPayer() != legData_[1].isPayer(), "Need one payer and one receiver leg");

    if (!optionData_.style().empty()) {
        QuantLib::Exercise::Type exerciseType = parseExerciseType(optionData_.style());
        QL_REQUIRE(exerciseType == QuantLib::Exercise::Type::European, "Only European spread option supported");
    }

    maturity_ = Date();
    npvCurrency_ = legData_[0].currency();
    Size payerLegId = legData_[0].isPayer() ? 0 : 1;

    // build the relevant fxIndexes;
    std::vector<boost::shared_ptr<QuantExt::FxIndex>> fxIndexes(2, nullptr);
    Currency ccy = parseCurrency(npvCurrency_);
    Option::Type optionType = parseOptionType(optionData_.callPut());

    // init engine factory builder
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    auto engineBuilder = boost::dynamic_pointer_cast<CommoditySpreadOptionEngineBuilder>(builder);
    // get config
    auto config = builder->configuration(MarketContext::pricing);

    // set exercise date to the pricing date of the coupon
    QL_REQUIRE(optionData_.exerciseDates().size() == 0,
               "Only European spread option supported, expiry date is end_date of the period");

    // Build the commodity legs

    for (Size i = 0; i < legData_.size();
         ++i) { // The order is important, the first leg is always the long position, the second is the short;
        legPayers_.push_back(legData_[i].isPayer());

        // build legs

        auto commLegData = (boost::dynamic_pointer_cast<CommodityFloatingLegData>(legData_[i].concreteLegData()));
        QL_REQUIRE(commLegData, "CommoditySpreadOption leg data should be of type CommodityFloating");

        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        auto cflb = boost::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);

        QL_REQUIRE(cflb, "CommoditySpreadOption: Expected a CommodityFloatingLegBuilder for leg "
                             << i << " but got " << legData_[i].legType());
        Leg leg = cflb->buildLeg(legData_[i], engineFactory, requiredFixings_, config);

        // setup the cf indexes
        QL_REQUIRE(!leg.empty(), "CommoditySpreadOption: Leg " << i << " has no coupons");
        auto index = boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(leg.front())->index();

        // check ccy consistency
        auto underlyingCcy = index->priceCurve()->currency();
        auto tmpFx = commLegData->fxIndex();
        if (tmpFx.empty()) {
            QL_REQUIRE(underlyingCcy.code() == npvCurrency_,
                       "CommoditySpreadOption, inconsistent currencies: Settlement currency is "
                           << npvCurrency_ << ", leg " << i + 1 << " currency " << legData_[i].currency()
                           << ", underlying currency " << underlyingCcy << ", no FxIndex provided");
        } else {
            QL_REQUIRE(underlyingCcy.code() != npvCurrency_,
                       "CommoditySpreadOption, inconsistent currencies: Settlement currency is "
                           << npvCurrency_ << ", leg " << i + 1 << " currency " << legData_[i].currency()
                           << ", underlying currency " << underlyingCcy << ", FxIndex " << tmpFx << "provided");
            auto domestic = npvCurrency_;
            auto foreign = underlyingCcy.code();
            fxIndexes[i] = buildFxIndex(tmpFx, domestic, foreign, engineFactory->market(),
                                        engineFactory->configuration(MarketContext::pricing));
            // update required fixings. This is handled automatically in the leg builder in the averaging case
            if (commLegData->isAveraged()) {
                for (auto cf : leg) {
                    auto fixingDate = cf->date();
                    if (!fxIndexes[i]->fixingCalendar().isBusinessDay(
                            fixingDate)) { // If fx index is not available for the cashflow pricing day,
                        // this ensures to require the previous valid one which will be used in pricing
                        // from fxIndex()->fixing(...)
                        Date adjustedFixingDate = fxIndexes[i]->fixingCalendar().adjust(fixingDate, Preceding);
                        requiredFixings_.addFixingDate(adjustedFixingDate, tmpFx);
                    } else {
                        requiredFixings_.addFixingDate(fixingDate, tmpFx);
                    }
                }
            }
        }
        legs_.push_back(leg);
        legCurrencies_.push_back(legData_[i].currency()); // all legs and cf are priced with the same ccy
    }

    QL_REQUIRE(legs_[0].size() == legs_[1].size(),
               "CommoditySpreadOption: the two legs must contain the same number of cashflows.");

    const boost::optional<OptionPaymentData>& opd = optionData_.paymentData();

    Position::Type positionType = parsePositionType(optionData_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    vector<boost::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;

    for (size_t i = 0; i < legs_[0].size(); ++i) {
        auto longFlow = boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[1 - payerLegId][i]);
        auto shortFlow = boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[payerLegId][i]);

        QuantLib::Real quantity = longFlow->periodQuantity();

        QL_REQUIRE(quantity == shortFlow->periodQuantity(), "all cashflows must refer to the same quantity");

        QuantLib::Date lastPricingDate = std::max(longFlow->lastPricingDate(),shortFlow->lastPricingDate());

        QuantLib::Date paymentDate = longFlow->date();

        QL_REQUIRE(paymentDate == shortFlow->date(),
                   "all cashflows must refer to the same paymentDate, its used as the settlementDate of the option");

        // Set the expiry date to the lastest pricing date of the
        expiryDate_ = lastPricingDate;
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate_);

        // Override the payment date with a general one from option data if provided, valid for all

        if (opd) {
            if (opd->rulesBased()) {
                const Calendar& cal = opd->calendar();
                QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
                paymentDate = cal.advance(expiryDate_, opd->lag(), Days, opd->convention());
            } else {
                const vector<Date>& dates = opd->dates();
                QL_REQUIRE(dates.size() == 1, "Need exactly one payment date for cash settled European option.");
                paymentDate = dates[0];
            }
            QL_REQUIRE(paymentDate >= expiryDate_, "Payment date must be greater than or equal to expiry date.");
        }

        // maturity gets overwritten every time, and it is ok. If the last option is settled with delay, maturity is set
        // to the settlement date.
        maturity_ = maturity_ == Date() ? paymentDate : std::max(maturity_, paymentDate);

        // build the instrument for the i-th cfs
        boost::shared_ptr<QuantExt::CommoditySpreadOption> spreadOption =
            boost::make_shared<QuantExt::CommoditySpreadOption>(longFlow, shortFlow, exercise, quantity, strike_,
                                                                optionType, paymentDate, fxIndexes[1 - payerLegId],
                                                                fxIndexes[1 - payerLegId]);

        // build and assign the engine
        boost::shared_ptr<PricingEngine> commoditySpreadOptionEngine =
            engineBuilder->engine(ccy, longFlow->index(), shortFlow->index(), id());
        spreadOption->setPricingEngine(commoditySpreadOptionEngine);

        additionalInstruments.push_back(spreadOption);
        additionalMultipliers.push_back(bsInd);
    }
    auto qlInst = additionalInstruments.back();
    auto qlInstMult = additionalMultipliers.back();
    additionalInstruments.pop_back();
    additionalMultipliers.pop_back();

    // Add premium
    auto configuration = engineBuilder->configuration(MarketContext::pricing);
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, bsInd,
                                                optionData_.premiumData(), -bsInd, ccy, engineFactory, configuration));

    instrument_ =
        boost::make_shared<VanillaInstrument>(qlInst, qlInstMult, additionalInstruments, additionalMultipliers);

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Other");
    additionalData_["isdaSubProduct"] = std::string("");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");
    if (!optionData_.premiumData().premiumData().empty()) {
        auto premium = optionData_.premiumData().premiumData().front();
        additionalData_["premiumAmount"] = premium.amount;
        additionalData_["premiumPaymentDate"] = premium.payDate;
        additionalData_["premiumCurrency"] = premium.ccy;
    }
}

void CommoditySpreadOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* csoNode = XMLUtils::getChildNode(node, "CommoditySpreadOptionData");
    QL_REQUIRE(csoNode, "No CommoditySpreadOptionData Node");

    XMLNode* optionDataNode = XMLUtils::getChildNode(csoNode, "OptionData");
    QL_REQUIRE(optionDataNode, "Invalid CommmoditySpreadOption trade xml: found no OptionData Node");

    optionData_.fromXML(optionDataNode);
    strike_ = XMLUtils::getChildValueAsDouble(csoNode, "SpreadStrike", true);

    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(csoNode, "LegData");
    QL_REQUIRE(nodes.size() == 2, "CommoditySpreadOption: Exactly two LegData nodes expected");
    for (auto& node : nodes) {
        auto ld = createLegData();
        ld->fromXML(node);
        legData_.push_back(*ld);
    }
    QL_REQUIRE(legData_[0].isPayer() != legData_[1].isPayer(),
               "CommoditySpreadOption: both a long and a short Assets are required.");
    // settlementCcy_ = XMLUtils::getChildValue(csoNode, "Currency", true);
}

XMLNode* CommoditySpreadOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);

    XMLNode* csoNode = doc.allocNode("CommoditySpreadOptionData");

    XMLUtils::appendNode(node, csoNode);
    for (size_t i = 0; i < legData_.size(); ++i) {
        XMLUtils::appendNode(csoNode, legData_[i].toXML(doc));
    }
    XMLUtils::appendNode(csoNode, optionData_.toXML(doc));
    XMLUtils::addChild(doc, csoNode, "SpreadStrike", strike_);
    return node;
}

} // namespace ore::data
