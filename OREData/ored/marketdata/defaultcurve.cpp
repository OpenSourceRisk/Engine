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

#include <ored/marketdata/defaultcurve.hpp>
#include <ored/utilities/log.hpp>

#include <qle/termstructures/defaultprobabilityhelpers.hpp>

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

DefaultCurve::DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions,
                           map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    try {

        const boost::shared_ptr<DefaultCurveConfig>& config = curveConfigs.defaultCurveConfig(spec.curveConfigID());

        boost::shared_ptr<CdsConvention> cdsConv;
        if (config->type() == DefaultCurveConfig::Type::SpreadCDS ||
            config->type() == DefaultCurveConfig::Type::HazardRate) {
            boost::shared_ptr<Convention> tmp = conventions.get(config->conventionID());
            QL_REQUIRE(tmp, "no conventions found with id " << config->conventionID());
            cdsConv = boost::dynamic_pointer_cast<CdsConvention>(tmp);
            QL_REQUIRE(cdsConv != NULL, "SpreadCDS and HazardRate curves require CDS convention (wrong type)");
        }

        Handle<YieldTermStructure> discountCurve;
        if (config->type() == DefaultCurveConfig::Type::SpreadCDS) {
            auto it = yieldCurves.find(config->discountCurveID());
            if (it != yieldCurves.end()) {
                discountCurve = it->second->handle();
            } else {
                QL_FAIL("The discount curve, " << config->discountCurveID()
                                               << ", required in the building "
                                                  "of the curve, "
                                               << spec.name() << ", was not found.");
            }
        }

        boost::shared_ptr<YieldCurve> benchmarkCurve, sourceCurve;
        if (config->type() == DefaultCurveConfig::Type::Benchmark) {
            auto it = yieldCurves.find(config->benchmarkCurveID());
            if (it != yieldCurves.end()) {
                benchmarkCurve = it->second;
            } else {
                QL_FAIL("The benchmark curve, " << config->benchmarkCurveID()
                                                << ", required in the building "
                                                   "of the curve, "
                                                << spec.name() << ", was not found.");
            }
            auto it2 = yieldCurves.find(config->sourceCurveID());
            if (it2 != yieldCurves.end()) {
                sourceCurve = it2->second;
            } else {
                QL_FAIL("The source curve, " << config->sourceCurveID()
                                             << ", required in the building "
                                                "of the curve, "
                                             << spec.name() << ", was not found.");
            }
        }

        // Get the market points listed in the curve config
        // Get the recovery rate if needed
        recoveryRate_ = Null<Real>();
        if (!config->recoveryRateQuote().empty()) {
            QL_REQUIRE(loader.has(config->recoveryRateQuote(), asof), 
                "There is no market data for the requested recovery rate " << config->recoveryRateQuote());
            recoveryRate_ = loader.get(config->recoveryRateQuote(), asof)->quote()->value();
        }

        // Get the spread/hazard rate quotes
        map<Period, Real> quotes;
        if (config->type() == DefaultCurveConfig::Type::SpreadCDS || 
            config->type() == DefaultCurveConfig::Type::HazardRate) {
            for (const auto& p : config->cdsQuotes()) {
                if (boost::shared_ptr<MarketDatum> md = loader.get(p, asof)) {
                    Period tenor;
                    if (config->type() == DefaultCurveConfig::Type::SpreadCDS) {
                        tenor = boost::dynamic_pointer_cast<CdsSpreadQuote>(md)->term();
                    } else {
                        tenor = boost::dynamic_pointer_cast<HazardRateQuote>(md)->term();
                    }
                    
                    // Add to quotes, with a check that we have no duplicate tenors
                    auto res = quotes.insert(make_pair(tenor, md->quote()->value()));
                    QL_REQUIRE(res.second, "duplicate term in quotes found ("
                        << tenor << ") while loading default curve " << config->curveID());
                }
            }

            QL_REQUIRE(!quotes.empty(), "No market points found for curve config " << config->curveID());
            LOG("DefaultCurve: using " << quotes.size() << " default quotes of " << config->cdsQuotes().size() << " requested quotes.");
        }

        if (config->type() == DefaultCurveConfig::Type::SpreadCDS) {
            QL_REQUIRE(recoveryRate_ != Null<Real>(), "DefaultCurve: no recovery rate given for type "
                                                      "SpreadCDS");
            std::vector<boost::shared_ptr<DefaultProbabilityHelper>> helper;
            for (auto quote : quotes) {
                helper.push_back(boost::make_shared<QuantExt::SpreadCdsHelper>(
                    quote.second, quote.first, cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
                    cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate_, discountCurve,
                    asof + cdsConv->settlementDays(), cdsConv->settlesAccrual(), cdsConv->paysAtDefaultTime()));
            }
            boost::shared_ptr<DefaultProbabilityTermStructure> tmp =
                boost::make_shared<PiecewiseDefaultCurve<SurvivalProbability, LogLinear>>(asof, helper,
                                                                                          config->dayCounter());

            // like for yield curves we need to copy the piecewise curve because
            // on eval date changes the relative date helpers with trigger a
            // bootstrap.
            vector<Date> dates(helper.size() + 1, asof);
            vector<Real> survivalProbs(helper.size() + 1, 1.0);
            for (Size i = 0; i < helper.size(); ++i) {
                dates[i + 1] = helper[i]->latestDate();
                survivalProbs[i + 1] = tmp->survivalProbability(dates[i + 1]);
            }
            LOG("DefaultCurve: copy piecewise curve to interpolated survival probability curve");
            curve_ = boost::make_shared<InterpolatedSurvivalProbabilityCurve<LogLinear>>(dates, survivalProbs,
                                                                                         config->dayCounter());
            if (config->extrapolation()) {
                curve_->enableExtrapolation();
                DLOG("DefaultCurve: Enabled Extrapolation");
            }
            return;
        }

        if (config->type() == DefaultCurveConfig::Type::HazardRate) {
            Calendar cal = cdsConv->calendar();
            std::vector<Date> dates;
            std::vector<Real> quoteValues;

            // if first term is not zero, add asof point
            if (quotes.begin()->first != 0 * Days) {
                LOG("DefaultCurve: add asof (" << asof << "), hazard rate " << quotes.begin()->second
                                               << ", because not given");
                dates.push_back(asof);
                quoteValues.push_back(quotes.begin()->second);
            }

            for (auto quote : quotes) {
                dates.push_back(cal.advance(asof, quote.first, Following, false));
                quoteValues.push_back(quote.second);
            }

            LOG("DefaultCurve: set up interpolated hazard rate curve");
            curve_ = boost::make_shared<InterpolatedHazardRateCurve<BackwardFlat>>(
                dates, quoteValues, config->dayCounter(), BackwardFlat());

            if (config->extrapolation()) {
                curve_->enableExtrapolation();
                DLOG("DefaultCurve: Enabled Extrapolation");
            }
            if (recoveryRate_ == Null<Real>()) {
                LOG("DefaultCurve: setting recovery rate to 0.0 for "
                    "hazard rate curve, because none is given.");
                recoveryRate_ = 0.0;
            }
            return;
        }

        if (config->type() == DefaultCurveConfig::Type::Benchmark) {
            std::vector<Period> pillars = config->pillars();
            Calendar cal = config->calendar();
            Size spotLag = config->spotLag();
            // setup
            std::vector<Date> dates;
            std::vector<Real> impliedSurvProb;
            Date spot = cal.advance(asof, spotLag * Days);
            for (Size i = 0; i < pillars.size(); ++i) {
                dates.push_back(cal.advance(spot, pillars[i]));
                Real tmp = sourceCurve->handle()->discount(dates[i]) / benchmarkCurve->handle()->discount(dates[i]);
                impliedSurvProb.push_back(dates[i] == asof ? 1.0 : tmp);
            }
            QL_REQUIRE(dates.size() > 0, "DefaultCurve (Benchmark): no dates given");
            if (dates[0] != asof) {
                dates.insert(dates.begin(), asof);
                impliedSurvProb.insert(impliedSurvProb.begin(), 1.0);
            }
            LOG("DefaultCurve: set up interpolated surv prob curve as yield over benchmark");
            curve_ = boost::make_shared<InterpolatedSurvivalProbabilityCurve<LogLinear>>(dates, impliedSurvProb,
                                                                                         config->dayCounter());

            if (config->extrapolation()) {
                curve_->enableExtrapolation();
                DLOG("DefaultCurve: Enabled Extrapolation");
            }
            QL_REQUIRE(recoveryRate_ == Null<Real>(), "DefaultCurve: recovery rate must not be given "
                                                      "for benchmark implied curve type, it is assumed to "
                                                      "be 0.0");
            recoveryRate_ = 0.0;
            return;
        }

        QL_FAIL("unknown default curve type");
    } catch (std::exception& e) {
        QL_FAIL("default curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("default curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
