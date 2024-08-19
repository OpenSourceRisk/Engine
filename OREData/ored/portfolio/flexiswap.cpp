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
#include <qle/instruments/flexiswap.hpp>

#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FlexiSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    LOG("FlexiSwap::build() for id \"" << id() << "\" called.");

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");  
    additionalData_["isdaTransaction"] = string("");  

    QL_REQUIRE(swap_.size() == 2, "swap must have 2 legs");
    QL_REQUIRE(swap_[0].currency() == swap_[1].currency(), "swap must be single currency");

    string ccy_str = swap_[0].currency();
    Currency currency = parseCurrency(ccy_str);

    Size fixedLegIndex, floatingLegIndex;
    if (swap_[0].legType() == "Floating" && swap_[1].legType() == "Fixed") {
        floatingLegIndex = 0;
        fixedLegIndex = 1;
    } else if (swap_[1].legType() == "Floating" && swap_[0].legType() == "Fixed") {
        floatingLegIndex = 1;
        fixedLegIndex = 0;
    } else {
        QL_FAIL("Invalid leg types " << swap_[0].legType() << " + " << swap_[1].legType());
    }

    QuantLib::ext::shared_ptr<FixedLegData> fixedLegData =
        QuantLib::ext::dynamic_pointer_cast<FixedLegData>(swap_[fixedLegIndex].concreteLegData());
    QuantLib::ext::shared_ptr<FloatingLegData> floatingLegData =
        QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(swap_[floatingLegIndex].concreteLegData());

    QL_REQUIRE(fixedLegData != nullptr, "expected fixed leg data");
    QL_REQUIRE(floatingLegData != nullptr, "expected floating leg data");

    QuantLib::ext::shared_ptr<EngineBuilder> tmp = engineFactory->builder("FlexiSwap");
    auto builder = QuantLib::ext::dynamic_pointer_cast<FlexiSwapBGSEngineBuilderBase>(tmp);
    QL_REQUIRE(builder, "No Flexi-Swap Builder found for \"" << id() << "\"");

    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    vector<Real> fixedNominal = buildScheduledVectorNormalised(
        swap_[fixedLegIndex].notionals(), swap_[fixedLegIndex].notionalDates(), fixedSchedule, 0.0);
    vector<Real> floatNominal = buildScheduledVectorNormalised(
        swap_[floatingLegIndex].notionals(), swap_[floatingLegIndex].notionalDates(), floatingSchedule, 0.0);
    vector<Real> fixedRate =
        buildScheduledVectorNormalised(fixedLegData->rates(), fixedLegData->rateDates(), fixedSchedule, 0.0);
    vector<Real> spreads = buildScheduledVectorNormalised(floatingLegData->spreads(), floatingLegData->spreadDates(),
                                                          floatingSchedule, 0.0);
    vector<Real> gearings = buildScheduledVectorNormalised(floatingLegData->gearings(), floatingLegData->gearingDates(),
                                                           floatingSchedule, 1.0);
    vector<Real> caps = buildScheduledVectorNormalised(floatingLegData->caps(), floatingLegData->capDates(),
                                                       floatingSchedule, (Real)Null<Real>());
    vector<Real> floors = buildScheduledVectorNormalised(floatingLegData->floors(), floatingLegData->floorDates(),
                                                         floatingSchedule, (Real)Null<Real>());
    floatingIndex_ = floatingLegData->index();
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(floatingIndex_, builder->configuration(MarketContext::pricing));
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());
    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());
    VanillaSwap::Type type = swap_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    vector<Real> lowerNotionalBounds = fixedNominal; // default, no optionality
    std::vector<bool> notionalCanBeDecreased(fixedNominal.size(), true);

    // check we have at most one optionality description

    QL_REQUIRE(lowerNotionalBounds_.empty() || exerciseDates_.empty(),
               "can not have lower notional bounds and exercise dates / types / values specified at the same time");

    // optionality is given by lower notional bounds

    if (!lowerNotionalBounds_.empty()) {
        lowerNotionalBounds =
            buildScheduledVectorNormalised(lowerNotionalBounds_, lowerNotionalBoundsDates_, fixedSchedule, 0.0);
        DLOG("optionality is given by lower notional bounds");
    }

    // optionality is given by exercise dates, types, values

    // FIXME this is an approximation, we build an approximate instrument here using the global lower
    // notional bounds; for a correct representation we would need local bounds that depend on the
    // current notional of the swap; see below where the approximation occurs specifically

    if (!exerciseDates_.empty()) {
        DLOG("optionality is given by exercise dates, types, values");

        // FIXME: we also ignore the notice period at this stage of the implementation, the notice day
        // is always assumed to lie on the fixing date of the corresponding float period of the swap

        // start with no optionality
        notionalCanBeDecreased = std::vector<bool>(fixedNominal.size(), false);

        // loop over exercise dates and update lower notional bounds belonging to that exercise
        Date previousExerciseDate = Null<Date>();
        for (Size i = 0; i < exerciseDates_.size(); ++i) {
            Date d = parseDate(exerciseDates_[i]);
            QL_REQUIRE(exerciseValues_[i] > 0.0 || close_enough(exerciseValues_[i], 0.0),
                       "exercise value #" << i << " (" << exerciseValues_[i] << ") must be non-negative");
            QL_REQUIRE(i == 0 || previousExerciseDate < d, "exercise dates must be strictly increasing, got "
                                                               << QuantLib::io::iso_date(previousExerciseDate)
                                                               << " and " << QuantLib::io::iso_date(d) << " as #" << i
                                                               << " and #" << i + 1);
            previousExerciseDate = d;
            // determine the fixed period that follows the exercise date
            Size exerciseIdx = std::lower_bound(fixedSchedule.dates().begin(), fixedSchedule.dates().end(), d) -
                               fixedSchedule.dates().begin();
            if (exerciseIdx >= fixedSchedule.dates().size() - 1) {
                DLOG("exercise date "
                     << QuantLib::io::iso_date(d)
                     << " ignored since there is no whole fixed leg period with accrual start >= exercise date");
                continue;
            }
            notionalCanBeDecreased[exerciseIdx] = true;
            if (exerciseTypes_[i] == "ReductionUpToLowerBound") {
                for (Size j = exerciseIdx; j < lowerNotionalBounds.size(); ++j) {
                    lowerNotionalBounds[j] = std::min(lowerNotionalBounds[j], exerciseValues_[i]);
                }
            } else if (exerciseTypes_[i] == "ReductionByAbsoluteAmount" ||
                       exerciseTypes_[i] == "ReductionUpToAbsoluteAmount") {
                // FIXME we just assume that all prepayment option before this one here were exercised
                // and reduce the lower notional bounds by the current exercise amount; we also treat
                // "by" the same as "up to"
                for (Size j = exerciseIdx; j < lowerNotionalBounds.size(); ++j) {
                    lowerNotionalBounds[j] = std::max(lowerNotionalBounds[j] - exerciseValues_[i], 0.0);
                }
            } else {
                QL_FAIL("exercise type '" << exerciseTypes_[i]
                                          << "' unknown, expected ReductionUpToLowerBound, ReductionByAbsoluteAmount, "
                                             "ReductionUpToAbsoluteAmount");
            }
        }
    }

    DLOG("fixedPeriod#,notional,lowerNotionalBound,canBeReduced");
    for (Size i = 0; i < lowerNotionalBounds.size(); ++i) {
        DLOG(i << "," << fixedNominal.at(i) << "," << lowerNotionalBounds[i] << "," << std::boolalpha
               << notionalCanBeDecreased[i]);
    }

    // set up ql instrument

    Position::Type optionLongShort = parsePositionType(optionLongShort_);

    auto flexiSwap = QuantLib::ext::make_shared<QuantExt::FlexiSwap>(
        type, fixedNominal, floatNominal, fixedSchedule, fixedRate, fixedDayCounter, floatingSchedule, *index, gearings,
        spreads, caps, floors, floatingDayCounter, lowerNotionalBounds, optionLongShort, notionalCanBeDecreased,
        paymentConvention);

    auto fixLeg = flexiSwap->leg(0);
    auto fltLeg = flexiSwap->leg(1);

    // set coupon pricers if needed (for flow report, discounting swap engine, not used in LGM engine)

    bool hasCapsFloors = false;
    for (auto const& k : caps) {
        if (k != Null<Real>())
            hasCapsFloors = true;
    }
    for (auto const& k : floors) {
        if (k != Null<Real>())
            hasCapsFloors = true;
    }
    if (hasCapsFloors) {
        QuantLib::ext::shared_ptr<EngineBuilder> cfBuilder = engineFactory->builder("CapFlooredIborLeg");
        QL_REQUIRE(cfBuilder, "No builder found for CapFlooredIborLeg");
        QuantLib::ext::shared_ptr<CapFlooredIborLegEngineBuilder> cappedFlooredIborBuilder =
            QuantLib::ext::dynamic_pointer_cast<CapFlooredIborLegEngineBuilder>(cfBuilder);
        QL_REQUIRE(cappedFlooredIborBuilder != nullptr, "expected CapFlooredIborLegEngineBuilder");
        QuantLib::ext::shared_ptr<FloatingRateCouponPricer> couponPricer =
            cappedFlooredIborBuilder->engine(IndexNameTranslator::instance().oreName(index->name()));
        QuantLib::setCouponPricer(fltLeg, couponPricer);
    }

    // determine expiries and strikes for calibration basket (simple approach, a la summit)
    std::vector<Date> expiryDates;
    std::vector<Real> strikes;
    Date today = Settings::instance().evaluationDate();
    Size legRatio = fltLeg.size() / fixLeg.size(); // no remainder by construction of a flexi swap
    for (Size i = 0; i < fltLeg.size(); ++i) {
        auto fltcpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(fltLeg[i]);
        if (fltcpn != nullptr && fltcpn->fixingDate() > today && i % legRatio == 0) {
            expiryDates.push_back(fltcpn->fixingDate());
            auto fixcpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixLeg[i / legRatio]);
            QL_REQUIRE(fixcpn != nullptr, "FlexiSwap Builder: expected fixed rate coupon");
            strikes.push_back(fixcpn->rate() - fltcpn->spread());
        }
    }

    // set pricing engine, init instrument and other trade members

    flexiSwap->setPricingEngine(
        builder->engine(id(), "", index.empty() ? ccy_str : IndexNameTranslator::instance().oreName(index->name()),
                        expiryDates, flexiSwap->maturityDate(), strikes));
    setSensitivityTemplate(*builder);

    // FIXME this won't work for exposure, currently not supported
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(flexiSwap);

    npvCurrency_ = ccy_str;
    notional_ = std::max(currentNotional(fixLeg), currentNotional(fltLeg));
    notionalCurrency_ = ccy_str;
    legCurrencies_ = vector<string>(2, ccy_str);
    legs_ = {fixLeg, fltLeg};
    legPayers_ = {swap_[fixedLegIndex].isPayer(), swap_[floatingLegIndex].isPayer()};
    maturity_ = flexiSwap->maturityDate();
    addToRequiredFixings(fltLeg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings_));
}

void FlexiSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "FlexiSwapData");
    QL_REQUIRE(swapNode, "FlexiSwap::fromXML(): FlexiSwapData not found");
    // optionality given by lower notional bounds
    lowerNotionalBounds_ = XMLUtils::getChildrenValuesWithAttributes<Real>(
        swapNode, "LowerNotionalBounds", "Notional", "startDate", lowerNotionalBoundsDates_, &parseReal);
    // optionality given by exercise dates, types and values
    noticePeriod_ = noticeCalendar_ = noticeConvention_ = "";
    exerciseDates_.clear();
    exerciseTypes_.clear();
    exerciseValues_.clear();
    XMLNode* prepayNode = XMLUtils::getChildNode(swapNode, "Prepayment");
    if (prepayNode) {
        noticePeriod_ = XMLUtils::getChildValue(prepayNode, "NoticePeriod", false);
        noticeCalendar_ = XMLUtils::getChildValue(prepayNode, "NoticeCalendar", false);
        noticeConvention_ = XMLUtils::getChildValue(prepayNode, "NoticeConvention", false);
        XMLNode* optionsNode = XMLUtils::getChildNode(prepayNode, "PrepaymentOptions");
        if (optionsNode) {
            auto prepayOptionNodes = XMLUtils::getChildrenNodes(optionsNode, "PrepaymentOption");
            for (auto const n : prepayOptionNodes) {
                exerciseDates_.push_back(XMLUtils::getChildValue(n, "ExerciseDate", true));
                exerciseTypes_.push_back(XMLUtils::getChildValue(n, "Type", true));
                exerciseValues_.push_back(parseReal(XMLUtils::getChildValue(n, "Value", true)));
            }
        }
    }
    // long short flag
    optionLongShort_ = XMLUtils::getChildValue(swapNode, "OptionLongShort", true);
    // underlying legs
    swap_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld; // we do not allow ORE+ leg types anyway
        ld.fromXML(nodes[i]);
        swap_.push_back(ld);
    }
}

XMLNode* FlexiSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("FlexiSwapData");
    XMLUtils::appendNode(node, swapNode);
    // optionality given by lower notional bounds
    if (!lowerNotionalBounds_.empty()) {
        XMLUtils::addChildrenWithOptionalAttributes(doc, swapNode, "LowerNotionalBounds", "Notional",
                                                    lowerNotionalBounds_, "startDate", lowerNotionalBoundsDates_);
    }
    // optionality given by exercise dates, types and values
    if (!exerciseDates_.empty()) {
        XMLNode* prepayNode = doc.allocNode("Prepayment");
        XMLUtils::appendNode(swapNode, prepayNode);
        if (!noticePeriod_.empty())
            XMLUtils::addChild(doc, prepayNode, "NoticePeriod", noticePeriod_);
        if (!noticeCalendar_.empty())
            XMLUtils::addChild(doc, prepayNode, "NoticeCalendar", noticeCalendar_);
        if (!noticeConvention_.empty())
            XMLUtils::addChild(doc, prepayNode, "NoticeConvention", noticeConvention_);
        XMLNode* optionsNode = doc.allocNode("PrepaymentOptions");
        XMLUtils::appendNode(prepayNode, optionsNode);
        for (Size i = 0; i < exerciseDates_.size(); ++i) {
            XMLNode* exerciseNode = doc.allocNode("PrepaymentOption");
            XMLUtils::appendNode(optionsNode, exerciseNode);
            XMLUtils::addChild(doc, exerciseNode, "ExerciseDate", exerciseDates_.at(i));
            XMLUtils::addChild(doc, exerciseNode, "Type", exerciseTypes_.at(i));
            XMLUtils::addChild(doc, exerciseNode, "Value", exerciseValues_.at(i));
        }
    }
    // long short option flag
    XMLUtils::addChild(doc, swapNode, "OptionLongShort", optionLongShort_);
    // underlying legs
    for (Size i = 0; i < swap_.size(); i++)
        XMLUtils::appendNode(swapNode, swap_[i].toXML(doc));
    return node;
}

} // namespace data
} // namespace ore
