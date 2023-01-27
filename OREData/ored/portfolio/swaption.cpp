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

#include <qle/instruments/multilegoption.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/indexnametranslator.hpp>

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

namespace {
QuantLib::Settlement::Method defaultSettlementMethod(const QuantLib::Settlement::Type t) {
    if (t == QuantLib::Settlement::Physical)
        return QuantLib::Settlement::PhysicalOTC;
    else
        return QuantLib::Settlement::ParYieldCurve; // ql < 1.14 behaviour
}
} // namespace

void Swaption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // build underlying swap and copy its required fixings

    DLOG("Swaption::build() for " << id() << ": build underlying swap");

    underlying_ = boost::make_shared<ore::data::Swap>(Envelope(), legData_);
    underlying_->build(engineFactory);
    requiredFixings_.addData(underlying_->requiredFixings());

    // build the exercise and parse some fields

    DLOG("Swaption::build() for " << id() << ": build exercise");

    exerciseBuilder_ = boost::make_shared<ExerciseBuilder>(optionData_, underlying_->legs());

    exerciseType_ = parseExerciseType(optionData_.style());
    settlementType_ = parseSettlementType(optionData_.settlement());
    settlementMethod_ = optionData_.settlementMethod() == "" ? defaultSettlementMethod(settlementType_)
                                                             : parseSettlementMethod(optionData_.settlementMethod());
    positionType_ = parsePositionType(optionData_.longShort());

    // determine the type: isCrossCcy, isOis, isBma, isStandard

    std::set<std::string> legTypes;
    for (auto const& l : legData_) {
        legTypes.insert(l.legType());
    }

    bool isCrossCcy = false;
    for (Size i = 1; i < legData_.size(); ++i) {
        isCrossCcy = isCrossCcy || legData_[0].currency() != legData_[1].currency();
    }

    bool isOis = false, isBma = false;
    for (auto const& l : legData_) {
        if (auto fld = boost::dynamic_pointer_cast<FloatingLegData>(l.concreteLegData())) {
            auto ind =
                *engineFactory->market()->iborIndex(fld->index(), engineFactory->configuration(MarketContext::pricing));
            isOis = isOis || (boost::dynamic_pointer_cast<OvernightIndex>(ind) != nullptr);
            isBma = isBma || (boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(ind) != nullptr);
        }
    }

    bool isStandard = true;
    isStandard = isStandard && (legData_.size() == 2 && legTypes == std::set<std::string>{"Fixed", "Floating"} &&
                                !isOis && !isBma && !isCrossCcy && legData_[0].isPayer() != legData_[1].isPayer());
    Real notional = Null<Real>(), fixedRate = Null<Real>(), gearing = Null<Real>(), spread = Null<Real>();
    for (auto const& l : underlying_->legs()) {
        for (auto const& c : l) {
            if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
                // only constant notional allowed
                if (notional == Null<Real>())
                    notional = cpn->nominal();
                else
                    isStandard = isStandard && close_enough(notional, cpn->nominal());
                if (auto f = boost::dynamic_pointer_cast<FixedRateCoupon>(c)) {
                    // only constant fixed rate allowed
                    if (fixedRate == Null<Real>())
                        fixedRate = f->rate();
                    else
                        isStandard = isStandard && close_enough(fixedRate, f->rate());
                } else if (auto f = boost::dynamic_pointer_cast<IborCoupon>(c)) {
                    // in arrears not allowed
                    isStandard = isStandard && !f->isInArrears();
                    // only constant spread allowed
                    if (spread == Null<Real>())
                        spread = f->spread();
                    else
                        isStandard = isStandard && close_enough(spread, f->spread());
                    // only constant gearing allowed
                    if (gearing == Null<Real>())
                        gearing = f->gearing();
                    else
                        isStandard = isStandard && close_enough(gearing, f->gearing());
                } else {
                    // no other coupon types allowed
                    isStandard = false;
                }
            } else {
                // no cashflows != coupon allowed
                isStandard = false;
            }
        }
    }

    // check for zero notional, this is not handled well in the European engine, so we switch to Bermudan in this case

    if (isStandard && close_enough(notional, 0.0)) {
        DLOG("Swaption::build() found zero notional, set isStandard := false to ensure valid pricing");
        isStandard = false;
    }

    DLOG("Swaption::build() for " << id() << ": type: isCrossCcy = " << std::boolalpha << isCrossCcy
                                  << ", isOis = " << isOis << ", isBma = " << isBma << ", isStandard = " << isStandard);

    // we do not support xccy swaptions currently

    QL_REQUIRE(!isCrossCcy, "Cross Currency Swaptions are not supported at the moment.");

    // fill currencies and set notional to null (will be retrieved via notional())

    npvCurrency_ = notionalCurrency_ = "USD"; // only if no legs are given, not relevant in this case

    if (!legData_.empty()) {
        npvCurrency_ = notionalCurrency_ = legData_[0].currency();
    }

    notional_ = Null<Real>();

    // if we do not have an active exercise as of today, or no underlying legs we build an expired dummy instrument
    // FIXME this should be handled in the engines, it's not critical though, since we always simulate into the future

    Date today = Settings::instance().evaluationDate();

    if (exerciseBuilder_->exercise() == nullptr || exerciseBuilder_->exercise()->dates().empty() ||
        exerciseBuilder_->exercise()->dates().back() <= today || legData_.empty()) {
        legs_ = {{boost::make_shared<QuantLib::SimpleCashFlow>(0.0, today)}};
        instrument_ = boost::make_shared<VanillaInstrument>(
            boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>(1, false)), 1.0,
            std::vector<boost::shared_ptr<QuantLib::Instrument>>{}, std::vector<Real>{});
        legCurrencies_ = {npvCurrency_};
        legPayers_ = {false};
        notional_ = Null<Real>();
        maturity_ = today;
        return;
    }

    // fill legs, only include coupons after first exercise

    legCurrencies_ = underlying_->legCurrencies();
    legPayers_ = underlying_->legPayers();
    legs_.clear();
    Date firstExerciseDate = exerciseBuilder_->exercise()->dates().front();
    for (auto const& l : underlying_->legs()) {
        legs_.push_back(Leg());
        for (auto const& c : l) {
            if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
                if (firstExerciseDate <= cpn->accrualStartDate()) {
                    legs_.back().push_back(c);
                }
            } else if (firstExerciseDate <= c->date()) {
                legs_.back().push_back(c);
            }
        }
    }

    // build a Euroepan or Bermudan swaption

    if (exerciseType_ == Exercise::European && isStandard)
        buildEuropean(engineFactory);
    else
        buildBermudan(engineFactory);

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Swaption");  
    additionalData_["isdaTransaction"] = string("");  
}

void Swaption::buildEuropean(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("Building European Swaption " << id());

    boost::shared_ptr<VanillaSwap> swap = buildVanillaSwap(engineFactory);

    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("EuropeanSwaption");
    boost::shared_ptr<EuropeanSwaptionEngineBuilder> swaptionBuilder =
        boost::dynamic_pointer_cast<EuropeanSwaptionEngineBuilder>(builder);
    QL_REQUIRE(swaptionBuilder != nullptr, "internal error: could not cast to EuropeanSwaptionEngineBuilder");

    boost::shared_ptr<QuantLib::Swaption> swaption =
        boost::make_shared<QuantLib::Swaption>(swap, exerciseBuilder_->exercise(), settlementType_, settlementMethod_);

    Currency ccy = parseCurrency(npvCurrency_);

    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, 1.0, optionData_.premiumData(),
                                       positionType_ == Position::Long ? -1.0 : 1.0, ccy, engineFactory,
                                       swaptionBuilder->configuration(MarketContext::pricing));

    swaption->setPricingEngine(
        swaptionBuilder->engine(IndexNameTranslator::instance().oreName(swap->iborIndex()->name())));

    if (settlementType_ == Settlement::Physical) {
        instrument_ = boost::make_shared<EuropeanOptionWrapper>(
            swaption, positionType_ == Position::Long ? true : false, exerciseBuilder_->exercise()->dates().back(),
            settlementType_ == Settlement::Physical ? true : false, swap, 1.0, 1.0, additionalInstruments,
            additionalMultipliers);
        maturity_ = std::max(swap->fixedSchedule().dates().back(), swap->floatingSchedule().dates().back());
    } else {
        instrument_ = boost::make_shared<VanillaInstrument>(swaption, positionType_ == Position::Long ? 1.0 : -1.0,
                                                            additionalInstruments, additionalMultipliers);
        // Align with ISDA AANA/GRID guidance as of November 2020
        // maturity_ = exDate;
        maturity_ = std::max(swap->fixedSchedule().dates().back(), swap->floatingSchedule().dates().back());
    }

    maturity_ = std::max(maturity_, lastPremiumDate);

    DLOG("Building European Swaption done");
}

void Swaption::buildBermudan(const boost::shared_ptr<EngineFactory>& engineFactory) {

    if (settlementType_ == Settlement::Physical)
        maturity_ = underlying_->maturity();
    else
        maturity_ = exerciseBuilder_->noticeDates().back();

    if (settlementType_ == Settlement::Cash && settlementMethod_ == Settlement::ParYieldCurve)
        WLOG("Cash-settled Bermudan Swaption (id = "
             << id() << ") with ParYieldCurve settlement method not supported by Lgm engine. "
             << "Approximate pricing using CollateralizedCashPrice pricing methodology");

    std::vector<QuantLib::Currency> ccys;
    for (auto const& c : underlying_->legCurrencies())
        ccys.push_back(parseCurrency(c));
    auto swaption =
        boost::make_shared<QuantExt::MultiLegOption>(underlying_->legs(), underlying_->legPayers(), ccys,
                                                     exerciseBuilder_->exercise(), settlementType_, settlementMethod_);

    // determine strikes for calibration basket (simple approach, a la summit)
    // also determine the ibor index (if several, chose the first) to get the engine
    std::vector<Real> strikes(exerciseBuilder_->noticeDates().size(), Null<Real>());
    boost::shared_ptr<IborIndex> index;
    for (Size i = 0; i < exerciseBuilder_->noticeDates().size(); ++i) {
        Real firstFixedRate = Null<Real>();
        Real firstFloatSpread = Null<Real>();
        for (auto const& l : underlying_->legs()) {
            for (auto const& c : l) {
                if (auto cpn = boost::dynamic_pointer_cast<FixedRateCoupon>(c)) {
                    if (cpn->accrualStartDate() >= exerciseBuilder_->noticeDates()[i] && firstFixedRate == Null<Real>())
                        firstFixedRate = cpn->rate();
                } else if (auto cpn = boost::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                    firstFloatSpread = cpn->spread();
                    if (index == nullptr) {
                        if (auto tmp = boost::dynamic_pointer_cast<IborIndex>(cpn->index())) {
                            DLOG("found ibor / ois index '" << tmp->name() << "'");
                            index = tmp;
                        } else if (auto tmp = boost::dynamic_pointer_cast<SwapIndex>(cpn->index())) {
                            DLOG("found cms index " << tmp->name() << ", use key '" << tmp->iborIndex()->name()
                                                    << "' to look up vol");
                            index = tmp->iborIndex();
                        }
                    }
                }
            }
        }
        if (firstFixedRate != Null<Real>()) {
            strikes[i] = firstFixedRate;
            if (firstFloatSpread != Null<Real>()) {
                strikes[i] -= firstFloatSpread;
            }
        }
        DLOG("calibration strike for ex date "
             << QuantLib::io::iso_date(exerciseBuilder_->noticeDates()[i]) << " is "
             << (strikes[i] == Null<Real>() ? "ATMF" : std::to_string(strikes[i])) << " (fixed rate "
             << (firstFixedRate == Null<Real>() ? "NA" : std::to_string(firstFixedRate)) << ", spread "
             << (firstFloatSpread == Null<Real>() ? "NA" : std::to_string(firstFloatSpread)) << ")");
    }

    if (index == nullptr) {
        DLOG("no ibor, ois, cms index found, use ccy key to look up vol");
    }

    auto builder = engineFactory->builder("BermudanSwaption");
    auto swaptionBuilder = boost::dynamic_pointer_cast<BermudanSwaptionEngineBuilder>(builder);
    QL_REQUIRE(swaptionBuilder, "internal error: could not cast to BermudanSwaptionEngineBuilder");

    auto tmp = engineFactory->builder("Swap");
    auto swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "internal error: could not cast to SwapEngineBuilder");

    cpu_timer timer;
    // use ibor / ois index as key, if possible, otherwise the npv currency
    auto swaptionEngine = swaptionBuilder->engine(
        id(), index == nullptr ? npvCurrency_ : IndexNameTranslator::instance().oreName(index->name()),
        exerciseBuilder_->noticeDates(), underlying_->maturity(), strikes);
    timer.stop();
    DLOG("Swaption model calibration time: " << timer.format(default_places, "%w") << " s");
    swaption->setPricingEngine(swaptionEngine);

    auto swapEngine = swapBuilder->engine(parseCurrency(npvCurrency_));

    std::vector<boost::shared_ptr<Instrument>> underlyingSwaps =
        buildUnderlyingSwaps(swapEngine, exerciseBuilder_->noticeDates());

    // If premium data is provided
    // 1) build the fee trade and pass it to the instrument wrapper for pricing
    // 2) add fee payment as additional trade leg for cash flow reporting
    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Real multiplier = positionType_ == Position::Long ? 1.0 : -1.0;
    Date lastPremiumDate =
        addPremiums(additionalInstruments, additionalMultipliers, 1.0, optionData_.premiumData(), -multiplier,
                    parseCurrency(npvCurrency_), engineFactory, swaptionBuilder->configuration(MarketContext::pricing));

    // instrument_ = boost::shared_ptr<InstrumentWrapper> (new VanillaInstrument (swaption, multiplier));
    instrument_ = boost::make_shared<BermudanOptionWrapper>(
        swaption, positionType_ == Position::Long ? true : false, exerciseBuilder_->noticeDates(),
        settlementType_ == Settlement::Physical ? true : false, underlyingSwaps, 1.0, 1.0, additionalInstruments,
        additionalMultipliers);

    maturity_ = std::max(maturity_, lastPremiumDate);

    DLOG("Building Bermudan Swaption done");
}

boost::shared_ptr<VanillaSwap> Swaption::buildVanillaSwap(const boost::shared_ptr<EngineFactory>& engineFactory) {

    Date exerciseDate = exerciseBuilder_->exercise()->dates().front();

    Size fixedLegIndex, floatingLegIndex;
    if (legData_[0].legType() == "Floating" && legData_[1].legType() == "Fixed") {
        floatingLegIndex = 0;
        fixedLegIndex = 1;
    } else if (legData_[1].legType() == "Floating" && legData_[0].legType() == "Fixed") {
        floatingLegIndex = 1;
        fixedLegIndex = 0;
    } else {
        QL_FAIL("Invalid leg types " << legData_[0].legType() << " + " << legData_[1].legType());
    }
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(legData_[fixedLegIndex].concreteLegData());
    boost::shared_ptr<FloatingLegData> floatingLegData =
        boost::dynamic_pointer_cast<FloatingLegData>(legData_[floatingLegIndex].concreteLegData());

    boost::shared_ptr<EngineBuilder> tmp = engineFactory->builder("Swap");
    boost::shared_ptr<SwapEngineBuilderBase> swapBuilder = boost::dynamic_pointer_cast<SwapEngineBuilderBase>(tmp);
    QL_REQUIRE(swapBuilder, "No Swap Builder found for Swaption " << id());

    // Get Trade details
    // We known that the notional, fixed rate, float spread, gearing are all constant in the underlying leg. To make
    // sure we get the correct value from the leg data we take the _last_ value in the leg data vectors, because
    // the first might contain a value that is different, but lies before the schedule start date.
    string ccy = legData_[0].currency();
    Currency currency = parseCurrency(ccy);
    Real nominal = legData_[0].notionals().back();

    Real rate = fixedLegData->rates().back();
    Real spread = floatingLegData->spreads().empty() ? 0.0 : floatingLegData->spreads().back();
    string indexName = floatingLegData->index();

    Schedule fixedSchedule = makeSchedule(legData_[fixedLegIndex].schedule());
    DayCounter fixedDayCounter = parseDayCounter(legData_[fixedLegIndex].dayCounter());

    Schedule floatingSchedule = makeSchedule(legData_[floatingLegIndex].schedule());
    Handle<IborIndex> index =
        engineFactory->market()->iborIndex(indexName, swapBuilder->configuration(MarketContext::pricing));
    DayCounter floatingDayCounter = parseDayCounter(legData_[floatingLegIndex].dayCounter());

    BusinessDayConvention paymentConvention =
        parseBusinessDayConvention(legData_[floatingLegIndex].paymentConvention());

    VanillaSwap::Type type = legData_[fixedLegIndex].isPayer() ? VanillaSwap::Payer : VanillaSwap::Receiver;

    // only take into account accrual periods with start date on or after first exercise date (if given)
    std::vector<Date> fixDates = fixedSchedule.dates();
    auto it1 = std::lower_bound(fixDates.begin(), fixDates.end(), exerciseDate);
    fixDates.erase(fixDates.begin(), it1);
    // check we have at least 1 to stop set fault on vector(fixDates.size() - 1, true) but maybe check should be 2
    QL_REQUIRE(fixDates.size() >= 2, "Not enough schedule dates are left in Swaption fixed leg (check exercise dates)");
    fixedSchedule = Schedule(fixDates, fixedSchedule.calendar(), Unadjusted, boost::none, boost::none, boost::none,
                             boost::none, std::vector<bool>(fixDates.size() - 1, true));
    std::vector<Date> floatingDates = floatingSchedule.dates();
    auto it2 = std::lower_bound(floatingDates.begin(), floatingDates.end(), exerciseDate);
    floatingDates.erase(floatingDates.begin(), it2);
    QL_REQUIRE(floatingDates.size() >= 2,
               "Not enough schedule dates are left in Swaption floating leg (check exercise dates)");
    floatingSchedule = Schedule(floatingDates, floatingSchedule.calendar(), Unadjusted, boost::none, boost::none,
                                boost::none, boost::none, std::vector<bool>(floatingDates.size() - 1, true));

    // Build a vanilla (bullet) swap underlying
    boost::shared_ptr<VanillaSwap> swap =
        boost::make_shared<VanillaSwap>(type, nominal, fixedSchedule, rate, fixedDayCounter, floatingSchedule, *index,
                                        spread, floatingDayCounter, paymentConvention);

    swap->setPricingEngine(swapBuilder->engine(currency));
    return swap;
}

std::vector<boost::shared_ptr<Instrument>>
Swaption::buildUnderlyingSwaps(const boost::shared_ptr<PricingEngine>& swapEngine,
                               const std::vector<Date>& exerciseDates) {
    std::vector<boost::shared_ptr<Instrument>> swaps;
    for (Size i = 0; i < exerciseDates.size(); ++i) {
        std::vector<Leg> legs = underlying_->legs();
        std::vector<bool> payer = underlying_->legPayers();
        for (Size j = 0; j < legs.size(); ++j) {
            Date ed = exerciseDates[i];
            auto it = std::lower_bound(legs[j].begin(), legs[j].end(), exerciseDates[i],
                                       [&ed](const boost::shared_ptr<CashFlow>& c, const Date& d) {
                                           if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
                                               return cpn->accrualStartDate() < ed;
                                           } else {
                                               return c->date() < ed;
                                           }
                                       });
            if (it != legs[j].begin())
                --it;
            legs[j].erase(legs[j].begin(), it);
        }
        auto newSwap = boost::make_shared<QuantLib::Swap>(legs, payer);
        if (swapEngine != nullptr) {
            newSwap->setPricingEngine(swapEngine);
        }
        swaps.push_back(newSwap);
        for (auto const& l : legs) {
            if (l.empty()) {
                WLOG("Added empty leg to underlying swap for exercise " << QuantLib::io::iso_date(exerciseDates[i])
                                                                        << "!");
            } else {
                Date d;
                if (auto cpn = boost::dynamic_pointer_cast<Coupon>(l.front())) {
                    d = cpn->accrualStartDate();
                } else {
                    d = l.front()->date();
                }
                DLOG("Added leg with start date " << QuantLib::io::iso_date(d) << " for exercise "
                                                  << QuantLib::io::iso_date(exerciseDates[i]));
            }
        }
    }
    return swaps;
}

QuantLib::Real Swaption::notional() const {
    Real tmp = 0.0;
    for (auto const& l : underlying_->legs()) {
        tmp = std::max(tmp, currentNotional(l));
    }
    return tmp;
}

const std::map<std::string, boost::any>& Swaption::additionalData() const {
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    for (Size i = 0; i < std::min(legData_.size(), legs_.size()); ++i) {
        string legID = to_string(i + 1);
        additionalData_["legType[" + legID + "]"] = legData_[i].legType();
        additionalData_["isPayer[" + legID + "]"] = legData_[i].isPayer();
        additionalData_["notionalCurrency[" + legID + "]"] = legData_[i].currency();
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
    optionData_.fromXML(XMLUtils::getChildNode(swapNode, "OptionData"));
    legData_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        legData_.push_back(ld);
    }
}

XMLNode* Swaption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swaptionNode = doc.allocNode("SwaptionData");
    XMLUtils::appendNode(node, swaptionNode);

    XMLUtils::appendNode(swaptionNode, optionData_.toXML(doc));
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swaptionNode, legData_[i].toXML(doc));

    return node;
}
} // namespace data
} // namespace ore
