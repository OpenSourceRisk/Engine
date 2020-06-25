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
#include <ored/utilities/log.hpp>
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
    CalibrationFunction(vector<Handle<Quote>>& correlations, const vector<boost::shared_ptr<QuantExt::CmsCapHelper>>& h,
                        const vector<Real>& weights, const Projection& projection)
        : correlations_(correlations), instruments_(h), weights_(weights), projection_(projection) {}

    virtual ~CalibrationFunction() {}

    virtual Real value(const Array& params) const {

        for (Size i = 0; i < correlations_.size(); i++) {
            boost::shared_ptr<SimpleQuote> q = boost::dynamic_pointer_cast<SimpleQuote>(*correlations_[i]);
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

    virtual Disposable<Array> values(const Array& params) const {
        for (Size i = 0; i < correlations_.size(); i++) {
            boost::shared_ptr<SimpleQuote> q = boost::dynamic_pointer_cast<SimpleQuote>(*correlations_[i]);
            q->setValue(params[i]);
        }
        Array values(instruments_.size());
        for (Size i = 0; i < instruments_.size(); i++) {
            values[i] = instruments_[i]->calibrationError() * std::sqrt(weights_[i]);
        }
        return values;
    }

    virtual Real finiteDifferenceEpsilon() const { return 1e-6; }

private:
    vector<Handle<Quote>> correlations_;
    mutable vector<boost::shared_ptr<QuantExt::CmsCapHelper>> instruments_;
    vector<Real> weights_;
    const Projection projection_;
};

void CorrelationCurve::calibrateCMSSpreadCorrelations(
    const boost::shared_ptr<CorrelationCurveConfig>& config, Date asof, const vector<Handle<Quote>>& prices,
    vector<Handle<Quote>>& correlations, boost::shared_ptr<QuantExt::CorrelationTermStructure>& curve,
    const Conventions& conventions, map<string, boost::shared_ptr<SwapIndex>>& swapIndices,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    map<string, boost::shared_ptr<SwaptionVolCurve>>& swaptionVolCurves) {

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
    Handle<Quote> revQuote(boost::shared_ptr<Quote>(new SimpleQuote(rev)));

    boost::shared_ptr<QuantLib::CmsCouponPricer> cmsPricer =
        boost::make_shared<LinearTsrPricer>(vol, revQuote, yts, settings);

    // build cms spread pricer
    Handle<QuantExt::CorrelationTermStructure> ch(curve);
    boost::shared_ptr<FloatingRateCouponPricer> pricer =
        boost::make_shared<QuantExt::LognormalCmsSpreadPricer>(cmsPricer, ch, yts, 16);

    // build instruments
    auto it3 = swapIndices.find(config->index1());
    if (it3 == swapIndices.end())
        QL_FAIL("The swap index not found " << config->index1());

    boost::shared_ptr<SwapIndex> index1 = it3->second;

    auto it4 = swapIndices.find(config->index2());
    if (it4 == swapIndices.end())
        QL_FAIL("The swap index not found " << config->index2());

    boost::shared_ptr<SwapIndex> index2 = it4->second;

    vector<boost::shared_ptr<QuantExt::CmsCapHelper>> instruments;

    boost::shared_ptr<Convention> tmp = conventions.get(config->conventions());
    QL_REQUIRE(tmp, "no conventions found with id " << config->conventions());

    boost::shared_ptr<CmsSpreadOptionConvention> conv = boost::dynamic_pointer_cast<CmsSpreadOptionConvention>(tmp);
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
        boost::shared_ptr<QuantExt::CmsCapHelper> inst = boost::make_shared<QuantExt::CmsCapHelper>(
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

    boost::shared_ptr<OptimizationMethod> method = boost::make_shared<LevenbergMarquardt>(1E-8, 1E-8, 1E-8);

    CalibrationFunction c(correlations, instruments, weights, proj);

    Problem prob(c, pc, proj.project(prms));

    method->minimize(prob, endCriteria);

    Array result(prob.currentValue());
}

CorrelationCurve::CorrelationCurve(Date asof, CorrelationCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs, const Conventions& conventions,
                                   map<string, boost::shared_ptr<SwapIndex>>& swapIndices,
                                   map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                   map<string, boost::shared_ptr<SwaptionVolCurve>>& swaptionVolCurves) {

    try {

        const boost::shared_ptr<CorrelationCurveConfig>& config =
            curveConfigs.correlationCurveConfig(spec.curveConfigID());

        // build default correlation termsructure
        boost::shared_ptr<QuantExt::CorrelationTermStructure> corr;

        if (config->quoteType() == CorrelationCurveConfig::QuoteType::Null) {

            corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(
                new QuantExt::FlatCorrelation(0, config->calendar(), 0.0, config->dayCounter()));

        } else {

            QL_REQUIRE(config->dimension() == CorrelationCurveConfig::Dimension::ATM ||
                           config->dimension() == CorrelationCurveConfig::Dimension::Constant,
                       "Unsupported correlation curve building dimension");

            vector<Period> optionTenors = parseVectorOfValues<Period>(config->optionTenors(), &parsePeriod);
            vector<Handle<Quote>> quotes(optionTenors.size());
            bool failed = false;

            // search market data loader for quotes, logging missing ones
            for (auto& q : config->quotes()) {
                if (loader.has(q, asof)) {
                    boost::shared_ptr<CorrelationQuote> c =
                        boost::dynamic_pointer_cast<CorrelationQuote>(loader.get(q, asof));

                    Size i = std::find(optionTenors.begin(), optionTenors.end(), parsePeriod(c->expiry())) -
                             optionTenors.begin();
                    QL_REQUIRE(i < optionTenors.size(), "correlation tenor not found for " << q);
                    quotes[i] = c->quote();
                } else {
                    DLOGGERSTREAM << "could not find correlation quote " << q << std::endl;
                    failed = true;
                }
            }

            // fail if any quotes missing
            if (failed) {
                QL_FAIL("could not build correlation curve");
            }

            vector<Handle<Quote>> corrs;

            for (Size i = 0; i < optionTenors.size(); i++) {
                if (config->quoteType() == CorrelationCurveConfig::QuoteType::Rate) {
                    corrs.push_back(quotes[i]);
                } else {
                    Handle<Quote> q(boost::make_shared<SimpleQuote>(0));
                    corrs.push_back(q);
                }
            }

            // build correlation termsructure
            bool flat = (config->dimension() == CorrelationCurveConfig::Dimension::Constant);
            LOG("building " << (flat ? "flat" : "interpolated curve") << " correlation termstructure");

            if (flat) {
                corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(
                    new QuantExt::FlatCorrelation(0, config->calendar(), corrs[0], config->dayCounter()));

                corr->enableExtrapolation(config->extrapolate());
            } else {
                vector<Real> times(optionTenors.size());

                for (Size i = 0; i < optionTenors.size(); ++i) {
                    Date d = config->calendar().advance(asof, optionTenors[i], config->businessDayConvention());

                    times[i] = config->dayCounter().yearFraction(asof, d);

                    LOG("asof " << asof << ", tenor " << optionTenors[i] << ", date " << d << ", time " << times[i]);
                }

                corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(
                    new QuantExt::InterpolatedCorrelationCurve<Linear>(times, corrs, config->dayCounter(),
                                                                       config->calendar()));
            }

            if (config->quoteType() == CorrelationCurveConfig::QuoteType::Price) {

                if (config->correlationType() == CorrelationCurveConfig::CorrelationType::CMSSpread) {
                    calibrateCMSSpreadCorrelations(config, asof, quotes, corrs, corr, conventions, swapIndices,
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
