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

#include <ored/portfolio/builders/flexiswap.hpp>
#include <ored/portfolio/flexiswap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/swaption.hpp>

#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <qle/instruments/flexiswap.hpp>
#include <qle/instruments/flexiswapreplication.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FlexiSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("FlexiSwap::build() for id \"" << id() << "\" called.");

    // ISDA taxonomy

    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    // checks

    QL_REQUIRE(!underlyingData_.empty(), "MultiLegOption: no underlying given");

    // get engine builder

    auto builder =
        QuantLib::ext::dynamic_pointer_cast<SwaptionEngineBuilder>(engineFactory->builder("BermudanSwaption"));
    QL_REQUIRE(builder, "BermudanSwaption builder is null");

    // build underlying legs

    vector<Leg> underlyingLegs;
    vector<bool> underlyingPayers;
    vector<Currency> underlyingCurrencies;
    legCurrencies_.clear();

    for (Size i = 0; i < underlyingData_.size(); ++i) {
        auto legBuilder = engineFactory->legBuilder(underlyingData_[i].legType());
        underlyingLegs.push_back(legBuilder->buildLeg(underlyingData_[i], engineFactory, requiredFixings_,
                                                      builder->configuration(MarketContext::pricing)));
        underlyingCurrencies.push_back(parseCurrency(underlyingData_[i].currency()));
        legCurrencies_.push_back(underlyingData_[i].currency());
        underlyingPayers.push_back(underlyingData_[i].isPayer());
        DLOG("Added leg of type " << underlyingData_[i].legType() << " in currency " << underlyingData_[i].currency()
                                  << " is payer " << underlyingData_[i].isPayer());
    }

    // set npv currency

    npvCurrency_ = legCurrencies_.front();

    // set single currency flag

    bool isSingleCurrency = std::all_of(underlyingData_.begin(), underlyingData_.end(), [&](const LegData& d) {
        return d.currency() == underlyingData_[0].currency();
    });

    // build lower notional bounds

    std::vector<std::vector<Real>> lowerNotionalBounds;
    for (Size i = 0; i < underlyingData_.size(); ++i) {
        Schedule schedule = makeSchedule(underlyingData_[i].schedule());
        std::vector<double> tmpLowerNotionalBounds;
        std::vector<std::string> tmpLowerNotionalBoundDates;
        if (auto b = lowerNotionalBounds_.find(underlyingData_[i].currency()); b != lowerNotionalBounds_.end()) {
            std::tie(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates) = b->second;
        } else if (auto b = lowerNotionalBounds_.find(std::string());
                   b != lowerNotionalBounds_.end() && isSingleCurrency) {
            std::tie(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates) = b->second;
        } else {
            QL_FAIL("FlexiSwap: lower notional bounds for currency '" << underlyingData_[i].currency()
                                                                      << "' not given.");
        }
        lowerNotionalBounds.push_back(
            buildScheduledVectorNormalised(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates, schedule, 0.0));
    }

    // flexi swap replication

    Date today = Settings::instance().evaluationDate();

    bool generateNotionalExchanges = true;
    auto basket = generateFlexiSwapReplication(today, underlyingLegs, underlyingPayers, underlyingCurrencies,
                                               lowerNotionalBounds, generateNotionalExchanges);

    for (Size i = 0; i < basket.size(); ++i) {
        auto index = getInterestRateIndexFromLegs(basket[i]->legs());
        string qualifier =
            index.empty() ? npvCurrency_ : IndexNameTranslator::instance().oreName(index.front()->name());
        auto dates = ext::dynamic_pointer_cast<BermudanExercise>(basket[i]->exercise())->dates();
        std::vector<Date> maturities(dates.size(), basket[i]->maturityDate());
        auto strikes = getCalibrationStrikesFromLegs(basket[i]->legs(), dates);
        basket[i]->setPricingEngine(builder->engine(id() + "_" + std::to_string(i), qualifier, dates, maturities,
                                                    strikes, false, std::string(), std::string()));
    }

    // set trade members

    setSensitivityTemplate(*builder);
    addProductModelEngine(*builder);

    // instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(flexiSwap);

    // npvCurrency_ = ccy_str;
    // notional_ = std::max(currentNotional(fixLeg), currentNotional(fltLeg));
    // notionalCurrency_ = ccy_str;
    // legCurrencies_ = vector<string>(2, ccy_str);
    // legs_ = {fixLeg, fltLeg};
    // legPayers_ = {swap_[fixedLegIndex].isPayer(), swap_[floatingLegIndex].isPayer()};
    // maturity_ = flexiSwap->maturityDate();
    // maturityType_ = "FlexiSwap Leg Maturity Date";
    // addToRequiredFixings(fltLeg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings_));
}

void FlexiSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "FlexiSwapData");
    QL_REQUIRE(swapNode, "FlexiSwap::fromXML(): FlexiSwapData not found");
    // optionality given by lower notional bounds
    for (auto n : XMLUtils::getChildrenNodes(swapNode, "LowerNotionalBounds")) {
        auto ccy = XMLUtils::getAttribute(n, "currency");
        std::vector<std::string> tmpDates;
        auto tmpBounds = XMLUtils::getChildrenValuesWithAttributes<Real>(n, std::string(), "Notional", "startDate",
                                                                         tmpDates, &parseReal);
        lowerNotionalBounds_[ccy] = std::make_pair(tmpBounds, tmpDates);
    }
    // optionality given by exercise dates, types and values
    // noticePeriod_ = noticeCalendar_ = noticeConvention_ = "";
    // exerciseDates_.clear();
    // exerciseTypes_.clear();
    // exerciseValues_.clear();
    // XMLNode* prepayNode = XMLUtils::getChildNode(swapNode, "Prepayment");
    // if (prepayNode) {
    //     noticePeriod_ = XMLUtils::getChildValue(prepayNode, "NoticePeriod", false);
    //     noticeCalendar_ = XMLUtils::getChildValue(prepayNode, "NoticeCalendar", false);
    //     noticeConvention_ = XMLUtils::getChildValue(prepayNode, "NoticeConvention", false);
    //     XMLNode* optionsNode = XMLUtils::getChildNode(prepayNode, "PrepaymentOptions");
    //     if (optionsNode) {
    //         auto prepayOptionNodes = XMLUtils::getChildrenNodes(optionsNode, "PrepaymentOption");
    //         for (auto const n : prepayOptionNodes) {
    //             exerciseDates_.push_back(XMLUtils::getChildValue(n, "ExerciseDate", true));
    //             exerciseTypes_.push_back(XMLUtils::getChildValue(n, "Type", true));
    //             exerciseValues_.push_back(parseReal(XMLUtils::getChildValue(n, "Value", true)));
    //         }
    //     }
    // }
    // long short flag
    optionLongShort_ = XMLUtils::getChildValue(swapNode, "OptionLongShort", true);
    // underlying legs
    underlyingData_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        underlyingData_.push_back(ld);
    }
}

XMLNode* FlexiSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("FlexiSwapData");
    XMLUtils::appendNode(node, swapNode);
    // optionality given by lower notional bounds
    for (auto const& [ccy, l] : lowerNotionalBounds_) {
        auto n = doc.allocNode("LowerNotionalBounds");
        XMLUtils::addAttribute(doc, n, "currency", ccy);
        XMLUtils::addChildrenWithOptionalAttributes(doc, n, std::string(), "Notional", l.first, "startDate", l.second);
    }
    // optionality given by exercise dates, types and values
    // if (!exerciseDates_.empty()) {
    //     XMLNode* prepayNode = doc.allocNode("Prepayment");
    //     XMLUtils::appendNode(swapNode, prepayNode);
    //     if (!noticePeriod_.empty())
    //         XMLUtils::addChild(doc, prepayNode, "NoticePeriod", noticePeriod_);
    //     if (!noticeCalendar_.empty())
    //         XMLUtils::addChild(doc, prepayNode, "NoticeCalendar", noticeCalendar_);
    //     if (!noticeConvention_.empty())
    //         XMLUtils::addChild(doc, prepayNode, "NoticeConvention", noticeConvention_);
    //     XMLNode* optionsNode = doc.allocNode("PrepaymentOptions");
    //     XMLUtils::appendNode(prepayNode, optionsNode);
    //     for (Size i = 0; i < exerciseDates_.size(); ++i) {
    //         XMLNode* exerciseNode = doc.allocNode("PrepaymentOption");
    //         XMLUtils::appendNode(optionsNode, exerciseNode);
    //         XMLUtils::addChild(doc, exerciseNode, "ExerciseDate", exerciseDates_.at(i));
    //         XMLUtils::addChild(doc, exerciseNode, "Type", exerciseTypes_.at(i));
    //         XMLUtils::addChild(doc, exerciseNode, "Value", exerciseValues_.at(i));
    //     }
    // }
    // long short option flag
    XMLUtils::addChild(doc, swapNode, "OptionLongShort", optionLongShort_);
    // underlying legs
    for (Size i = 0; i < underlyingData_.size(); i++)
        XMLUtils::appendNode(swapNode, underlyingData_[i].toXML(doc));
    return node;
}

} // namespace data
} // namespace ore
