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

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/defaultprobabilityhelpers.hpp>
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
                QL_FAIL("The discount curve, " << config->discountCurveID() << ", required in the building "
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
                QL_FAIL("The benchmark curve, " << config->benchmarkCurveID() << ", required in the building "
                                                                                 "of the curve, "
                                                << spec.name() << ", was not found.");
            }
            auto it2 = yieldCurves.find(config->sourceCurveID());
            if (it2 != yieldCurves.end()) {
                sourceCurve = it2->second;
            } else {
                QL_FAIL("The source curve, " << config->sourceCurveID() << ", required in the building "
                                                                           "of the curve, "
                                             << spec.name() << ", was not found.");
            }
        }

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole set of quotes or do not have more quotes in the
        // market data

        vector<Real> quotes;
        vector<Period> terms;
        recoveryRate_ = Null<Real>();

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::RECOVERY_RATE) {

                boost::shared_ptr<RecoveryRateQuote> q = boost::dynamic_pointer_cast<RecoveryRateQuote>(md);

                if (q->name() == config->recoveryRateQuote()) {
                    QL_REQUIRE(recoveryRate_ == Null<Real>(), "duplicate recovery rate quote " << q->name()
                                                                                               << " found.");
                    recoveryRate_ = q->quote()->value();
                }
            }

            if (config->type() == DefaultCurveConfig::Type::SpreadCDS && md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::CDS &&
                md->quoteType() == MarketDatum::QuoteType::CREDIT_SPREAD) {

                boost::shared_ptr<CdsSpreadQuote> q = boost::dynamic_pointer_cast<CdsSpreadQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->quotes().begin(), config->quotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->quotes().end()) {

                    vector<Period>::const_iterator it2 = std::find(terms.begin(), terms.end(), q->term());
                    QL_REQUIRE(it2 == terms.end(), "duplicate term in quotes found ("
                                                       << q->term() << ") while loading default curve "
                                                       << config->curveID());
                    terms.push_back(q->term());
                    quotes.push_back(q->quote()->value());
                }
            }

            if (config->type() == DefaultCurveConfig::Type::HazardRate && md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::HAZARD_RATE) {
                boost::shared_ptr<HazardRateQuote> q = boost::dynamic_pointer_cast<HazardRateQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->quotes().begin(), config->quotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->quotes().end()) {

                    vector<Period>::const_iterator it2 = std::find(terms.begin(), terms.end(), q->term());
                    QL_REQUIRE(it2 == terms.end(), "duplicate term in quotes found ("
                                                       << q->term() << ") while loading default curve "
                                                       << config->curveID());
                    terms.push_back(q->term());
                    quotes.push_back(q->quote()->value());
                }
            }
        }

        LOG("DefaultCurve: read " << quotes.size() << " default quotes");

        QL_REQUIRE(quotes.size() == config->quotes().size(), "read " << quotes.size() << ", but "
                                                                     << config->quotes().size() << " required.");

        if (config->type() == DefaultCurveConfig::Type::SpreadCDS) {
            QL_REQUIRE(recoveryRate_ != Null<Real>(), "DefaultCurve: no recovery rate given for type "
                                                      "SpreadCDS");
            std::vector<boost::shared_ptr<DefaultProbabilityHelper>> helper;
            for (Size i = 0; i < quotes.size(); ++i) {
                helper.push_back(boost::make_shared<SpreadCdsHelper>(
                    quotes[i], terms[i], cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
                    cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate_, discountCurve,
                    cdsConv->settlesAccrual(), cdsConv->paysAtDefaultTime()));
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
            // if first term is not zero, add asof point
            if (terms[0] != 0 * Days) {
                LOG("DefaultCurve: add asof (" << asof << "), hazard rate " << quotes[0] << ", because not given");
                dates.push_back(asof);
                quotes.insert(quotes.begin(), quotes[0]);
            }
            for (Size i = 0; i < terms.size(); ++i) {
                dates.push_back(cal.advance(asof, terms[i], Following, false));
            }
            LOG("DefaultCurve: set up interpolated hazard rate curve");
            curve_ = boost::make_shared<InterpolatedHazardRateCurve<BackwardFlat>>(dates, quotes, config->dayCounter(),
                                                                                   BackwardFlat());
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
