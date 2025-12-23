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

#include <ored/marketdata/inflationcurve.hpp>
#include <ored/utilities/inflationstartdate.hpp>
#include <ored/utilities/log.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>

#include "inflationcurve.hpp"
#include <algorithm>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/inflation/piecewisecpiinflationcurve.hpp>
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;
using namespace std;
using namespace ore::data;

namespace ore {
namespace data {

InflationCurve::InflationCurve(Date asof, InflationCurveSpec spec, const Loader& loader,
                               const CurveConfigurations& curveConfigs,
                               map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                               const bool buildCalibrationInfo) {

    try {

        const QuantLib::ext::shared_ptr<InflationCurveConfig>& config =
            curveConfigs.inflationCurveConfig(spec.curveConfigID());

        const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();

        Handle<YieldTermStructure> nominalTs;
        auto it = yieldCurves.find(config->nominalTermStructure());
        if (it != yieldCurves.end()) {
            nominalTs = it->second->handle(config->nominalTermStructure());
        } else {
            QL_FAIL("The nominal term structure, " << config->nominalTermStructure()
                                                   << ", required in the building "
                                                      "of the curve, "
                                                   << spec.name() << ", was not found.");
        }

        // We loop over all market data, looking for quotes that match the configuration
        auto seasonality = buildSeasonality(asof, loader, config);
        QuantExt::ext::shared_ptr<ZeroInflationIndex> zcIndex;
        CurveBuildResults results;
        if (config->type() == InflationCurveConfig::Type::ZC) {
            results = buildZeroInflationCurve(asof, loader, conventions, config, nominalTs, seasonality);
            curve_ = results.curve;
            zcIndex = results.index;
            // Force bootstrapping, throw errors now and not later
            auto zr =
                    QuantLib::ext::static_pointer_cast<QuantLib::PiecewiseZeroInflationCurve<Linear>>(curve_)->zeroRate(
                        QL_EPSILON);
            DLOG("Zerorate at base date " << curve_->baseDate() << " is " << zr);
        } else if (config->type() == InflationCurveConfig::Type::YY) {
            // Check if we derive yoy from zc
            bool derive_yoy_from_zc = deriveYYfromZC(config, spec, loader, asof);
            QuantLib::ext::shared_ptr<InflationTermStructure> zeroInflCurve_;
            if (derive_yoy_from_zc) {
                DLOG("Derive YoY inflation quotes from ZC for curve " << spec.name());
                auto res = buildZeroInflationCurve(asof, loader, conventions, config, nominalTs, seasonality);
                zeroInflCurve_ = res.curve;
            }
            results =
                buildYoYInflationCurve(asof, loader, conventions, config, nominalTs, seasonality, derive_yoy_from_zc, zeroInflCurve_);
            curve_ = results.curve;
            // Force bootstrapping, throw errors now and not later
            auto yoyRate = QuantLib::ext::static_pointer_cast<PiecewiseYoYInflationCurve<Linear>>(curve_)->yoyRate(QL_EPSILON);
            DLOG("YoY rate at base date " << curve_->baseDate() << " is " << yoyRate);
                 
        } else {
            QL_FAIL("unknown inflation curve type");
        }

        if (seasonality != nullptr) {
            curve_->setSeasonality(seasonality);
        }
        curve_->enableExtrapolation(config->extrapolate());
        curve_->unregisterWith(Settings::instance().evaluationDate());
        
        // set calibration info
        std::vector<QuantLib::Date> pillarDates = results.pillarDates;
        if (buildCalibrationInfo) {

            if (pillarDates.empty()) {
                // default: fill pillar dates with monthly schedule up to last term given in the curve config (but max
                // 60y)
                Date maturity = results.latestMaturity;
                for (Size i = 1; i < 60 * 12; ++i) {
                    Date current = inflationPeriod(curve_->baseDate() + i * Months, curve_->frequency()).first;
                    if (current + curve_->observationLag() <= maturity)
                        pillarDates.push_back(current);
                    else
                        break;
                }
            }

            if (config->type() == InflationCurveConfig::Type::YY) {
                auto yoyCurve = QuantLib::ext::dynamic_pointer_cast<YoYInflationCurve>(curve_);
                QL_REQUIRE(yoyCurve, "internal error: expected YoYInflationCurve (inflation curve builder)");
                auto calInfo = QuantLib::ext::make_shared<YoYInflationCurveCalibrationInfo>();
                calInfo->dayCounter = config->dayCounter().name();
                calInfo->calendar = config->calendar().empty() ? "null" : config->calendar().name();
                calInfo->baseDate = curve_->baseDate();
                for (Size i = 0; i < pillarDates.size(); ++i) {
                    calInfo->pillarDates.push_back(pillarDates[i]);
                    calInfo->yoyRates.push_back(yoyCurve->yoyRate(pillarDates[i], 0 * Days));
                    calInfo->times.push_back(yoyCurve->timeFromReference(pillarDates[i]));
                }
                calibrationInfo_ = calInfo;
            }

            if (config->type() == InflationCurveConfig::Type::ZC) {
                auto zcCurve = QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(curve_);
                QL_REQUIRE(zcCurve, "internal error: expected ZeroInflationCurve (inflation curve builder)");
                zcIndex = zcIndex->clone(Handle<ZeroInflationTermStructure>(zcCurve));
                auto calInfo = QuantLib::ext::make_shared<ZeroInflationCurveCalibrationInfo>();
                calInfo->dayCounter = config->dayCounter().name();
                calInfo->calendar = config->calendar().empty() ? "null" : config->calendar().name();
                calInfo->baseDate = curve_->baseDate();
                auto lim = inflationPeriod(curve_->baseDate(), curve_->frequency());
                try {
                    calInfo->baseCpi = zcIndex->fixing(lim.first);
                } catch (...) {
                }
                for (Size i = 0; i < pillarDates.size(); ++i) {
                    calInfo->pillarDates.push_back(pillarDates[i]);
                    calInfo->zeroRates.push_back(zcCurve->zeroRate(pillarDates[i], 0 * Days));
                    calInfo->times.push_back(zcCurve->timeFromReference(pillarDates[i]));
                    Real cpi = 0.0;
                    try {
                        cpi = zcIndex->fixing(pillarDates[i]);
                    } catch (...) {
                    }
                    calInfo->forwardCpis.push_back(cpi);
                }
                calibrationInfo_ = calInfo;
            }
        }

    } catch (std::exception& e) {
        QL_FAIL("inflation curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("inflation curve building failed: unknown error");
    }
}

bool InflationCurve::deriveYYfromZC(const boost::shared_ptr<ore::data::InflationCurveConfig>& config,
                                    ore::data::InflationCurveSpec& spec, const ore::data::Loader& loader,
                                    QuantLib::Date& asof) const {
    auto quotes = config->segments().front().quotes();
    QL_REQUIRE(!quotes.empty(), "no quotes provided in first segment for inflation curve " << spec.name());
    auto md = loader.get(quotes.front(), asof);
    QL_REQUIRE(md, "MarketDatum " << quotes.front() << " required to build inflation curve " << spec.name()
                                  << " not found in market data for date " << asof);
    QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP ||
                   md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONSWAP,
               "MarketDatum " << quotes.front() << " is not a valid inflation swap quote");
    return md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP;
}

QuantLib::ext::shared_ptr<Seasonality>
InflationCurve::buildSeasonality(Date asof, const Loader& loader,
                                 const QuantLib::ext::shared_ptr<InflationCurveConfig>& config) const {
    // construct seasonality
    QuantLib::ext::shared_ptr<Seasonality> seasonality;
    if (config->seasonalityBaseDate() != Null<Date>()) {
        if (config->overrideSeasonalityFactors().empty()) {
            std::vector<string> strFactorIDs = config->seasonalityFactors();
            std::vector<double> factors(strFactorIDs.size());
            for (Size i = 0; i < strFactorIDs.size(); i++) {
                QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader.get(strFactorIDs[i], asof);
                // Check that we have a valid seasonality factor
                if (marketQuote) {
                    QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::SEASONALITY,
                               "Market quote (" << marketQuote->name() << ") not of type seasonality.");
                    // Currently only monthly seasonality with 12 multiplicative factors os allowed
                    QL_REQUIRE(config->seasonalityFrequency() == Monthly && strFactorIDs.size() == 12,
                               "Only monthly seasonality with 12 factors is allowed. Provided "
                                   << config->seasonalityFrequency() << " with " << strFactorIDs.size() << " factors.");
                    QuantLib::ext::shared_ptr<SeasonalityQuote> sq =
                        QuantLib::ext::dynamic_pointer_cast<SeasonalityQuote>(marketQuote);
                    QL_REQUIRE(sq, "Could not cast to SeasonalityQuote, internal error.");
                    QL_REQUIRE(sq->type() == "MULT", "Market quote (" << sq->name() << ") not of multiplicative type.");
                    Size seasBaseDateMonth = ((Size)config->seasonalityBaseDate().month());
                    int findex = sq->applyMonth() - seasBaseDateMonth;
                    if (findex < 0)
                        findex += 12;
                    QL_REQUIRE(findex >= 0 && findex < 12, "Unexpected seasonality index " << findex);
                    factors[findex] = sq->quote()->value();
                } else {
                    QL_FAIL("Could not find quote for ID " << strFactorIDs[i] << " with as of date "
                                                           << io::iso_date(asof) << ".");
                }
            }
            QL_REQUIRE(!factors.empty(), "no seasonality factors found");
            seasonality = QuantLib::ext::make_shared<MultiplicativePriceSeasonality>(
                config->seasonalityBaseDate(), config->seasonalityFrequency(), factors);
        } else {
            // override market data by explicit list
            seasonality = QuantLib::ext::make_shared<MultiplicativePriceSeasonality>(
                config->seasonalityBaseDate(), config->seasonalityFrequency(), config->overrideSeasonalityFactors());
        }
    }

    return seasonality;
}


InflationCurve::CurveBuildResults
    InflationCurve::buildZeroInflationCurve(Date asof, const Loader& loader, const QuantLib::ext::shared_ptr<Conventions>& conventions,
                            const QuantLib::ext::shared_ptr<InflationCurveConfig>& config,
                            const Handle<YieldTermStructure>& nominalTs,
                            const QuantLib::ext::shared_ptr<Seasonality>& seasonality) const {
    CurveBuildResults results;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper>> helpers;
    
    // Shall we allow different indices in the segments? 
    // For now we require all segments to use the same index.
    QuantLib::ext::shared_ptr<ZeroInflationIndex> index;
    QuantLib::Period obsLagFromSegment = 0 * Days;
    for (const auto& segment : config->segments()) {
            auto convention =
                QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(conventions->get(segment.convention()));
            QL_REQUIRE(convention, "InflationSwap Conventions for " << segment.convention() << " not found.");
            auto p = getStartAndLag(asof, *convention);
            Date swapStart = p.first;
            if (p.second != 0 * Days) {
                // keep the largest lag across all segments for the curve as observation lag,
                // only relevant if we have multiple segments and publication rules given, otherwise we use lag
                // from the curve config
                obsLagFromSegment = obsLagFromSegment == 0 * Days ? p.second : std::max(obsLagFromSegment, p.second);
            }
            QL_REQUIRE(index == nullptr || index->name() == convention->index()->name(),
                       "all segments must use the same zero inflation index");
            index = convention->index();
            
            for (const auto& q : segment.quotes()) {
                auto md = loader.get(q, asof);
                QL_REQUIRE(md, "MarketDatum " << md << " required to build inflation curve " << config->curveID()
                                              << " not found in market data for date " << asof);
                QL_REQUIRE(md->asofDate() == asof,
                           "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
                QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP,
                           "MarketDatum " << md << " is not a valid inflation swap quote");
                auto zcq = QuantLib::ext::dynamic_pointer_cast<ZcInflationSwapQuote>(md);
                QL_REQUIRE(zcq, "Could not cast to ZcInflationSwapQuote, internal error.");
                CPI::InterpolationType observationInterpolation = convention->interpolated() ? CPI::Linear : CPI::Flat;
                Date maturity = swapStart + zcq->term();
                results.latestMaturity =
                    results.latestMaturity == Date() ? maturity : std::max(results.latestMaturity, maturity);
                DLOG("Zero inflation swap " << zcq->name() << " maturity " << maturity << " term " << zcq->term()
                                            << " quote " << zcq->quote()->value());
                QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
                    QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
                        zcq->quote(), convention->observationLag(), maturity, convention->fixCalendar(),
                        convention->fixConvention(), convention->dayCounter(), index, observationInterpolation,
                        nominalTs, swapStart);

                // Unregister with inflation index. See PR #326 for details.
                instrument->unregisterWithAll();
                instrument->registerWith(nominalTs);
                instrument->registerWith(zcq->quote());

                helpers.push_back(instrument);
            }
        }
    auto curveObsLag = obsLagFromSegment != 0 * Days ? obsLagFromSegment : config->lag();

    QuantLib::Date baseDate = QuantExt::ZeroInflation::curveBaseDate(
                config->useLastAvailableFixingAsBaseDate(), asof, curveObsLag, config->frequency(), index);

    if (config->interpolationVariable() == InflationCurveConfig::InterpolationVariable::ZeroRate) {

        results.curve = QuantLib::ext::make_shared<PiecewiseZeroInflationCurve<Linear>>(
            asof, baseDate, curveObsLag, config->frequency(), config->dayCounter(), helpers, seasonality,
            config->tolerance());
    } else {
        auto baseFixing = index->fixing(baseDate, true);
        if (config->interpolationMethod().empty() || config->interpolationMethod() == "Linear") {

            results.curve = QuantLib::ext::make_shared<QuantExt::PiecewiseCPIInflationCurve<Linear>>(
                asof, baseDate, baseFixing, curveObsLag, config->frequency(), config->dayCounter(), helpers,
                seasonality, config->tolerance());
        } else if (config->interpolationMethod() == "LogLinear") {
            results.curve = QuantLib::ext::make_shared<QuantExt::PiecewiseCPIInflationCurve<LogLinear>>(
                asof, baseDate, baseFixing, curveObsLag, config->frequency(), config->dayCounter(), helpers,
                seasonality, config->tolerance());
        } else {
            QL_FAIL("Interpolation method " << config->interpolationMethod()
                                            << " not supported for ZC cpi inflation curve, use Linear or LogLinear");
        }
    }
    results.index = index;
    return results;
}

InflationCurve::CurveBuildResults
    InflationCurve::buildYoYInflationCurve(Date asof, const Loader& loader, const QuantLib::ext::shared_ptr<Conventions>& conventions,
                            const QuantLib::ext::shared_ptr<InflationCurveConfig>& config,
                            const Handle<YieldTermStructure>& nominalTs,
                            const QuantLib::ext::shared_ptr<Seasonality>& seasonality, bool deriveFromZC,
                           const QuantLib::ext::shared_ptr<InflationTermStructure>& zcCurve) const {
    CurveBuildResults results;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::YoYInflationTraits::helper>> helpers;
    
    // Shall we allow different indices in the segments? 
    // For now we require all segments to use the same index.
    QuantLib::ext::shared_ptr<ZeroInflationIndex> zcIndex;
    QuantLib::ext::shared_ptr<YoYInflationIndex> index;
    QuantLib::Period obsLagFromSegment = 0 * Days;
    bool interpolatedIndex = false;
    for (const auto& segment : config->segments()) {
        auto convention =
            QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(conventions->get(segment.convention()));
        QL_REQUIRE(convention, "InflationSwap Conventions for " << segment.convention() << " not found.");
        interpolatedIndex |= convention->interpolated();
        auto p = getStartAndLag(asof, *convention);
        Date swapStart = p.first;
        if (p.second != 0 * Days) {
            // keep the largest lag across all segments for the curve as observation lag,
            // only relevant if we have multiple segments and publication rules given, otherwise we use lag
            // from the curve config
            obsLagFromSegment = obsLagFromSegment == 0 * Days ? p.second : std::max(obsLagFromSegment, p.second);
        }
        QL_REQUIRE(zcIndex == nullptr || zcIndex == convention->index(),
                   "all segments must use the same zero inflation index");
        zcIndex = convention->index();
        index = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zcIndex, convention->interpolated());
        for (const auto& q : segment.quotes()) {
            auto md = loader.get(q, asof);
            QL_REQUIRE(md, "MarketDatum " << md << " required to build inflation curve " << config->curveID()
                                          << " not found in market data for date " << asof);
            QL_REQUIRE(md->asofDate() == asof,
                       "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
            QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP && deriveFromZC ||
                           md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONSWAP && !deriveFromZC,
                       "MarketDatum " << md << " is not a valid inflation swap quote");
            Handle<Quote> quote;
            Period term;
            if (!deriveFromZC) {
                auto yyq = QuantLib::ext::dynamic_pointer_cast<YoYInflationSwapQuote>(md);
                QL_REQUIRE(yyq, "Could not cast to YoYInflationSwapQuote, internal error.");
                quote = yyq->quote();
                term = yyq->term();
            } else {
                auto zcq = QuantLib::ext::dynamic_pointer_cast<ZcInflationSwapQuote>(md);
                QL_REQUIRE(zcq, "Could not cast to ZcInflationSwapQuote, internal error.");
                quote = computeFairYoYQuote(swapStart, swapStart + zcq->term(), convention, zcIndex, zcCurve, nominalTs,
                                            zcq->term(), zcq->quote()->value());
                term = zcq->term();
            }
            Date maturity = swapStart + term;
            results.latestMaturity =
                results.latestMaturity == Date() ? maturity : std::max(results.latestMaturity, maturity);
            QuantLib::ext::shared_ptr<YoYInflationTraits::helper> instrument =
                QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(
                    quote, convention->observationLag(), maturity, convention->fixCalendar(),
                    convention->fixConvention(), convention->dayCounter(), index, nominalTs, swapStart);
            results.pillarDates.push_back(instrument->pillarDate());

            // Unregister with inflation index (and evaluationDate). See PR #326 for details.
            instrument->unregisterWithAll();
            instrument->registerWith(nominalTs);
            instrument->registerWith(quote);

            helpers.push_back(instrument);
        }
    }
    auto curveObsLag = obsLagFromSegment != 0 * Days ? obsLagFromSegment : config->lag();
    // base zero rate: if given, take it, otherwise set it to first quote
    Real baseRate = config->baseRate() != Null<Real>() ? config->baseRate() : helpers.front()->quote()->value();
    Date baseDate = QuantExt::ZeroInflation::curveBaseDate(false, asof, curveObsLag, config->frequency(), index);

    QL_DEPRECATED_DISABLE_WARNING
    results.curve = QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<Linear>>(new PiecewiseYoYInflationCurve<Linear>(
        asof, baseDate, baseRate, curveObsLag, config->frequency(), interpolatedIndex, config->dayCounter(),
        helpers, {}, config->tolerance()));
    QL_DEPRECATED_ENABLE_WARNING
    results.index = zcIndex;
    
    return results;
}

QuantLib::Handle<QuantLib::Quote>
InflationCurve::computeFairYoYQuote(const QuantLib::Date& swapStart, const QuantLib::Date& maturity,
                                    const QuantLib::ext::shared_ptr<InflationSwapConvention>& conv,
                                    const QuantLib::ext::shared_ptr<ZeroInflationIndex>& ziIndex,
                                    const QuantLib::ext::shared_ptr<InflationTermStructure>& zcCurve,
                                    const Handle<YieldTermStructure>& nominalTs,
                                    const QuantLib::Period term, const double zcQuote) const {
    auto conversionIndex = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(
        ziIndex->clone(Handle<ZeroInflationTermStructure>(
            QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(zcCurve))),
        conv->interpolated());
    QuantLib::ext::shared_ptr<InflationCouponPricer> yoyCpnPricer =
        QuantLib::ext::make_shared<YoYInflationCouponPricer>(nominalTs);
    // construct a yoy swap just as it is done in the yoy inflation helper
    Schedule schedule = MakeSchedule()
                            .from(swapStart)
                            .to(maturity)
                            .withTenor(1 * Years)
                            .withConvention(Unadjusted)
                            .withCalendar(conv->fixCalendar())
                            .backwards();
    QL_DEPRECATED_DISABLE_WARNING
    YearOnYearInflationSwap tmp(YearOnYearInflationSwap::Payer, 1000000.0, schedule, 0.02, conv->dayCounter(), schedule,
                                conversionIndex, conv->observationLag(), 0.0, conv->dayCounter(), conv->fixCalendar(),
                                conv->fixConvention());
    QL_DEPRECATED_ENABLE_WARNING
    for (auto& c : tmp.yoyLeg()) {
        auto cpn = QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(c);
        QL_REQUIRE(cpn, "yoy inflation coupon expected, could not cast");
        cpn->setPricer(yoyCpnPricer);
    }
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(nominalTs);
    tmp.setPricingEngine(engine);
    auto yoyRate = tmp.fairRate();
    DLOG("Derive " << term << " yoy quote " << yoyRate << " from zc curve and zc quote " << zcQuote);
    return Handle<Quote>(ext::make_shared<SimpleQuote>(yoyRate));
}

} // namespace data
} // namespace ore
