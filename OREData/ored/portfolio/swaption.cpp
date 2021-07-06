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

#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/instruments/rebatedexercise.hpp>

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/timer/timer.hpp>

#include <algorithm>

using boost::timer::cpu_timer;
using boost::timer::default_places;
using namespace QuantLib;
using std::lower_bound;
using std::sort;

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

    // check at least one exercise date is given
    QL_REQUIRE(!option_.exerciseDates().empty(), "No exercise dates given");

    // check if all exercise dates (adjusted by the notice period) are in the past and if so, build an expired
    // instrument
    Period noticePeriod = option_.noticePeriod().empty() ? 0 * Days : parsePeriod(option_.noticePeriod());
    Calendar noticeCal = option_.noticeCalendar().empty() ? NullCalendar() : parseCalendar(option_.noticeCalendar());
    BusinessDayConvention noticeBdc =
        option_.noticeConvention().empty() ? Unadjusted : parseBusinessDayConvention(option_.noticeConvention());
    std::vector<Date> exerciseDates(option_.exerciseDates().size());
    for (Size i = 0; i < option_.exerciseDates().size(); ++i) {
        exerciseDates.push_back(noticeCal.advance(parseDate(option_.exerciseDates()[i]), -noticePeriod, noticeBdc));
    }

    Date latestExerciseDate = *std::max_element(exerciseDates.begin(), exerciseDates.end());
    if (QuantLib::detail::simple_event(latestExerciseDate).hasOccurred()) {
        exercise_ = boost::make_shared<BermudanExercise>(exerciseDates);
        legs_ = {{boost::make_shared<QuantLib::SimpleCashFlow>(0.0, latestExerciseDate)}};
        instrument_ = boost::shared_ptr<InstrumentWrapper>(
            new VanillaInstrument(boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>(1, false)), 1.0, {}, {}));
        legCurrencies_ = {swap_[0].currency()};
        legPayers_ = {false};
        npvCurrency_ = swap_[0].currency();
        notional_ = Null<Real>();
        notionalCurrency_ = npvCurrency_;
        maturity_ = latestExerciseDate;
        return;
    }

    // build swaption with at least one alive exercise date
    bool fixedFirst = swap_[0].legType() == "Fixed";
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_[fixedFirst ? 0 : 1].concreteLegData());
    boost::shared_ptr<FloatingLegData> floatingLegData =
        boost::dynamic_pointer_cast<FloatingLegData>(swap_[fixedFirst ? 1 : 0].concreteLegData());
    underlyingIndex_ = floatingLegData->index();

    bool isNonStandard =
        (swap_[0].notionals().size() > 1 || swap_[1].notionals().size() > 1 || floatingLegData->spreads().size() > 1 ||
         floatingLegData->gearings().size() > 1 || fixedLegData->rates().size() > 1);

    Exercise::Type exerciseType = parseExerciseType(option_.style());

    // treat non standard europeans as bermudans, also if exercise fees are present
    QL_REQUIRE(exerciseType == Exercise::Bermudan || exerciseType == Exercise::European,
               "Exercise type " << option_.style() << " not implemented for Swaptions");
    if (exerciseType == Exercise::Bermudan || (isNonStandard && exerciseType == Exercise::European) ||
        !option_.exerciseFees().empty())
        buildBermudan(engineFactory);
    else
        buildEuropean(engineFactory);

    // add required fixings, we add the required fixing for the underlying swap, which might be more
    // than actually required, i.e. we are conservative here
    std::map<string,string> indexNameMapper = {{underlyingIndexQlName_, underlyingIndex_}};
    if (engineFactory->iborFallbackConfig().isIndexReplaced(underlyingIndex_)) {
        string rfrIndexName = engineFactory->iborFallbackConfig().fallbackData(underlyingIndex_).rfrIndex;
        // we don't support convention based rfr fallback indices, with ore ticket 1758 this might change
        indexNameMapper[parseIborIndex(rfrIndexName)->name()] = rfrIndexName;
    }
    addToRequiredFixings(underlyingLeg_, boost::make_shared<FixingDateGetter>(requiredFixings_, indexNameMapper));
}

namespace {
QuantLib::Settlement::Method defaultMethod(const QuantLib::Settlement::Type t) {
    if (t == QuantLib::Settlement::Physical)
        return QuantLib::Settlement::PhysicalOTC;
    else
        return QuantLib::Settlement::ParYieldCurve; // ql < 1.14 behaviour
}
} // namespace

void Swaption::buildEuropean(const boost::shared_ptr<EngineFactory>& engineFactory) {

    LOG("Building European Swaption " << id());

    // Swaption details
    Settlement::Type settleType = parseSettlementType(option_.settlement());
    Settlement::Method settleMethod = option_.settlementMethod() == ""
                                          ? defaultMethod(settleType)
                                          : parseSettlementMethod(option_.settlementMethod());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Only one exercise date expected for European Option");

    Period noticePeriod = option_.noticePeriod().empty() ? 0 * Days : parsePeriod(option_.noticePeriod());
    Calendar noticeCal = option_.noticeCalendar().empty() ? NullCalendar() : parseCalendar(option_.noticeCalendar());
    BusinessDayConvention noticeBdc =
        option_.noticeConvention().empty() ? Unadjusted : parseBusinessDayConvention(option_.noticeConvention());
    Date exDate = noticeCal.advance(parseDate(option_.exerciseDates().front()), -noticePeriod, noticeBdc);
    QL_REQUIRE(exDate >= Settings::instance().evaluationDate(), "Exercise date expected in the future.");

    boost::shared_ptr<VanillaSwap> swap = buildVanillaSwap(engineFactory, exDate);
    underlyingLeg_ = swap->floatingLeg();
    underlyingFixedLeg_ = swap->fixedLeg();

    Position::Type positionType = parsePositionType(option_.longShort());
    Real multiplier = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);

    // Check for existence of swaption engine builder.
    string tt("EuropeanSwaption");
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tt);
    QL_REQUIRE(builder, "No builder found for " << tt);
    boost::shared_ptr<EuropeanSwaptionEngineBuilder> swaptionBuilder =
        boost::dynamic_pointer_cast<EuropeanSwaptionEngineBuilder>(builder);

    exercise_ = boost::make_shared<EuropeanExercise>(exDate);

    // Build Swaption
    boost::shared_ptr<QuantLib::Swaption> swaption(new QuantLib::Swaption(swap, exercise_, settleType, settleMethod));

    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, 1.0, option_.premiumData(), -multiplier, currency,
                engineFactory, swaptionBuilder->configuration(MarketContext::pricing));

    // Set the engine.
    swaption->setPricingEngine(swaptionBuilder->engine(currency));

    // Now set the instrument wrapper, depending on delivery
    if (settleType == Settlement::Physical) {
        // tracks state for any option flavour, including physical delivery
        instrument_ = boost::shared_ptr<InstrumentWrapper>(
            new EuropeanOptionWrapper(swaption, positionType == Position::Long ? true : false, exDate,
                                      settleType == Settlement::Physical ? true : false, swap, 1.0, 1.0,
                                      additionalInstruments, additionalMultipliers));
        // maturity of underlying
        maturity_ = std::max(swap->fixedSchedule().dates().back(), swap->floatingSchedule().dates().back());
    } else {
        instrument_ = boost::shared_ptr<InstrumentWrapper>(
            new VanillaInstrument(swaption, multiplier, additionalInstruments, additionalMultipliers));
        // Align with ISDA AANA/GRID guidance as of November 2020
        // maturity_ = exDate;
        maturity_ = std::max(swap->fixedSchedule().dates().back(), swap->floatingSchedule().dates().back());
    }

    DLOG("Building European Swaption done");
}

void Swaption::buildBermudan(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Building Bermudan Swaption " << id());

    QL_REQUIRE(swap_.size() >= 1, "underlying swap must have at least 1 leg");

    string ccy_str = swap_[0].currency();
    Currency currency = parseCurrency(ccy_str);

    bool fixedFirst = swap_[0].legType() == "Fixed";
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_[fixedFirst ? 0 : 1].concreteLegData());
    boost::shared_ptr<FloatingLegData> floatingLegData =
        boost::dynamic_pointer_cast<FloatingLegData>(swap_[fixedFirst ? 1 : 0].concreteLegData());
    underlyingIndex_ = floatingLegData->index();

    bool isNonStandard =
        (swap_[0].notionals().size() > 1 || swap_[1].notionals().size() > 1 || floatingLegData->spreads().size() > 1 ||
         floatingLegData->gearings().size() > 1 || fixedLegData->rates().size() > 1);

    boost::shared_ptr<VanillaSwap> vanillaSwap;
    boost::shared_ptr<NonstandardSwap> nonstandardSwap;
    boost::shared_ptr<Swap> swap;
    if (!isNonStandard) {
        vanillaSwap = buildVanillaSwap(engineFactory);
        underlyingLeg_ = vanillaSwap->floatingLeg();
        underlyingFixedLeg_ = vanillaSwap->fixedLeg();
        swap = vanillaSwap;
    } else {
        nonstandardSwap = buildNonStandardSwap(engineFactory);
        underlyingLeg_ = nonstandardSwap->floatingLeg();
        underlyingFixedLeg_ = nonstandardSwap->fixedLeg();
        swap = nonstandardSwap;
    }

    DLOG("Swaption::Build(): Underlying Start = " << QuantLib::io::iso_date(swap->startDate()));

    ExerciseBuilder eb(option_, {swap->leg(1)});
    QL_REQUIRE(!eb.noticeDates().empty(), "BermudanSwaption: Swaption does not have any alive exercise dates");
    exercise_ = eb.exercise();
    maturity_ = eb.noticeDates().back();

    Settlement::Type delivery = parseSettlementType(option_.settlement());
    Settlement::Method deliveryMethod =
        option_.settlementMethod() == "" ? defaultMethod(delivery) : parseSettlementMethod(option_.settlementMethod());

    if (delivery == Settlement::Cash && deliveryMethod == Settlement::ParYieldCurve)
        WLOG("Cash-settled Bermudan Swaption (id = "
             << id() << ") with ParYieldCurve settlement method not supported by Lgm engine. "
             << "Approximate pricing using CollateralizedCashPrice pricing methodology");

    // Build swaption
    DLOG("Build Swaption instrument");
    boost::shared_ptr<QuantLib::Instrument> swaption;
    if (isNonStandard) {
        swaption = boost::shared_ptr<QuantLib::Instrument>(
            new QuantLib::NonstandardSwaption(nonstandardSwap, exercise_, delivery, deliveryMethod));
        // workaround for missing registration in QL versions < 1.13
        swaption->registerWithObservables(nonstandardSwap);
    } else
        swaption = boost::shared_ptr<QuantLib::Instrument>(
            new QuantLib::Swaption(vanillaSwap, exercise_, delivery, deliveryMethod));

    QuantLib::Position::Type positionType = parsePositionType(option_.longShort());
    if (delivery == Settlement::Physical)
        maturity_ = swap->maturityDate();

    DLOG("Get/Build Bermudan Swaption engine");
    cpu_timer timer;

    string tt("BermudanSwaption");
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tt);
    QL_REQUIRE(builder, "No builder found for " << tt);
    boost::shared_ptr<BermudanSwaptionEngineBuilder> swaptionBuilder =
        boost::dynamic_pointer_cast<BermudanSwaptionEngineBuilder>(builder);

    // determine strikes for calibration basket (simple approach, a la summit)
    std::vector<Real> strikes(eb.noticeDates().size(), Null<Real>());
    if (!isNonStandard) {
        Real tmp = vanillaSwap->fixedRate() - vanillaSwap->spread();
        std::fill(strikes.begin(), strikes.end(), tmp);
        DLOG("calibration strike is " << tmp << "(fixed rate " << vanillaSwap->fixedRate() << ", spread "
                                      << vanillaSwap->spread() << ")");
    } else {
        for (Size i = 0; i < eb.noticeDates().size(); ++i) {
            const Schedule& fix = nonstandardSwap->fixedSchedule();
            const Schedule& flt = nonstandardSwap->floatingSchedule();
            Size fixIdx =
                std::lower_bound(fix.dates().begin(), fix.dates().end(), eb.noticeDates()[i]) - fix.dates().begin();
            Size fltIdx =
                std::lower_bound(flt.dates().begin(), flt.dates().end(), eb.noticeDates()[i]) - flt.dates().begin();
            // play safe
            fixIdx = std::min(fixIdx, nonstandardSwap->fixedRate().size() - 1);
            fltIdx = std::min(fltIdx, nonstandardSwap->spreads().size() - 1);
            strikes[i] = nonstandardSwap->fixedRate()[fixIdx] - nonstandardSwap->spreads()[fltIdx];
            DLOG("calibration strike for ex date "
                 << QuantLib::io::iso_date(eb.noticeDates()[i]) << " is " << strikes[i] << " (fixed rate "
                 << nonstandardSwap->fixedRate()[fixIdx] << ", spread " << nonstandardSwap->spreads()[fltIdx] << ")");
        }
    }

    boost::shared_ptr<PricingEngine> engine =
        swaptionBuilder->engine(id(), isNonStandard, ccy_str, eb.noticeDates(), swap->maturityDate(), strikes);

    timer.stop();
    DLOG("Swaption model calibration time: " << timer.format(default_places, "%w") << " s");

    swaption->setPricingEngine(engine);

    // timer.restart();
    // Real npv = swaption->NPV();
    // Real pt = timer.elapsed();
    // DLOG("Swaption pricing time: " << pt*1000 << " ms");

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());
    boost::shared_ptr<PricingEngine> swapEngine = swapBuilder->engine(currency);

    std::vector<boost::shared_ptr<Instrument>> underlyingSwaps =
        buildUnderlyingSwaps(swapEngine, swap, eb.noticeDates());

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Real multiplier = positionType == Position::Long ? 1.0 : -1.0;
    addPremiums(additionalInstruments, additionalMultipliers, 1.0, option_.premiumData(), -multiplier, currency,
                engineFactory, swaptionBuilder->configuration(MarketContext::pricing));

    // instrument_ = boost::shared_ptr<InstrumentWrapper> (new VanillaInstrument (swaption, multiplier));
    instrument_ = boost::shared_ptr<InstrumentWrapper>(
        new BermudanOptionWrapper(swaption, positionType == Position::Long ? true : false, eb.noticeDates(),
                                  delivery == Settlement::Physical ? true : false, underlyingSwaps, 1.0, 1.0,
                                  additionalInstruments, additionalMultipliers));

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
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_[fixedLegIndex].concreteLegData());
    boost::shared_ptr<FloatingLegData> floatingLegData =
        boost::dynamic_pointer_cast<FloatingLegData>(swap_[floatingLegIndex].concreteLegData());

    QL_REQUIRE(fixedLegData->rates().size() == 1, "Vanilla Swaption: constant rate required");
    QL_REQUIRE(floatingLegData->spreads().size() <= 1, "Vanilla Swaption: constant spread required");

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());

    // Get Trade details
    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);
    Real nominal = swap_[0].notionals().back();

    Real rate = fixedLegData->rates().front();
    Real spread = floatingLegData->spreads().empty() ? 0.0 : floatingLegData->spreads().front();
    string indexName = floatingLegData->index();

    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());

    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(indexName, swapBuilder->configuration(MarketContext::pricing));
    underlyingIndexQlName_ = index->name();
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());

    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());

    VanillaSwap::Type type = swap_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    // We treat overnight and bma indices approximately as ibor indices and warn about this in the log
    if (boost::dynamic_pointer_cast<OvernightIndex>(*index) ||
        boost ::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(*index))
        ALOG("Swaption trade " << id() << " on ON or BMA index '" << underlyingIndex_
                               << "' built, will treat the index approximately as an ibor index");

    // only take into account accrual periods with start date on or after first exercise date (if given)
    if (firstExerciseDate != Null<Date>()) {
        std::vector<Date> fixDates = fixedSchedule.dates();
        auto it1 = std::lower_bound(fixDates.begin(), fixDates.end(), firstExerciseDate);
        fixDates.erase(fixDates.begin(), it1);
        // check we have at least 1 to stop set fault on vector(fixDates.size() - 1, true) but maybe check should be 2
        QL_REQUIRE(fixDates.size() >= 2,
                   "Not enough schedule dates are left in Swaption fixed leg (check exercise dates)");
        fixedSchedule = Schedule(fixDates, fixedSchedule.calendar(), Unadjusted, boost::none, boost::none, boost::none,
                                 boost::none, std::vector<bool>(fixDates.size() - 1, true));
        std::vector<Date> floatingDates = floatingSchedule.dates();
        auto it2 = std::lower_bound(floatingDates.begin(), floatingDates.end(), firstExerciseDate);
        floatingDates.erase(floatingDates.begin(), it2);
        QL_REQUIRE(floatingDates.size() >= 2,
                   "Not enough schedule dates are left in Swaption floating leg (check exercise dates)");
        floatingSchedule = Schedule(floatingDates, floatingSchedule.calendar(), Unadjusted, boost::none, boost::none,
                                    boost::none, boost::none, std::vector<bool>(floatingDates.size() - 1, true));
    }

    // Build a vanilla (bullet) swap underlying
    boost::shared_ptr<VanillaSwap> swap(new VanillaSwap(type, nominal, fixedSchedule, rate, fixedDayCounter,
                                                        floatingSchedule, *index, spread, floatingDayCounter,
                                                        paymentConvention));

    swap->setPricingEngine(swapBuilder->engine(currency));

    // Set other ore::data::Trade details
    npvCurrency_ = ccy;
    notional_ = Null<Real>();
    notionalCurrency_ = ccy;
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
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_[fixedLegIndex].concreteLegData());
    boost::shared_ptr<FloatingLegData> floatingLegData =
        boost::dynamic_pointer_cast<FloatingLegData>(swap_[floatingLegIndex].concreteLegData());

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());

    // Get Trade details
    string ccy = swap_[0].currency();
    Currency currency = parseCurrency(ccy);
    Schedule fixedSchedule = makeSchedule(swap_[fixedLegIndex].schedule());
    Schedule floatingSchedule = makeSchedule(swap_[floatingLegIndex].schedule());
    vector<Real> fixedNominal = buildScheduledVectorNormalised(
        swap_[fixedLegIndex].notionals(), swap_[fixedLegIndex].notionalDates(), fixedSchedule, (Real)Null<Real>());
    vector<Real> floatNominal =
        buildScheduledVectorNormalised(swap_[floatingLegIndex].notionals(), swap_[floatingLegIndex].notionalDates(),
                                       floatingSchedule, (Real)Null<Real>());
    vector<Real> fixedRate = buildScheduledVectorNormalised(fixedLegData->rates(), fixedLegData->rateDates(),
                                                            fixedSchedule, (Real)Null<Real>());
    vector<Real> spreads = buildScheduledVectorNormalised(floatingLegData->spreads(), floatingLegData->spreadDates(),
                                                          floatingSchedule, (Real)Null<Real>());
    // gearings are optional, i.e. may be empty
    vector<Real> gearings = buildScheduledVectorNormalised(floatingLegData->gearings(), floatingLegData->gearingDates(),
                                                           floatingSchedule, 1.0);
    string indexName = floatingLegData->index();
    DayCounter fixedDayCounter = parseDayCounter(swap_[fixedLegIndex].dayCounter());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(indexName, swapBuilder->configuration(MarketContext::pricing));
    underlyingIndexQlName_ = index->name();
    DayCounter floatingDayCounter = parseDayCounter(swap_[floatingLegIndex].dayCounter());
    BusinessDayConvention paymentConvention = parseBusinessDayConvention(swap_[floatingLegIndex].paymentConvention());

    // We treat overnight and bma indices approximately as ibor indices and warn about this in the log
    if (boost::dynamic_pointer_cast<OvernightIndex>(*index) ||
        boost ::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(*index))
        ALOG("Swaption trade " << id() << " on ON or BMA index '" << underlyingIndex_
                               << "' built, will treat the index approximately as an ibor index");

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
    notional_ = Null<Real>();
    notionalCurrency_ = ccy;
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
            // if in the swap data the fixed leg is the first component, the legs in the underlying
            // swap match the order of the legs in the swap data, otherwise they are swapped
            payer[j] = swap_[0].legType() == "Fixed" ? swap_[j].isPayer() : swap_[1 - j].isPayer();
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

QuantLib::Real Swaption::notional() const {
    // So far we cover single currency swaptions with fixed/float underlying swaps only.
    return std::max(currentNotional(underlyingLeg_), currentNotional(underlyingFixedLeg_));
}

std::string Swaption::notionalCurrency() const {
    // So far we cover single currency swaptions only,
    // but leave this function override for the cross currency case later
    return notionalCurrency_;
}

const std::map<std::string,boost::any>& Swaption::additionalData() const {
    Size numLegs = swap_.size();
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    for (Size i = 0; i < numLegs; ++i) {
        string legID = to_string(i+1);
        additionalData_["legType[" + legID + "]"] = swap_[i].legType();
        additionalData_["isPayer[" + legID + "]"] = swap_[i].isPayer();
        additionalData_["notionalCurrency[" + legID + "]"] = swap_[i].currency();
        for (Size j = 0; j < legs_[i].size(); ++j) {
            boost::shared_ptr<CashFlow> flow = legs_[i][j];
            // pick flow with earliest future payment date on this leg
            if (flow->date() > asof) {
                additionalData_["amount[" + legID + "]"] = flow->amount();
                additionalData_["paymentDate[" + legID + "]"] = to_string(flow->date());
                boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(flow);
                if (coupon) {
                    additionalData_["currentNotional[" + legID + "]"] = coupon->nominal();
                    additionalData_["rate[" + legID + "]"] = coupon->rate();
                    boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(flow);
                    if (frc) {
                        additionalData_["index[" + legID + "]"] = frc->index()->name();
                        additionalData_["spread[" + legID + "]"] = frc->spread();                        
                    }
                }
                break;
            }
        }
        if (legs_[i].size() > 0) {
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[i][0]);
            if (coupon)
                additionalData_["originalNotional[" + legID + "]"] = coupon->nominal();
        }
    }
    return additionalData_;
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
} // namespace data
} // namespace ore
