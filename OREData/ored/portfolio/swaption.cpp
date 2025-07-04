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
#include <ql/instruments/swaption.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/cashflows/scaledcoupon.hpp> 
#include <qle/instruments/multilegoption.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ored/model/irmodeldata.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/timer/timer.hpp>

#include <algorithm>

#include <qle/models/representativeswaption.hpp>

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

// This helper functionality checks for constant definitions of the given 
// notional, the rates and the spreads. Those fields must have the same values everywhere on all legs.
// Additional to this, the gearing must be equal to one on all floating legs.
// Finally, the floating legs must be of type IborCoupon, OvernightIndexedCoupon or AverageONIndexedCoupon.
// If all those conditions are met, the trade is called standard.
bool areStandardLegs(const vector<vector<ext::shared_ptr<CashFlow>>> &legs)
{

    // Fields to be checked on the fixed legs
    Real constNotional = Null<Real>();
    Real constRate = Null<Real>();

    // Fields to be checked on the floating legs
    Real constSpread = Null<Real>();

    for (Size i = 0; i < legs.size(); ++i) {
        for (auto const& c : legs[i]) { 
           
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                if(constNotional == Null<Real>()) 
                    constNotional = cpn->nominal();
                else if (!QuantLib::close_enough(cpn->nominal(), constNotional))
                    return false;

                if (!QuantLib::close_enough(cpn->gearing() , 1.00))
                    return false;
   
                if(constSpread == Null<Real>())
                    constSpread = cpn->spread();
                else if (!QuantLib::close_enough(cpn->spread(), constSpread))
                    return false;
                
                if (                    
                    !(QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c))
                    && !(QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c)) 
                    && !(QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c))                
                )
                    return false; // The trade must then be a non-standard type like e.g. CMS coupon

                continue;
            }

            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c)) {
                if(constNotional == Null<Real>()) 
                    constNotional = cpn->nominal();
                else if (!QuantLib::close_enough(cpn->nominal(), constNotional))
                    return false;
   
                if(constRate == Null<Real>())
                    constRate = cpn->rate();
                else if (!QuantLib::close_enough(cpn->rate(), constRate))
                    return false;
                
                continue;
            }

            return false; // Coupon could not be cast to one of the two main types
        }
    }

    // Both legs and fields must have been there at least once
    if(constNotional == Null<Real>() || constRate == Null<Real>() || constSpread == Null<Real>())
        return false; 

    // If no non-constant field and no other subtlety was found return true
    return true;
}

void Swaption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("Swaption::build() for " << id());

    // 1 ISDA taxonomy

    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Swaption");
    additionalData_["isdaTransaction"] = string("");

    // 1a fill currencies and set notional to null (will be retrieved via notional())

    npvCurrency_ = notionalCurrency_ = "USD"; // only if no legs are given, not relevant in this case

    if (!legData_.empty()) {
        npvCurrency_ = notionalCurrency_ = legData_[0].currency();
    }

    // 2 build underlying swap and copy its required fixings

    underlying_ = QuantLib::ext::make_shared<ore::data::Swap>(Envelope(), legData_);
    underlying_->build(engineFactory);
    requiredFixings_.addData(underlying_->requiredFixings());

    // 3 build the exercise and parse some fields

    DLOG("Swaption::build() for " << id() << ": build exercise");

    exerciseBuilder_ = QuantLib::ext::make_shared<ExerciseBuilder>(optionData_, underlying_->legs());

    exerciseType_ = parseExerciseType(optionData_.style());
    settlementType_ = parseSettlementType(optionData_.settlement());
    settlementMethod_ = optionData_.settlementMethod() == "" ? defaultSettlementMethod(settlementType_)
                                                             : parseSettlementMethod(optionData_.settlementMethod());
    positionType_ = parsePositionType(optionData_.longShort());

    notional_ = Null<Real>();

    Date today = Settings::instance().evaluationDate();

    // 5 if the swaption is exercised (as per option data / exercise data), build the cashflows that remain to be paid

    if (exerciseBuilder_->isExercised()) {
        Date exerciseDate = exerciseBuilder_->exerciseDate();
        maturity_ = std::max(today, exerciseDate); // will be updated below
        maturityType_ = maturity_ == today ? "Today" : "Exercise Date";
        if (optionData_.settlement() == "Physical") {

            // 5.1 if physical exercise, inlcude the "exercise-into" cashflows of the underlying

            for (Size i = 0; i < underlying_->legs().size(); ++i) {
                legs_.push_back(Leg());
                legCurrencies_.push_back(underlying_->legCurrencies()[i]);
                legPayers_.push_back(underlying_->legPayers()[i]);
                for (auto const& c : underlying_->legs()[i]) {
                    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                        Date exerciseAccrualStart = optionData_.midCouponExercise()
                                                        ? exerciseBuilder_->noticeCalendar().advance(
                                                              exerciseDate, exerciseBuilder_->noticePeriod(),
                                                              exerciseBuilder_->noticeConvention())
                                                        : cpn->accrualStartDate();
                        if (exerciseDate <= exerciseAccrualStart && exerciseAccrualStart < cpn->accrualEndDate()) {
                            if(optionData_.midCouponExercise()) {
                                Real midCouponMultiplier =
                                    cpn->dayCounter().yearFraction(exerciseAccrualStart, cpn->accrualEndDate()) /
                                    cpn->dayCounter().yearFraction(cpn->accrualStartDate(), cpn->accrualEndDate());
                                legs_.back().push_back(
                                    QuantLib::ext::make_shared<QuantExt::ScaledCoupon>(midCouponMultiplier, cpn));
                            } else {
                                legs_.back().push_back(c);
                            }
                            maturity_ = std::max(maturity_, c->date());
                            if (maturity_ == c->date())
                                maturityType_ = "Coupon Date";
                            if (notional_ == Null<Real>())
                                notional_ = cpn->nominal();
                        }
                    } else if (exerciseDate <= c->date()) {
                        legs_.back().push_back(c);
                        maturity_ = std::max(maturity_, c->date());
                        if (maturity_ == c->date())
                            maturityType_ = "Coupon Date";
                    }
                }
            }

        } else {

            // 5.2 if cash exercise, include the cashSettlement payment

            if (exerciseBuilder_->cashSettlement()) {
                legs_.push_back(Leg());
                legs_.back().push_back(exerciseBuilder_->cashSettlement());
                legCurrencies_.push_back(npvCurrency_);
                legPayers_.push_back(false);
                maturity_ = std::max(maturity_, exerciseBuilder_->cashSettlement()->date());
                if (maturity_ == exerciseBuilder_->cashSettlement()->date())
                    maturityType_ = "Cash Settlement Date";
            }
        }

        // 5.3 include the exercise fee payment

        if (exerciseBuilder_->feeSettlement()) {
            legs_.push_back(Leg());
            legs_.back().push_back(exerciseBuilder_->feeSettlement());
            legCurrencies_.push_back(npvCurrency_);
            legPayers_.push_back(true);
            maturity_ = std::max(maturity_, exerciseBuilder_->feeSettlement()->date());
            if (maturity_ == exerciseBuilder_->feeSettlement()->date())
                maturityType_ = "Fee Settlement Date";
        }

        // 5.4 add unconditional premiums, build instrument (as swap) and exit

        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, Position::Long ? 1.0 : -1.0,
                                           optionData_.premiumData(), positionType_ == Position::Long ? -1.0 : 1.0,
                                           parseCurrency(npvCurrency_), engineFactory,
                                           engineFactory->configuration(MarketContext::pricing));
        auto builder = QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(engineFactory->builder("Swap"));
        QL_REQUIRE(builder, "could not get swap builder to build exercised swaption instrument.");
        auto swap = QuantLib::ext::make_shared<QuantLib::Swap>(legs_, legPayers_);
        swap->setPricingEngine(builder->engine(parseCurrency(npvCurrency_),
                                               envelope().additionalField("discount_curve", false),
                                               envelope().additionalField("security_spread", false), {}));
        setSensitivityTemplate(*builder);
        addProductModelEngine(*builder);
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(swap, positionType_ == Position::Long ? 1.0 : -1.0,
                                                            additionalInstruments, additionalMultipliers);
        maturity_ = std::max(maturity_, lastPremiumDate);
        if (maturity_ == lastPremiumDate)
            maturityType_ = "Last Premium Date";
        DLOG("Building exercised swaption done.");
        return;
    }

    // 6 if we do not have an active exercise as of today, or no underlying legs we only build unconditional premiums

    if (exerciseBuilder_->exercise() == nullptr || exerciseBuilder_->exercise()->dates().empty() ||
        exerciseBuilder_->exercise()->dates().back() <= today || legData_.empty()) {

        legs_ = {{QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(0.0, today)}};
        legCurrencies_.push_back(npvCurrency_);
        legPayers_.push_back(false);
        maturity_ = today;
        maturityType_ = "Today";
        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, Position::Long ? 1.0 : -1.0,
                                           optionData_.premiumData(), positionType_ == Position::Long ? -1.0 : 1.0,
                                           parseCurrency(npvCurrency_), engineFactory,
                                           engineFactory->configuration(MarketContext::pricing));
        auto builder = QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(engineFactory->builder("Swap"));
        QL_REQUIRE(builder, "could not get swap builder to build expired swaption instrument.");
        auto swap = QuantLib::ext::make_shared<QuantLib::Swap>(legs_, legPayers_);
        swap->setPricingEngine(builder->engine(parseCurrency(npvCurrency_),
                                               envelope().additionalField("discount_curve", false),
                                               envelope().additionalField("security_spread", false), {}));
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(swap, positionType_ == Position::Long ? 1.0 : -1.0,
                                                            additionalInstruments, additionalMultipliers);
        setSensitivityTemplate(*builder);
        addProductModelEngine(*builder);
        maturity_ = std::max(maturity_, lastPremiumDate);
        if (maturity_ == lastPremiumDate)
            maturityType_ = "Last Premium Date";
        DLOG("Building (non-exercised) swaption without alive exercise dates done.");
        return;
    }

    // 7 fill legs, only include coupons after first exercise

    legCurrencies_ = underlying_->legCurrencies();
    legPayers_ = underlying_->legPayers();
    legs_.clear();
    Date firstExerciseDate = exerciseBuilder_->exercise()->dates().front();
    for (auto const& l : underlying_->legs()) {
        legs_.push_back(Leg());
        for (auto const& c : l) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                if (firstExerciseDate <= cpn->accrualStartDate()) {
                    legs_.back().push_back(c);
                }
            } else if (firstExerciseDate <= c->date()) {
                legs_.back().push_back(c);
            }
        }
    }

    // 8 build swaption

    if (settlementType_ == Settlement::Physical) {
        maturity_ = underlying_->maturity();
        maturityType_ = "Underlying Maturity";
    }
    else {
        maturity_ = exerciseBuilder_->noticeDates().back();
        maturityType_ = "Last Notice Date";
    }

    if (exerciseType_ != Exercise::European && settlementType_ == Settlement::Cash &&
        settlementMethod_ == Settlement::ParYieldCurve)
        WLOG("Cash-settled Bermudan/American Swaption (id = "
             << id() << ") with ParYieldCurve settlement method not supported by Lgm engine. "
             << "Approximate pricing using CollateralizedCashPrice pricing methodology");

    std::vector<QuantLib::Currency> ccys;
    for (auto const& c : underlying_->legCurrencies())
        ccys.push_back(parseCurrency(c));
    auto swaption = QuantLib::ext::make_shared<QuantExt::MultiLegOption>(
        underlying_->legs(), underlying_->legPayers(), ccys, exerciseBuilder_->exercise(), settlementType_,
        settlementMethod_, exerciseBuilder_->settlementDates(), optionData_.midCouponExercise(),
        exerciseBuilder_->noticePeriod(), exerciseBuilder_->noticeCalendar(), exerciseBuilder_->noticeConvention());

    std::string builderType;
    std::vector<std::string> builderPrecheckMessages;
    // for European swaptions that are not handled by the black pricer, we fall back to the numeric Bermudan pricer
    if (exerciseType_ == Exercise::European &&
        QuantExt::BlackMultiLegOptionEngineBase::instrumentIsHandled(*swaption, builderPrecheckMessages)) {
        if(areStandardLegs(underlying_->legs()))
            builderType = "EuropeanSwaption";
        else
            builderType = "EuropeanSwaption_NonStandard";
    } else {
        QL_REQUIRE(
            QuantExt::NumericLgmMultiLegOptionEngineBase::instrumentIsHandled(*swaption, builderPrecheckMessages),
            "Swaption::build(): instrument is not handled by the available engines: " +
                boost::join(builderPrecheckMessages, ", "));
       
        if (exerciseType_ == Exercise::European || exerciseType_ == Exercise::Bermudan)
        {           
            if(areStandardLegs(underlying_->legs()))
                builderType = "BermudanSwaption";
            else
                builderType = "BermudanSwaption_NonStandard";
        }
        else if (exerciseType_ == Exercise::American)
        { 
            if(areStandardLegs(underlying_->legs()))
                builderType = "AmericanSwaption";
            else
                builderType = "AmericanSwaption_NonStandard";
        }
    }

    string pricingProductType = envelope().additionalField("pricing_product_type", false);
    if (!pricingProductType.empty()) 
        builderType = pricingProductType;

    DLOG("Getting builder for '" << builderType << "', got " << builderPrecheckMessages.size()
                                 << " builder precheck messages:");
    for (auto const& m : builderPrecheckMessages) {
        DLOG(m);
    }

    auto swaptionBuilder = QuantLib::ext::dynamic_pointer_cast<SwaptionEngineBuilder>(engineFactory->builder(builderType));
    QL_REQUIRE(swaptionBuilder, "Swaption::build(): internal error: could not cast to SwaptionEngineBuilder");

    auto swapBuilder = QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(engineFactory->builder("Swap"));
    QL_REQUIRE(swapBuilder, "Swaption::build(): internal error: could not cast to SwapEngineBuilder");

    // 9.1  determine index (if several, pick first) got get the engine

    QuantLib::ext::shared_ptr<InterestRateIndex> index;

    for (auto const& l : underlying_->legs()) {
        for (auto const& c : l) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                if (index == nullptr) {
                    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<IborIndex>(cpn->index())) {
                        DLOG("found ibor / ois index '" << tmp->name() << "'");
                        index = tmp;
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(cpn->index())) {
                        DLOG("found cms index " << tmp->name() << ", use key '" << tmp->iborIndex()->name()
                                                << "' to look up vol");
                        index = tmp->iborIndex();
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<BMAIndex>(cpn->index())) {
                        DLOG("found bma/sifma index '" << tmp->name() << "'");
                        index = tmp;
                    }
                }
            }
        }
    }

    if (index == nullptr) {
        DLOG("no ibor, ois, bma/sifma, cms index found, use ccy key to look up vol");
    }

    // 9.2 determine strikes for calibration basket (simple approach, a la summit)

    std::vector<Real> strikes(exerciseBuilder_->noticeDates().size(), Null<Real>());
    for (Size i = 0; i < exerciseBuilder_->noticeDates().size(); ++i) {
        Real firstFixedRate = Null<Real>(), lastFixedRate = Null<Real>();
        Real firstFloatSpread = Null<Real>(), lastFloatSpread = Null<Real>();
        Real firstGearing = Null<Real>(), lastGearing = Null<Real>();

        for (auto const& l : underlying_->legs()) {
            for (auto const& c : l) {
                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c)) {
                    if (cpn->accrualStartDate() >= exerciseBuilder_->noticeDates()[i] && firstFixedRate == Null<Real>())
                        firstFixedRate = cpn->rate();
                    lastFixedRate = cpn->rate();
                } else if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                    if (cpn->accrualStartDate() >= exerciseBuilder_->noticeDates()[i] &&
                        firstFloatSpread == Null<Real>()) {
                        firstFloatSpread = cpn->spread();
                        firstGearing = cpn->gearing();
                    }
                    lastFloatSpread = cpn->spread();
                    lastGearing = cpn->gearing();
                    if (index == nullptr) {
                        if (auto tmp = QuantLib::ext::dynamic_pointer_cast<IborIndex>(cpn->index())) {
                            DLOG("found ibor / ois index '" << tmp->name() << "'");
                            index = tmp;
                        } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(cpn->index())) {
                            DLOG("found cms index " << tmp->name() << ", use key '" << tmp->iborIndex()->name()
                                                    << "' to look up vol");
                            index = tmp->iborIndex();
                        } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<BMAIndex>(cpn->index())) {
                            DLOG("found bma/sifma index '" << tmp->name() << "'");
                            index = tmp;
                        }
                    }
                }
            }
        }
        // if no first fixed rate (float spread) was found, fall back on the last values
        if(firstFixedRate == Null<Real>())
            firstFixedRate = lastFixedRate;
        if(firstFloatSpread == Null<Real>())
            firstFloatSpread = lastFloatSpread;
        // if no first gearing was found, fall back on the last value
        if(firstGearing == Null<Real>())
            firstGearing = lastGearing;
        // construct calibration strike
        if (firstFixedRate != Null<Real>()) {
            strikes[i] = firstFixedRate;
            if (firstFloatSpread != Null<Real>()) {
                strikes[i] -= firstFloatSpread;
            }
        }
        if (firstGearing != Null<Real>())
            strikes[i] /= firstGearing;
        DLOG("calibration strike for ex date "
             << QuantLib::io::iso_date(exerciseBuilder_->noticeDates()[i]) << " is "
             << (strikes[i] == Null<Real>() ? "ATMF" : std::to_string(strikes[i])) << " (fixed rate "
             << (firstFixedRate == Null<Real>() ? "NA" : std::to_string(firstFixedRate)) << ", spread "
             << (firstFloatSpread == Null<Real>() ? "NA" : std::to_string(firstFloatSpread)) << ", gearing "
             << (firstGearing == Null<Real>() ? "NA" : std::to_string(firstGearing)) << ")");
    }

    // 9.3 build underlying swaps, add premiums, build option wrapper

    auto swapEngine =
        swapBuilder->engine(parseCurrency(npvCurrency_), envelope().additionalField("discount_curve", false),
                            envelope().additionalField("security_spread", false), {});
    
    std::vector<QuantLib::ext::shared_ptr<Instrument>> underlyingSwaps = buildUnderlyingSwaps(swapEngine, exerciseBuilder_->noticeDates());

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Real multiplier = positionType_ == Position::Long ? 1.0 : -1.0;
    Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, Position::Long ? 1.0 : -1.0,
                                       optionData_.premiumData(), -multiplier, parseCurrency(npvCurrency_),
                                       engineFactory, swaptionBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::make_shared<BermudanOptionWrapper>(
        swaption, positionType_ == Position::Long ? true : false, exerciseBuilder_->noticeDates(),
        exerciseBuilder_->settlementDates(), settlementType_ == Settlement::Physical ? true : false,
	    underlyingSwaps, 1.0, 1.0, additionalInstruments, additionalMultipliers);

    maturity_ = std::max(maturity_, lastPremiumDate);
    if (maturity_ == lastPremiumDate)
        maturityType_ = "Last Premium Date";

    // 9.4 get engine and set it

    cpu_timer timer;
    auto calibrationStrategy =
        parseCalibrationStrategy(swaptionBuilder->modelParameter("CalibrationStrategy", {}, false, "None"));
    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine; 
    std::vector<Date> maturitiesEngine;
    std::vector<Rate> strikesEngine;

    if (calibrationStrategy != CalibrationStrategy::DeltaGammaAdjusted) {
        maturitiesEngine = std::vector<Date>(exerciseBuilder_->noticeDates().size(), underlying_->maturity());
        strikesEngine = strikes;
    } else {
        string qualifier = index == nullptr ? npvCurrency_ : IndexNameTranslator::instance().oreName(index->name());
        std::vector<QuantLib::ext::shared_ptr<FixedVsFloatingSwap>> underlyingMatched =
            buildRepresentativeSwaps(engineFactory, qualifier);
        for (const auto& swap : underlyingMatched) {
            maturitiesEngine.push_back(swap->maturityDate());
            strikesEngine.push_back(swap->fixedRate());
        }
    }

    // use ibor / ois index as key, if possible, otherwise the npv currency
    swaptionEngine = swaptionBuilder->engine(
        id(), index == nullptr ? npvCurrency_ : IndexNameTranslator::instance().oreName(index->name()),
        exerciseBuilder_->noticeDates(), maturitiesEngine, strikesEngine, exerciseType_ == Exercise::American,
        envelope().additionalField("discount_curve", false), envelope().additionalField("security_spread", false));

    timer.stop();
    DLOG("Swaption model calibration time: " << timer.format(default_places, "%w") << " s");
    
    swaption->setPricingEngine(swaptionEngine);
    setSensitivityTemplate(*swaptionBuilder);
    addProductModelEngine(*swaptionBuilder);

    DLOG("Building Swaption done");
}

std::vector<QuantLib::ext::shared_ptr<FixedVsFloatingSwap>>
Swaption::buildRepresentativeSwaps(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                   const std::string& qualifier) {
    DLOG("build representative swaps.")
    auto market = QuantLib::ext::dynamic_pointer_cast<Market>(engineFactory->market());
    auto configuration = engineFactory->configuration(MarketContext::irCalibration);
    Handle<YieldTermStructure> discountCurve = market->discountCurve(npvCurrency_, configuration);
    Handle<SwapIndex> swapIndex = market->swapIndex(market->swapIndexBase(qualifier, configuration), configuration);
    QuantExt::RepresentativeSwaptionMatcher matcher(underlying_->legs(), underlying_->legPayers(), *swapIndex, true,
                                                    discountCurve, 0.0);
    std::vector<QuantLib::ext::shared_ptr<FixedVsFloatingSwap>> swaps;
    for (Size i = 0; i < exerciseBuilder_->noticeDates().size(); ++i) {
        Date ed = exerciseBuilder_->noticeDates()[i];
        swaps.push_back(
            matcher
                .representativeSwaption(
                    ed, QuantExt::RepresentativeSwaptionMatcher::InclusionCriterion::AccrualStartGeqExercise)
                ->underlying());
        DLOG("representative swap for exercise date " << ed << ": fixed rate = " << swaps.back()->fixedRate()
                                                      << ", maturity = " << swaps.back()->maturityDate()
                                                      << ", notional = " << swaps.back()->nominal());
    }
    return swaps;
}

std::vector<QuantLib::ext::shared_ptr<Instrument>>
Swaption::buildUnderlyingSwaps(const QuantLib::ext::shared_ptr<PricingEngine>& swapEngine,
                               const std::vector<Date>& exerciseDates) {

    std::vector<QuantLib::ext::shared_ptr<Instrument>> swaps;

    for (Size i = 0; i < exerciseDates.size(); ++i) {

        std::vector<Leg> legs = underlying_->legs();
        std::vector<bool> payer = underlying_->legPayers();

        for (Size j = 0; j < legs.size(); ++j) {
            Date ed = exerciseDates[i];
            auto it = std::lower_bound(legs[j].begin(), legs[j].end(), exerciseDates[i],
                                       [&ed](const QuantLib::ext::shared_ptr<CashFlow>& c, const Date& d) {
                                           if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                                               return cpn->accrualStartDate() < ed;
                                           } else {
                                               return c->date() < ed;
                                           }
                                       });
            if (it != legs[j].begin())
                --it;
            legs[j].erase(legs[j].begin(), it);
        }

        auto newSwap = QuantLib::ext::make_shared<QuantLib::Swap>(legs, payer);

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
                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(l.front())) {
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

bool Swaption::isExercised() const { return exerciseBuilder_->isExercised(); }

const std::map<std::string, boost::any>& Swaption::additionalData() const {
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    for (Size i = 0; i < std::min(legData_.size(), legs_.size()); ++i) {
        string legID = to_string(i + 1);
        additionalData_["legType[" + legID + "]"] = ore::data::to_string(legData_[i].legType());
        additionalData_["isPayer[" + legID + "]"] = legData_[i].isPayer();
        additionalData_["notionalCurrency[" + legID + "]"] = legData_[i].currency();
        for (Size j = 0; j < legs_[i].size(); ++j) {
            QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
            // pick flow with earliest future payment date on this leg
            if (flow->date() > asof) {
                additionalData_["amount[" + legID + "]"] = flow->amount();
                additionalData_["paymentDate[" + legID + "]"] = to_string(flow->date());
                QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
                if (coupon) {
                    additionalData_["currentNotional[" + legID + "]"] = coupon->nominal();
                    additionalData_["rate[" + legID + "]"] = coupon->rate();
                    QuantLib::ext::shared_ptr<FloatingRateCoupon> frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(flow);
                    if (frc) {
                        additionalData_["index[" + legID + "]"] = frc->index()->name();
                        additionalData_["spread[" + legID + "]"] = frc->spread();
                    }
                }
                break;
            }
        }
        if (legs_[i].size() > 0) {
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs_[i][0]);
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

XMLNode* Swaption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swaptionNode = doc.allocNode("SwaptionData");
    XMLUtils::appendNode(node, swaptionNode);

    XMLUtils::appendNode(swaptionNode, optionData_.toXML(doc));
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swaptionNode, legData_[i].toXML(doc));

    return node;
}

map<AssetClass, set<string>>
Swaption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    map<AssetClass, set<string>> result;
    if (auto s = envelope().additionalField("security_spread", false); !s.empty())
        result[AssetClass::BOND] = {s};
    return result;
}

} // namespace data
} // namespace ore
