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
#include <ored/utilities/log.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/inflation/piecewiseyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;
using namespace ore::data;

namespace ore {
namespace data {

InflationCurve::InflationCurve(Date asof, InflationCurveSpec spec, const Loader& loader,
                               const CurveConfigurations& curveConfigs, const Conventions& conventions,
                               map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    try {

        const boost::shared_ptr<InflationCurveConfig>& config = curveConfigs.inflationCurveConfig(spec.curveConfigID());

        boost::shared_ptr<InflationSwapConvention> conv =
            boost::dynamic_pointer_cast<InflationSwapConvention>(conventions.get(config->conventions()));

        QL_REQUIRE(conv != nullptr, "convention " << config->conventions() << " could not be found.");

        Handle<YieldTermStructure> nominalTs;
        auto it = yieldCurves.find(config->nominalTermStructure());
        if (it != yieldCurves.end()) {
            nominalTs = it->second->handle();
        } else {
            QL_FAIL("The nominal term structure, " << config->nominalTermStructure() << ", required in the building "
                                                                                        "of the curve, "
                                                   << spec.name() << ", was not found.");
        }

        // We loop over all market data, looking for quotes that match the configuration

        const std::vector<string> strQuotes = config->quotes();
        std::vector<Handle<Quote>> quotes(strQuotes.size(), Handle<Quote>());
        std::vector<Period> terms(strQuotes.size());
        std::vector<bool> isZc(strQuotes.size(), true);

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && (md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONSWAP ||
                                           (md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONSWAP &&
                                            config->type() == InflationCurveConfig::Type::YY))) {

                boost::shared_ptr<ZcInflationSwapQuote> q = boost::dynamic_pointer_cast<ZcInflationSwapQuote>(md);
                if (q != NULL && q->index() == spec.index()) {
                    auto it = std::find(strQuotes.begin(), strQuotes.end(), q->name());
                    if (it != strQuotes.end()) {
                        QL_REQUIRE(quotes[it - strQuotes.begin()].empty(), "duplicate quote " << q->name());
                        quotes[it - strQuotes.begin()] = q->quote();
                        terms[it - strQuotes.begin()] = q->term();
                        isZc[it - strQuotes.begin()] = true;
                    }
                }

                boost::shared_ptr<YoYInflationSwapQuote> q2 = boost::dynamic_pointer_cast<YoYInflationSwapQuote>(md);
                if (q2 != NULL && q2->index() == spec.index()) {
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
        boost::shared_ptr<Seasonality> seasonality;
        if (config->seasonalityBaseDate() != Null<Date>()) {
            std::vector<string> strFactorIDs = config->seasonalityFactors();
            std::vector<double> factors(strFactorIDs.size());
            for (Size i = 0; i < strFactorIDs.size(); i++) {
                boost::shared_ptr<MarketDatum> marketQuote = loader.get(strFactorIDs[i], asof);

                // Check that we have a valid seasonality factor
                if (marketQuote) {
                    QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::SEASONALITY,
                               "Market quote (" << marketQuote->name() << ") not of type seasonality.");
                    // Currently only monthly seasonality with 12 multiplicative factors os allowed
                    QL_REQUIRE(config->seasonalityFrequency() == Monthly && strFactorIDs.size() == 12,
                               "Only monthly seasonality with 12 factors is allowed. Provided "
                                   << config->seasonalityFrequency() << " with " << strFactorIDs.size() << " factors.");
                    boost::shared_ptr<SeasonalityQuote> sq = boost::dynamic_pointer_cast<SeasonalityQuote>(marketQuote);
                    QL_REQUIRE(sq->type() == "MULT", "Market quote (" << sq->name() << ") not of multiplicative type.");
                    int findex = config->seasonalityBaseDate().month() - sq->applyMonth() < 0
                                     ? config->seasonalityBaseDate().month() - sq->applyMonth() + 12
                                     : config->seasonalityBaseDate().month() - sq->applyMonth();
                    factors[findex] = sq->quote()->value();
                } else {
                    QL_FAIL("Could not find quote for ID " << strFactorIDs[i] << " with as of date "
                                                           << io::iso_date(asof) << ".");
                }
            }
            QL_REQUIRE(!factors.empty(), "no seasonality factors found");
            seasonality = boost::make_shared<MultiplicativePriceSeasonality>(config->seasonalityBaseDate(),
                                                                             config->seasonalityFrequency(), factors);
        }

        // construct curve (ZC or YY depending on configuration)

        // base zero / yoy rate: if given, take it, otherwise set it to first quote
        Real baseRate = config->baseRate() != Null<Real>() ? config->baseRate() : quotes[0]->value();

        interpolatedIndex_ = conv->interpolated();

        boost::shared_ptr<YoYInflationIndex> zc_to_yoy_conversion_index;
        if (config->type() == InflationCurveConfig::Type::ZC || derive_yoy_from_zc) {
            // ZC Curve
            std::vector<boost::shared_ptr<ZeroInflationTraits::helper>> instruments;
            boost::shared_ptr<ZeroInflationIndex> index = conv->index();
            for (Size i = 0; i < strQuotes.size(); ++i) {
                // QL conventions do not incorporate settlement delay => patch here once QL is patched
                Date maturity = asof + terms[i];
                instruments.push_back(boost::make_shared<ZeroCouponInflationSwapHelper>(
                    quotes[i], conv->observationLag(), maturity, conv->fixCalendar(), conv->fixConvention(),
                    conv->dayCounter(), index));
            }
            curve_ = boost::shared_ptr<PiecewiseZeroInflationCurve<Linear>>(new PiecewiseZeroInflationCurve<Linear>(
                asof, config->calendar(), config->dayCounter(), config->lag(), config->frequency(), interpolatedIndex_,
                baseRate, nominalTs, instruments, config->tolerance()));
            if (derive_yoy_from_zc) {
                // set up yoy wrapper with empty ts, so that zero index is used to forecast fixings
                // for this link the appropriate curve to the zero index
                zc_to_yoy_conversion_index = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                    index->clone(Handle<ZeroInflationTermStructure>(
                        boost::dynamic_pointer_cast<ZeroInflationTermStructure>(curve_))),
                    interpolatedIndex_);
            }
        }

        if (config->type() == InflationCurveConfig::Type::YY) {
            // YOY Curve
            std::vector<boost::shared_ptr<YoYInflationTraits::helper>> instruments;
            boost::shared_ptr<ZeroInflationIndex> zcindex = conv->index();
            boost::shared_ptr<YoYInflationIndex> index =
                boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zcindex, interpolatedIndex_);
            boost::shared_ptr<InflationCouponPricer> yoyCpnPricer =
                boost::make_shared<QuantExt::YoYInflationCouponPricer2>(nominalTs);
            for (Size i = 0; i < strQuotes.size(); ++i) {
                Date maturity = asof + terms[i];
                Real effectiveQuote = quotes[i]->value();
                if (derive_yoy_from_zc) {
                    // contruct a yoy swap just as it is done in the yoy inflation helper
                    Schedule schedule = MakeSchedule()
                                            .from(Settings::instance().evaluationDate())
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
                        auto cpn = boost::dynamic_pointer_cast<YoYInflationCoupon>(c);
                        QL_REQUIRE(cpn, "yoy inflation coupon expected, could not cast");
                        cpn->setPricer(yoyCpnPricer);
                    }
                    boost::shared_ptr<PricingEngine> engine =
                        boost::make_shared<QuantLib::DiscountingSwapEngine>(nominalTs);
                    tmp.setPricingEngine(engine);
                    effectiveQuote = tmp.fairRate();
                    DLOG("Derive " << terms[i] << " yoy quote " << effectiveQuote << " from zc quote "
                                   << quotes[i]->value());
                }
                // QL conventions do not incorporate settlement delay => patch here once QL is patched
                instruments.push_back(boost::make_shared<YearOnYearInflationSwapHelper>(
                    Handle<Quote>(boost::make_shared<SimpleQuote>(effectiveQuote)), conv->observationLag(), maturity,
                    conv->fixCalendar(), conv->fixConvention(), conv->dayCounter(), index));
            }
            // base zero rate: if given, take it, otherwise set it to first quote
            Real baseRate = config->baseRate() != Null<Real>() ? config->baseRate() : quotes[0]->value();
            curve_ = boost::shared_ptr<PiecewiseYoYInflationCurve<Linear>>(new PiecewiseYoYInflationCurve<Linear>(
                asof, config->calendar(), config->dayCounter(), config->lag(), config->frequency(), interpolatedIndex_,
                baseRate, nominalTs, instruments, config->tolerance()));
        }

        if (seasonality != nullptr) {
            curve_->setSeasonality(seasonality);
        }
        curve_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("inflation curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("inflation curve building failed: unknown error");
    }
}
}
}
