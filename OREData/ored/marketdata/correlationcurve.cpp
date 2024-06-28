/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <algorithm>
#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/correlationcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/cashflows/lognormalcmsspreadpricer.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/optimization/problem.hpp>
#include <ql/math/optimization/projectedconstraint.hpp>
#include <ql/math/optimization/projection.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/models/cmscaphelper.hpp>
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

class CorrelationCurve::CalibrationFunction : public CostFunction {
public:
    CalibrationFunction(vector<Handle<Quote>>& correlations, const vector<QuantLib::ext::shared_ptr<QuantExt::CmsCapHelper>>& h,
                        const vector<Real>& weights, const Projection& projection)
        : correlations_(correlations), instruments_(h), weights_(weights), projection_(projection) {}

    virtual ~CalibrationFunction() {}

    virtual Real value(const Array& params) const override {

        for (Size i = 0; i < correlations_.size(); i++) {
            QuantLib::ext::shared_ptr<SimpleQuote> q = QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*correlations_[i]);
            q->setValue(params[i]);
            LOG("set corr " << params[i]);
        }
        Real value = 0.0;
        for (Size i = 0; i < instruments_.size(); i++) {
            Real diff = instruments_[i]->calibrationError();
            value += diff * diff * weights_[i];
        }
        return std::sqrt(value);
    }

    virtual Array values(const Array& params) const override {
        for (Size i = 0; i < correlations_.size(); i++) {
            QuantLib::ext::shared_ptr<SimpleQuote> q = QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*correlations_[i]);
            q->setValue(params[i]);
        }
        Array values(instruments_.size());
        for (Size i = 0; i < instruments_.size(); i++) {
            values[i] = instruments_[i]->calibrationError() * std::sqrt(weights_[i]);
        }
        return values;
    }

    virtual Real finiteDifferenceEpsilon() const override { return 1e-6; }

private:
    vector<Handle<Quote>> correlations_;
    mutable vector<QuantLib::ext::shared_ptr<QuantExt::CmsCapHelper>> instruments_;
    vector<Real> weights_;
    const Projection projection_;
};

void CorrelationCurve::calibrateCMSSpreadCorrelations(
    const QuantLib::ext::shared_ptr<CorrelationCurveConfig>& config, Date asof, const vector<Handle<Quote>>& prices,
    vector<Handle<Quote>>& correlations, QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>& curve,
    map<string, QuantLib::ext::shared_ptr<SwapIndex>>& swapIndices, map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
    map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& swaptionVolCurves) {

    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    
    // build cms pricingengine
    string ccy = config->currency();
    string swaptionVol = "SwaptionVolatility/" + ccy + "/" + config->swaptionVolatility();
    auto it = swaptionVolCurves.find(swaptionVol);

    if (it == swaptionVolCurves.end())
        QL_FAIL("The swaption vol curve not found " << swaptionVol);
    Handle<SwaptionVolatilityStructure> vol(it->second->volTermStructure());

    string dc = "Yield/" + ccy + "/" + config->discountCurve();
    Handle<YieldTermStructure> yts;
    auto it2 = yieldCurves.find(dc);
    if (it2 != yieldCurves.end()) {
        yts = it2->second->handle();
    } else {
        QL_FAIL("The discount curve not found, " << dc);
    }

    Real lower = (vol->volatilityType() == ShiftedLognormal) ? 0.0001 : -2;
    Real upper = (vol->volatilityType() == ShiftedLognormal) ? 2.0000 : 2;

    LinearTsrPricer::Settings settings;
    settings.withRateBound(lower, upper);

    Real rev = 0;
    Handle<Quote> revQuote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rev)));

    QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer> cmsPricer =
        QuantLib::ext::make_shared<LinearTsrPricer>(vol, revQuote, yts, settings);

    // build cms spread pricer
    Handle<QuantExt::CorrelationTermStructure> ch(curve);
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> pricer =
        QuantLib::ext::make_shared<QuantExt::LognormalCmsSpreadPricer>(cmsPricer, ch, yts, 16);

    // build instruments
    auto it3 = swapIndices.find(config->index1());
    if (it3 == swapIndices.end())
        QL_FAIL("The swap index not found " << config->index1());

    QuantLib::ext::shared_ptr<SwapIndex> index1 = it3->second;

    auto it4 = swapIndices.find(config->index2());
    if (it4 == swapIndices.end())
        QL_FAIL("The swap index not found " << config->index2());

    QuantLib::ext::shared_ptr<SwapIndex> index2 = it4->second;

    vector<QuantLib::ext::shared_ptr<QuantExt::CmsCapHelper>> instruments;

    QuantLib::ext::shared_ptr<Convention> tmp = conventions->get(config->conventions());
    QL_REQUIRE(tmp, "no conventions found with id " << config->conventions());

    QuantLib::ext::shared_ptr<CmsSpreadOptionConvention> conv = QuantLib::ext::dynamic_pointer_cast<CmsSpreadOptionConvention>(tmp);
    QL_REQUIRE(conv != NULL, "CMS Correlation curves require CMSSpreadOption convention ");
    Period forwardStart = conv->forwardStart();
    Period spotDays = conv->spotDays();
    Period cmsTenor = conv->swapTenor();
    Natural fixingDays = conv->fixingDays();
    Calendar calendar = conv->calendar();
    DayCounter dcount = conv->dayCounter();
    BusinessDayConvention bdc = conv->rollConvention();
    vector<Period> optionTenors = parseVectorOfValues<Period>(config->optionTenors(), &parsePeriod);

    for (Size i = 0; i < prices.size(); i++) {
        QuantLib::ext::shared_ptr<QuantExt::CmsCapHelper> inst = QuantLib::ext::make_shared<QuantExt::CmsCapHelper>(
            asof, index1, index2, yts, prices[i], correlations[i], optionTenors[i], forwardStart, spotDays, cmsTenor,
            fixingDays, calendar, dcount, bdc, pricer, cmsPricer);
        instruments.push_back(inst);
    }

    // set up problem
    EndCriteria endCriteria(1000, 500, 1E-8, 1E-8, 1E-8);
    BoundaryConstraint constraint(-1, 1);
    vector<Real> weights(prices.size(), 1);

    Array prms(prices.size(), 0);
    vector<bool> all(prms.size(), false);
    Projection proj(prms, all);
    ProjectedConstraint pc(constraint, proj);

    QuantLib::ext::shared_ptr<OptimizationMethod> method = QuantLib::ext::make_shared<LevenbergMarquardt>(1E-8, 1E-8, 1E-8);

    CalibrationFunction c(correlations, instruments, weights, proj);

    Problem prob(c, pc, proj.project(prms));

    method->minimize(prob, endCriteria);

    Array result(prob.currentValue());
}

CorrelationCurve::CorrelationCurve(Date asof, CorrelationCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs,
                                   map<string, QuantLib::ext::shared_ptr<SwapIndex>>& swapIndices,
                                   map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                   map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& swaptionVolCurves) {

    try {

        const QuantLib::ext::shared_ptr<CorrelationCurveConfig>& config =
            curveConfigs.correlationCurveConfig(spec.curveConfigID());

        // build default correlation termstructure
        QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> corr;

        if (config->quoteType() == MarketDatum::QuoteType::NONE) {

            corr = QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>(
                new QuantExt::FlatCorrelation(0, config->calendar(), 0.0, config->dayCounter()));

        } else {
            QL_REQUIRE(config->dimension() == CorrelationCurveConfig::Dimension::ATM ||
                           config->dimension() == CorrelationCurveConfig::Dimension::Constant,
                       "Unsupported correlation curve building dimension");

            // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
            // contain exactly one element.
            auto wildcard = getUniqueWildcard(config->quotes());

            // vector with times, quotes pairs
            vector<pair<Real, Handle<Quote>>> quotePairs;
            // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
            if (wildcard) {
                QL_REQUIRE(config->dimension() == CorrelationCurveConfig::Dimension::ATM, 
                    "CorrelationCurve: Wildcards only supported for curve dimension ATM");
                LOG("Have single quote with pattern " << wildcard->pattern());

                // Loop over quotes and process commodity option quotes matching pattern on asof
                for (const auto& md : loader.get(*wildcard, asof)) {

                    QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

                    auto q = QuantLib::ext::dynamic_pointer_cast<CorrelationQuote>(md);
                    if (q && q->quoteType() == config->quoteType()) {

                        TLOG("The quote " << q->name() << " matched the pattern");

                        Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, config->calendar(), config->businessDayConvention());
                        if (expiryDate > asof) {
                            // add a <time, quote> pair
                            quotePairs.push_back(make_pair(config->dayCounter().yearFraction(asof, expiryDate), q->quote()));
                            TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                << setprecision(9) << q->quote()->value() << ")");
                        }
                    }
                }

                if (quotePairs.size() == 0) {
                    Real corr = config->index1() == config->index2() ? 1.0 : 0.0;
                    WLOG("CorrelationCurve: No quotes found for correlation curve: "
                         << config->curveID() << ", continuing with correlation " << corr << ".");
                    corr_ = QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>(
                        new QuantExt::FlatCorrelation(0, config->calendar(), corr, config->dayCounter()));
                    return;
                }

            } else {
                vector<Period> optionTenors = parseVectorOfValues<Period>(config->optionTenors(), &parsePeriod);
                quotePairs.resize(optionTenors.size());
                bool failed = false;

                // search market data loader for quotes, logging missing ones
                for (auto& q : config->quotes()) {
                    if (loader.has(q, asof)) {
                        QuantLib::ext::shared_ptr<CorrelationQuote> c =
                            QuantLib::ext::dynamic_pointer_cast<CorrelationQuote>(loader.get(q, asof));

                        Size i = std::find(optionTenors.begin(), optionTenors.end(), parsePeriod(c->expiry())) -
                            optionTenors.begin();
                        QL_REQUIRE(i < optionTenors.size(), "CorrelationCurve: correlation tenor not found for " << q);

                        Real time;
                        // add the expiry time - don't need for Constant curves
                        if (config->dimension() != CorrelationCurveConfig::Dimension::Constant) {
                            Date d = config->calendar().advance(asof, optionTenors[i], config->businessDayConvention());
                            time = config->dayCounter().yearFraction(asof, d);
                        }
                        quotePairs[i] = make_pair(time, c->quote());

                        TLOG("CorrelationCurve: Added quote " << c->name() << ", tenor " << optionTenors[i] << ", with value "
                            << fixed << setprecision(9) << c->quote()->value() );
                    } else {
                        DLOGGERSTREAM("could not find correlation quote " << q);
                        failed = true;
                    }
                }
                // fail if any quotes missing
                if (failed) {
                    QL_FAIL("could not build correlation curve");
                }
            }
            vector<Handle<Quote>> corrs;

            // sort the quotes
            std::sort(quotePairs.begin(), quotePairs.end());
            vector<Real> times;
            vector<Handle<Quote>> quotes;
            for (Size i = 0; i < quotePairs.size(); i++) {
                if (config->quoteType() == MarketDatum::QuoteType::RATE) {
                    corrs.push_back(quotePairs[i].second);
                } else {
                    Handle<Quote> q(QuantLib::ext::make_shared<SimpleQuote>(0));
                    corrs.push_back(q);
                }
                quotes.push_back(quotePairs[i].second);
                times.push_back(quotePairs[i].first);
            }                       

            // build correlation termstructure
            bool flat = (config->dimension() == CorrelationCurveConfig::Dimension::Constant || quotes.size() == 1);
            LOG("building " << (flat ? "flat" : "interpolated curve") << " correlation termstructure");

            if (flat) {
                corr = QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>(
                    new QuantExt::FlatCorrelation(0, config->calendar(), corrs[0], config->dayCounter()));

                corr->enableExtrapolation(config->extrapolate());
            } else {

                corr = QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>(
                    new QuantExt::InterpolatedCorrelationCurve<Linear>(times, corrs, config->dayCounter(),
                                                                       config->calendar()));
            }

            if (config->quoteType() == MarketDatum::QuoteType::PRICE) {

                if (config->correlationType() == CorrelationCurveConfig::CorrelationType::CMSSpread) {
                    calibrateCMSSpreadCorrelations(config, asof, quotes, corrs, corr, swapIndices,
                                                   yieldCurves, swaptionVolCurves);
                } else {
                    QL_FAIL("price calibration only supported for CMSSpread correlations");
                }
            }

            LOG("Returning correlation surface for config " << spec);
        }
        corr_ = corr;
    } catch (std::exception& e) {
        QL_FAIL("correlation curve building failed for curve " << spec.curveConfigID() << " on date "
                                                               << io::iso_date(asof) << ": " << e.what());
    } catch (...) {
        QL_FAIL("correlation curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
