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

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/exercise.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/instruments/nonstandardswaption.hpp>

#include <ored/utilities/log.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/optionwrapper.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/timer.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void Swaption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    QL_REQUIRE(swap_.size() == 2, "underlying swap must have 2 legs");

    // we can assume 2 legs now
    bool isCrossCcy = swap_[0].currency() != swap_[1].currency();

    bool isFixedFloating = (swap_[0].legType() == "Fixed" && swap_[1].legType() == "Floating") ||
                           (swap_[1].legType() == "Fixed" && swap_[0].legType() == "Floating");

    // First elimate these
    QL_REQUIRE(!isCrossCcy, "Cross Currency Swaptions not supported");
    QL_REQUIRE(isFixedFloating, "Basis Swaptions not supported");

    bool isNonStandard =
        (swap_[0].notionals().size() > 1 || swap_[1].notionals().size() > 1 ||
         swap_[0].floatingLegData().spreads().size() > 1 || swap_[1].floatingLegData().spreads().size() > 1 ||
         swap_[0].floatingLegData().gearings().size() > 1 || swap_[1].floatingLegData().gearings().size() > 1 ||
         swap_[0].fixedLegData().rates().size() > 1 || swap_[1].fixedLegData().rates().size() > 1);

    Exercise::Type exerciseType = parseExerciseType(option_.style());
    // QL_REQUIRE(!(isNonStandard && exerciseType == Exercise::European),
    //            "nonstandard European Swaptions not implemented");
    // treat non standard europeans as bermudans
    if (exerciseType == Exercise::Bermudan || (isNonStandard && exerciseType == Exercise::European))
        buildBermudan(engineFactory);
    else if (exerciseType == Exercise::European)
        buildEuropean(engineFactory);
    else
        QL_FAIL("Exercise type " << option_.style() << " not implemented for Swaptions");
}

void Swaption::buildEuropean(const boost::shared_ptr<EngineFactory>& engineFactory) {

    LOG("Building European Swaption " << id());

    // Swaption details
    Settlement::Type settleType = parseSettlementType(option_.settlement());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Only one exercise date expected for European Option");
    Date exDate = parseDate(option_.exerciseDates().front());
    QL_REQUIRE(exDate >= Settings::instance().evaluationDate(), "Exercise date expected in the future.");

    boost::shared_ptr<VanillaSwap> swap = buildVanillaSwap(engineFactory, exDate);

    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);

    boost::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

    // Build Swaption
    boost::shared_ptr<QuantLib::Swaption> swaption(new QuantLib::Swaption(swap, exercise, settleType));

    // Add Engine
    string tt("EuropeanSwaption");
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tt);
    QL_REQUIRE(builder, "No builder found for " << tt);
    boost::shared_ptr<EuropeanSwaptionEngineBuilder> swaptionBuilder =
        boost::dynamic_pointer_cast<EuropeanSwaptionEngineBuilder>(builder);
    swaption->setPricingEngine(swaptionBuilder->engine(currency));

    Position::Type positionType = parsePositionType(option_.longShort());
    Real multiplier = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    // Now set the instrument wrapper, depending on delivery
    if (settleType == Settlement::Physical) {
        // tracks state for any option flavour, including physical delivery
        instrument_ = boost::shared_ptr<InstrumentWrapper>(
            new EuropeanOptionWrapper(swaption, positionType == Position::Long ? true : false, exDate,
                                      settleType == Settlement::Physical ? true : false, swap));
        // maturity of underlying
        maturity_ = std::max(swap->fixedSchedule().dates().back(), swap->floatingSchedule().dates().back());
    } else {
        instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(swaption, multiplier));
        maturity_ = exDate;
    }

    DLOG("Building European Swaption done");
}

void Swaption::buildBermudan(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Building Bermudan Swaption " << id());

    QL_REQUIRE(swap_.size() >= 1, "underlying swap must have at least 1 leg");

    string ccy_str = swap_[0].currency();
    Currency currency = parseCurrency(ccy_str);

    bool isNonStandard =
        (swap_[0].notionals().size() > 1 || swap_[1].notionals().size() > 1 ||
         swap_[0].floatingLegData().spreads().size() > 1 || swap_[1].floatingLegData().spreads().size() > 1 ||
         swap_[0].floatingLegData().gearings().size() > 1 || swap_[1].floatingLegData().gearings().size() > 1 ||
         swap_[0].fixedLegData().rates().size() > 1 || swap_[1].fixedLegData().rates().size() > 1);

    boost::shared_ptr<VanillaSwap> vanillaSwap;
    boost::shared_ptr<NonstandardSwap> nonstandardSwap;
    boost::shared_ptr<Swap> swap;
    if (!isNonStandard) {
        vanillaSwap = buildVanillaSwap(engineFactory);
        swap = vanillaSwap;
    } else {
        nonstandardSwap = buildNonStandardSwap(engineFactory);
        swap = nonstandardSwap;
    }

    DLOG("Swaption::Build(): Underlying Start = " << QuantLib::io::iso_date(swap->startDate()));
    DLOG("Swaption::Build(): Underlying NPV = " << swap->NPV());
    // DLOG("Swaption::Build(): Fair Swap Rate = " << swap->fairRate());

    // build exercise
    std::vector<QuantLib::Date> exDates;
    for (Size i = 0; i < option_.exerciseDates().size(); i++) {
        Date exDate = ore::data::parseDate(option_.exerciseDates()[i]);
        if (exDate > Settings::instance().evaluationDate())
            exDates.push_back(exDate);
    }
    QL_REQUIRE(exDates.size() > 0, "Bermudan Swaption does not have any alive exercise dates");
    maturity_ = exDates.back();
    boost::shared_ptr<Exercise> exercise(new BermudanExercise(exDates));

    Settlement::Type delivery = parseSettlementType(option_.settlement());

    if (delivery != Settlement::Physical)
        WLOG("Cash-settled Bermudan Swaption (id = "
             << id() << ") not supported by Lgm engine. "
             << "Approximate pricing using physical settlement pricing methodology");

    // Build swaption
    DLOG("Build Swaption instrument");
    boost::shared_ptr<QuantLib::Instrument> swaption;
    if (isNonStandard)
        swaption =
            boost::shared_ptr<QuantLib::Instrument>(new QuantLib::NonstandardSwaption(nonstandardSwap, exercise));
    else
        swaption = boost::shared_ptr<QuantLib::Instrument>(new QuantLib::Swaption(vanillaSwap, exercise));

    QuantLib::Position::Type positionType = parsePositionType(option_.longShort());
    if (delivery == Settlement::Physical)
        maturity_ = swap->maturityDate();

    DLOG("Get/Build Bermudan Swaption engine");
    boost::timer timer;

    string tt("BermudanSwaption");
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tt);
    QL_REQUIRE(builder, "No builder found for " << tt);
    boost::shared_ptr<BermudanSwaptionEngineBuilder> swaptionBuilder =
        boost::dynamic_pointer_cast<BermudanSwaptionEngineBuilder>(builder);

    boost::shared_ptr<PricingEngine> engine =
        swaptionBuilder->engine(id(), isNonStandard, ccy_str, exDates, swap->maturityDate(),
                                isNonStandard ? 0.0 : vanillaSwap->fixedRate()); // FIXME

    Real ct = timer.elapsed();
    DLOG("Swaption model calibration time: " << ct * 1000 << " ms");

    swaption->setPricingEngine(engine);

    // timer.restart();
    // Real npv = swaption->NPV();
    // Real pt = timer.elapsed();
    // DLOG("Swaption pricing time: " << pt*1000 << " ms");

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());
    boost::shared_ptr<PricingEngine> swapEngine = swapBuilder->engine(currency);

    std::vector<boost::shared_ptr<Instrument>> underlyingSwaps = buildUnderlyingSwaps(swapEngine, swap, exDates);

    // instrument_ = boost::shared_ptr<InstrumentWrapper> (new VanillaInstrument (swaption, multiplier));
    instrument_ = boost::shared_ptr<InstrumentWrapper>(
        new BermudanOptionWrapper(swaption, positionType == Position::Long ? true : false, exDates,
                                  delivery == Settlement::Physical ? true : false, underlyingSwaps));

    DLOG("Building Bermudan Swaption done");
}

boost::shared_ptr<VanillaSwap> Swaption::buildVanillaSwap(const boost::shared_ptr<EngineFactory>& engineFactory,
                                                          const Date& firstExerciseDate) {
    // Limitation to vanilla for now
    QL_REQUIRE(swap_.size() == 2 && swap_[0].currency() == swap_[1].currency() &&
                   swap_[0].isPayer() != swap_[1].isPayer() && swap_[0].notionals() == swap_[1].notionals() &&
                   swap_[0].notionals().size() == 1,
               "Not a Vanilla Swap");

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

    QL_REQUIRE(swap_[fixedLegIndex].fixedLegData().rates().size() == 1, "Vanilla Swaption: constant rate required");
    QL_REQUIRE(swap_[floatingLegIndex].floatingLegData().spreads().size() == 1,
               "Vanilla Swaption: constant spread required");

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());

    // Get Trade details
    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);
    Real nominal = swap_[0].notionals().back();

    Real rate = swap_[fixedLegIndex].fixedLegData().rates().front();
    Real spread = swap_[floatingLegIndex].floatingLegData().spreads().front();
    string indexName = swap_[floatingLegIndex].floatingLegData().index();

    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());

    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(indexName, swapBuilder->configuration(MarketContext::pricing));
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());

    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());

    VanillaSwap::Type type = swap_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    // only take into account accrual periods with start date on or after first exercise date (if given)
    // if (firstExerciseDate != Null<Date>()) {
    //     std::vector<Date> fixDates = fixedSchedule.dates();
    //     auto it1 = std::lower_bound(fixDates.begin(), fixDates.end(), firstExerciseDate);
    //     fixDates.erase(fixDates.begin(), it1);
    //     fixedSchedule = Schedule(fixDates, fixedSchedule.calendar());
    //     std::vector<Date> floatingDates = floatingSchedule.dates();
    //     auto it2 = std::lower_bound(floatingDates.begin(), floatingDates.end(), firstExerciseDate);
    //     floatingDates.erase(floatingDates.begin(), it2);
    //     floatingSchedule = Schedule(floatingDates, floatingSchedule.calendar());
    // }

    // Build a vanilla (bullet) swap underlying
    boost::shared_ptr<VanillaSwap> swap(new VanillaSwap(type, nominal, fixedSchedule, rate, fixedDayCounter,
                                                        floatingSchedule, *index, spread, floatingDayCounter,
                                                        paymentConvention));

    swap->setPricingEngine(swapBuilder->engine(currency));

    // Set other ore::data::Trade details
    npvCurrency_ = ccy;
    notional_ = nominal;
    legCurrencies_ = vector<string>(2, ccy);
    legs_.push_back(swap->fixedLeg());
    legs_.push_back(swap->floatingLeg());
    legPayers_.push_back(swap_[fixedLegIndex].isPayer());
    legPayers_.push_back(swap_[floatingLegIndex].isPayer());

    return swap;
}

boost::shared_ptr<NonstandardSwap>
Swaption::buildNonStandardSwap(const boost::shared_ptr<EngineFactory>& engineFactory) {
    QL_REQUIRE(swap_.size() == 2, "Two swap legs expected");
    QL_REQUIRE(swap_[0].currency() == swap_[1].currency(), "single currency swap expected");
    QL_REQUIRE(swap_[0].isPayer() != swap_[1].isPayer(), "pay and receive leg expected");

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

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());

    // Get Trade details
    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);
    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    vector<Real> fixedNominal = buildScheduledVectorNormalised(swap_[fixedLegIndex].notionals(),
                                                               swap_[fixedLegIndex].notionalDates(), fixedSchedule);
    vector<Real> floatNominal = buildScheduledVectorNormalised(
        swap_[floatingLegIndex].notionals(), swap_[floatingLegIndex].notionalDates(), floatingSchedule);
    vector<Real> fixedRate = buildScheduledVectorNormalised(
        swap_[fixedLegIndex].fixedLegData().rates(), swap_[fixedLegIndex].fixedLegData().rateDates(), fixedSchedule);
    vector<Real> spreads =
        buildScheduledVectorNormalised(swap_[floatingLegIndex].floatingLegData().spreads(),
                                       swap_[floatingLegIndex].floatingLegData().spreadDates(), floatingSchedule);
    // gearings are optional, i.e. may be empty
    vector<Real> gearings =
        buildScheduledVectorNormalised(swap_[floatingLegIndex].floatingLegData().gearings(),
                                       swap_[floatingLegIndex].floatingLegData().gearingDates(), floatingSchedule, 1.0);
    string indexName = swap_[floatingLegIndex].floatingLegData().index();
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(indexName, swapBuilder->configuration(MarketContext::pricing));
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());
    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());

    VanillaSwap::Type type = swap_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    // Build a vanilla (bullet) swap underlying
    boost::shared_ptr<NonstandardSwap> swap(new NonstandardSwap(type, fixedNominal, floatNominal, fixedSchedule,
                                                                fixedRate, fixedDayCounter, floatingSchedule, *index,
                                                                gearings, spreads, floatingDayCounter,
                                                                false, // no intermediate notional exchanges
                                                                false, // no final notional exchanges
                                                                paymentConvention));

    swap->setPricingEngine(swapBuilder->engine(currency));

    // Set other ore::data::Trade details
    npvCurrency_ = ccy;
    notional_ = std::max(currentNotional(swap->fixedLeg()), currentNotional(swap->floatingLeg()));
    legCurrencies_ = vector<string>(2, ccy);
    legs_.push_back(swap->fixedLeg());
    legs_.push_back(swap->floatingLeg());
    legPayers_.push_back(swap_[fixedLegIndex].isPayer());
    legPayers_.push_back(swap_[floatingLegIndex].isPayer());

    return swap;
}

std::vector<boost::shared_ptr<Instrument>>
Swaption::buildUnderlyingSwaps(const boost::shared_ptr<PricingEngine>& swapEngine,
                               const boost::shared_ptr<Swap>& underlyingSwap, const std::vector<Date>& exerciseDates) {
    std::vector<boost::shared_ptr<Instrument>> swaps;
    for (Size i = 0; i < exerciseDates.size(); ++i) {
        std::vector<Leg> legs(2);
        std::vector<bool> payer(2);
        for (Size j = 0; j < legs.size(); ++j) {
            legs[j] = underlyingSwap->leg(j);
            payer[j] = underlyingSwap->legNPV(j) < 0.0 ? true : false;
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs[j].front());
            while (legs[j].size() > 0 && coupon->accrualStartDate() < exerciseDates[i]) {
                legs[j].erase(legs[j].begin());
                coupon = boost::dynamic_pointer_cast<Coupon>(legs[j].front());
            }
        }
        boost::shared_ptr<Swap> newSwap(new Swap(legs, payer));
        newSwap->setPricingEngine(swapEngine);
        swaps.push_back(newSwap);
        if (legs[0].size() > 0 && legs[1].size() > 0) {
            boost::shared_ptr<Coupon> coupon1 = boost::dynamic_pointer_cast<Coupon>(legs[0].front());
            boost::shared_ptr<Coupon> coupon2 = boost::dynamic_pointer_cast<Coupon>(legs[1].front());
            DLOG("Added underlying Swap start " << QuantLib::io::iso_date(coupon1->accrualStartDate()) << " "
                                                << QuantLib::io::iso_date(coupon2->accrualStartDate()) << " "
                                                << "exercise " << QuantLib::io::iso_date(exerciseDates[i]));
        } else {
            WLOG("Added underlying Swap with at least one empty leg for exercise "
                 << QuantLib::io::iso_date(exerciseDates[i]) << "!");
        }
    }
    return swaps;
}

void Swaption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "SwaptionData");
    option_.fromXML(XMLUtils::getChildNode(swapNode, "OptionData"));
    swap_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        swap_.push_back(ld);
    }
}

XMLNode* Swaption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swaptionNode = doc.allocNode("SwaptionData");
    XMLUtils::appendNode(node, swaptionNode);

    XMLUtils::appendNode(swaptionNode, option_.toXML(doc));
    for (Size i = 0; i < swap_.size(); i++)
        XMLUtils::appendNode(swaptionNode, swap_[i].toXML(doc));

    return node;
}
}
}
