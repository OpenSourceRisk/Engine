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

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <qle/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/utilities/inflation.hpp>
#include <algorithm>

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

        const QuantLib::ext::shared_ptr<InflationCurveConfig>& config = curveConfigs.inflationCurveConfig(spec.curveConfigID());

        const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
        QuantLib::ext::shared_ptr<InflationSwapConvention> conv =
            QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(conventions->get(config->conventions()));

        QL_REQUIRE(conv != nullptr, "convention " << config->conventions() << " could not be found.");

        Handle<YieldTermStructure> nominalTs;
        auto it = yieldCurves.find(config->nominalTermStructure());
        if (it != yieldCurves.end()) {
            nominalTs = it->second->handle();
        } else {
            QL_FAIL("The nominal term structure, " << config->nominalTermStructure()
                                                   << ", required in the building "
                                                      "of the curve, "
                                                   << spec.name() << ", was not found.");
        }

        // We loop over all market data, looking for quotes that match the configuration

        const std::vector<string> strQuotes = config->swapQuotes();
        std::vector<Handle<Quote>> quotes(strQuotes.size(), Handle<Quote>());
        std::vector<Period> terms(strQuotes.size());
        std::vector<bool> isZc(strQuotes.size(), true);

        std::ostringstream ss1;
        ss1 << MarketDatum::InstrumentType::ZC_INFLATIONSWAP << "/*";
        Wildcard w1(ss1.str());
        auto data1 = loader.get(w1, asof);

        std::ostringstream ss2;
        ss2 << MarketDatum::InstrumentType::YY_INFLATIONSWAP << "/*";
        Wildcard w2(ss2.str());
        auto data2 = loader.get(w2, asof);

        data1.merge(data2);

        for (const auto& md : data1) {

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            if ((md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP ||
                (md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONSWAP &&
                 config->type() == InflationCurveConfig::Type::YY))) {

                QuantLib::ext::shared_ptr<ZcInflationSwapQuote> q = QuantLib::ext::dynamic_pointer_cast<ZcInflationSwapQuote>(md);
                if (q) {
                    auto it = std::find(strQuotes.begin(), strQuotes.end(), q->name());
                    if (it != strQuotes.end()) {
                        QL_REQUIRE(quotes[it - strQuotes.begin()].empty(), "duplicate quote " << q->name());
                        quotes[it - strQuotes.begin()] = q->quote();
                        terms[it - strQuotes.begin()] = q->term();
                        isZc[it - strQuotes.begin()] = true;
                    }
                }

                QuantLib::ext::shared_ptr<YoYInflationSwapQuote> q2 = QuantLib::ext::dynamic_pointer_cast<YoYInflationSwapQuote>(md);
                if (q2) {
                    auto it = std::find(strQuotes.begin(), strQuotes.end(), q2->name());
                    if (it != strQuotes.end()) {
                        QL_REQUIRE(quotes[it - strQuotes.begin()].empty(), "duplicate quote " << q2->name());
                        quotes[it - strQuotes.begin()] = q2->quote();
                        terms[it - strQuotes.begin()] = q2->term();
                        isZc[it - strQuotes.begin()] = false;
                    }
                }
            }
        }

        // do we have all quotes and do we derive yoy quotes from zc ?
        for (Size i = 0; i < strQuotes.size(); ++i) {
            QL_REQUIRE(!quotes[i].empty(), "quote " << strQuotes[i] << " not found in market data.");
            QL_REQUIRE(isZc[i] == isZc[0], "mixed zc and yoy quotes");
        }
        bool derive_yoy_from_zc = (config->type() == InflationCurveConfig::Type::YY && isZc[0]);

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
                                       << config->seasonalityFrequency() << " with " << strFactorIDs.size()
                                       << " factors.");
                        QuantLib::ext::shared_ptr<SeasonalityQuote> sq =
                            QuantLib::ext::dynamic_pointer_cast<SeasonalityQuote>(marketQuote);
                            QL_REQUIRE(sq, "Could not cast to SeasonalityQuote, internal error.");
                            QL_REQUIRE(sq->type() == "MULT",
                                   "Market quote (" << sq->name() << ") not of multiplicative type.");
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
                seasonality = QuantLib::ext::make_shared<MultiplicativePriceSeasonality>(config->seasonalityBaseDate(),
                                                                                 config->seasonalityFrequency(),
                                                                                 config->overrideSeasonalityFactors());
            }
        }

        // Get helper start date and curve's obs lag.
        auto p = getStartAndLag(asof, *conv);
        Date swapStart = p.first;
        Period curveObsLag = p.second == Period() ? config->lag() : p.second;

        // construct curve (ZC or YY depending on configuration)
        std::vector<Date> pillarDates;

        interpolatedIndex_ = conv->interpolated();
        CPI::InterpolationType observationInterpolation = interpolatedIndex_ ? CPI::Linear : CPI::Flat;
        QuantLib::ext::shared_ptr<YoYInflationIndex> zc_to_yoy_conversion_index;
        if (config->type() == InflationCurveConfig::Type::ZC || derive_yoy_from_zc) {
            // ZC Curve
            std::vector<QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper>> instruments;
            QuantLib::ext::shared_ptr<ZeroInflationIndex> index = conv->index();
            for (Size i = 0; i < strQuotes.size(); ++i) {
                // QL conventions do not incorporate settlement delay => patch here once QL is patched
                Date maturity = swapStart + terms[i];
                QuantLib::ext::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
                    QuantLib::ext::make_shared<ZeroCouponInflationSwapHelper>(
                        quotes[i], conv->observationLag(), maturity, conv->fixCalendar(), conv->fixConvention(),
                        conv->dayCounter(), index, observationInterpolation, nominalTs, swapStart);
                // The instrument gets registered to update on change of evaluation date. This triggers a
                // rebootstrapping of the curve. In order to avoid this during simulation we unregister from the
                // evaluationDate.
                instrument->unregisterWith(Settings::instance().evaluationDate());
                instruments.push_back(instrument);
            }
            // base zero / yoy rate: if given, take it, otherwise set it to observered zeroRate
            Real baseRate = quotes[0]->value();
            if (config->baseRate() != Null<Real>()) {
                baseRate = config->baseRate();
            } else if (index) {
                try {
                    baseRate = QuantExt::ZeroInflation::guessCurveBaseRate(
                     config->useLastAvailableFixingAsBaseDate(), swapStart, asof, terms[0], conv->dayCounter(),
                     conv->observationLag(), quotes[0]->value(), curveObsLag, config->dayCounter(), index,
                        interpolatedIndex_, seasonality);
                } catch (const std::exception& e) {
                    WLOG("base rate estimation failed with " << e.what() << ", fallback to use first quote");
                    baseRate = quotes[0]->value();
                }
            }
            curve_ = QuantLib::ext::shared_ptr<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
                new QuantExt::PiecewiseZeroInflationCurve<Linear>(
                    asof, config->calendar(), config->dayCounter(), curveObsLag, config->frequency(), baseRate,
                    instruments, config->tolerance(), index, config->useLastAvailableFixingAsBaseDate()));
            
            // force bootstrap so that errors are thrown during the build, not later
            QuantLib::ext::static_pointer_cast<QuantExt::PiecewiseZeroInflationCurve<Linear>>(curve_)->zeroRate(QL_EPSILON);
            if (derive_yoy_from_zc) {
                // set up yoy wrapper with empty ts, so that zero index is used to forecast fixings
                // for this link the appropriate curve to the zero index
                zc_to_yoy_conversion_index = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(
                    index->clone(Handle<ZeroInflationTermStructure>(
                        QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(curve_))),
                    interpolatedIndex_);
            }
        }
        if (config->type() == InflationCurveConfig::Type::YY) {
            // YOY Curve
            std::vector<QuantLib::ext::shared_ptr<YoYInflationTraits::helper>> instruments;
            QuantLib::ext::shared_ptr<ZeroInflationIndex> zcindex = conv->index();
            QuantLib::ext::shared_ptr<YoYInflationIndex> index =
                QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zcindex, interpolatedIndex_);
            QuantLib::ext::shared_ptr<InflationCouponPricer> yoyCpnPricer =
                QuantLib::ext::make_shared<YoYInflationCouponPricer>(nominalTs);
            for (Size i = 0; i < strQuotes.size(); ++i) {
                Date maturity = swapStart + terms[i];
                Real effectiveQuote = quotes[i]->value();
                if (derive_yoy_from_zc) {
                    // construct a yoy swap just as it is done in the yoy inflation helper
                    Schedule schedule = MakeSchedule()
                                            .from(swapStart)
                                            .to(maturity)
                                            .withTenor(1 * Years)
                                            .withConvention(Unadjusted)
                                            .withCalendar(conv->fixCalendar())
                                            .backwards();
                    YearOnYearInflationSwap tmp(YearOnYearInflationSwap::Payer, 1000000.0, schedule, 0.02,
                                                conv->dayCounter(), schedule, zc_to_yoy_conversion_index,
                                                conv->observationLag(), 0.0, conv->dayCounter(), conv->fixCalendar(),
                                                conv->fixConvention());
                    for (auto& c : tmp.yoyLeg()) {
                        auto cpn = QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(c);
                        QL_REQUIRE(cpn, "yoy inflation coupon expected, could not cast");
                        cpn->setPricer(yoyCpnPricer);
                    }
                    QuantLib::ext::shared_ptr<PricingEngine> engine =
                        QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(nominalTs);
                    tmp.setPricingEngine(engine);
                    effectiveQuote = tmp.fairRate();
                    DLOG("Derive " << terms[i] << " yoy quote " << effectiveQuote << " from zc quote "
                                   << quotes[i]->value());
                }
                // QL conventions do not incorporate settlement delay => patch here once QL is patched
                QuantLib::ext::shared_ptr<YoYInflationTraits::helper> instrument =
                    QuantLib::ext::make_shared<YearOnYearInflationSwapHelper>(
                        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(effectiveQuote)), conv->observationLag(),
                        maturity, conv->fixCalendar(), conv->fixConvention(), conv->dayCounter(), index, nominalTs,
                        swapStart);
                instrument->unregisterWith(Settings::instance().evaluationDate());
                instruments.push_back(instrument);
                pillarDates.push_back(instrument->pillarDate());
            }
            // base zero rate: if given, take it, otherwise set it to first quote
            Real baseRate =
                config->baseRate() != Null<Real>() ? config->baseRate() : instruments.front()->quote()->value();
            curve_ = QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<Linear>>(new PiecewiseYoYInflationCurve<Linear>(
                asof, config->calendar(), config->dayCounter(), curveObsLag, config->frequency(), interpolatedIndex_,
                baseRate, instruments, config->tolerance()));
            // force bootstrap so that errors are thrown during the build, not later
            QuantLib::ext::static_pointer_cast<PiecewiseYoYInflationCurve<Linear>>(curve_)->yoyRate(QL_EPSILON);
        }
        if (seasonality != nullptr) {
            curve_->setSeasonality(seasonality);
        }
        curve_->enableExtrapolation(config->extrapolate());
        curve_->unregisterWith(Settings::instance().evaluationDate());

        // set calibration info

        if (buildCalibrationInfo) {

            if (pillarDates.empty()) {
                // default: fill pillar dates with monthly schedule up to last term given in the curve config (but max
                // 60y)
                Date maturity = swapStart + terms.back();
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
                auto zcIndex = conv->index()->clone(Handle<ZeroInflationTermStructure>(zcCurve));
                auto calInfo = QuantLib::ext::make_shared<ZeroInflationCurveCalibrationInfo>();
                calInfo->dayCounter = config->dayCounter().name();
                calInfo->calendar = config->calendar().empty() ? "null" : config->calendar().name();
                calInfo->baseDate = curve_->baseDate();
                auto lim = inflationPeriod(curve_->baseDate(), curve_->frequency());
                try {
                    calInfo->baseCpi = conv->index()->fixing(lim.first);
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

} // namespace data
} // namespace ore
