/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/cdo.hpp>
#include <ored/portfolio/cdo.hpp>
#include <qle/pricingengines/midpointcdoengine.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <qle/termstructures/interpolatedhazardratecurve.hpp>
#include <qle/termstructures/interpolatedsurvivalprobabilitycurve.hpp>
#include <qle/termstructures/multisectiondefaultcurve.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;
using namespace QuantExt;

//! Disable sensitivites for the all but the first default probability curve
//! Shift all other curves parallel with the first curve, reduce the numbers of calculations
std::vector<Handle<DefaultProbabilityTermStructure>> buildPerformanceOptimizedDefaultCurves(const std::vector<Handle<DefaultProbabilityTermStructure>>& curves) {
    std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
    Handle<DefaultProbabilityTermStructure> clientCurve;
    Handle<DefaultProbabilityTermStructure> baseCurve;
    vector<Time> baseCurveTimes;
    
    for (const auto& targetCurve : curves) {
        //For all but the first curve use a spreaded curve, with the first curve as reference and
        //the inital spread between the curve and the first curve, keep the spread constant
        //in case of a spreaded curve we need to use the underlying reference curve to retrieve the
        //correct pillar times, otherwise the interpolation grid is to coarse and we wouldnt match
        //today's market prices
        vector<Time> targetCurveTimes = ore::data::SyntheticCDO::extractTimeGridDefaultCurve(targetCurve);
        // Sort times we need to interpolate an all pillars of the base and target curve
        std::sort(targetCurveTimes.begin(), targetCurveTimes.end());
        // Use the first curve as basis curve and update times
        if (targetCurve == curves.front()) {
            baseCurve = targetCurve;
            clientCurve = baseCurve;
            baseCurveTimes = targetCurveTimes;
        } else {
            std::vector<Time> times;
            std::set_union(baseCurveTimes.begin(), baseCurveTimes.end(), targetCurveTimes.begin(),
                           targetCurveTimes.end(), std::back_inserter(times));
            vector<Handle<Quote>> spreads(times.size());
            std::transform(times.begin(), times.end(), spreads.begin(), [&baseCurve, &targetCurve](Time t) {
                Probability spread =
                    targetCurve->survivalProbability(t, true) / baseCurve->survivalProbability(t, true);
                if (close_enough(spread, 0.0)) {
                    spread = 1e-18;
                }
                return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spread));
            });
            clientCurve = Handle<DefaultProbabilityTermStructure>(
                QuantLib::ext::make_shared<SpreadedSurvivalProbabilityTermStructure>(baseCurve, times, spreads));
            if (baseCurve->allowsExtrapolation())
                clientCurve->enableExtrapolation();
        }
        dpts.push_back(clientCurve);
    }
    return dpts;
}

QuantLib::ext::shared_ptr<PricingEngine> GaussCopulaBucketingCdoEngineBuilder::engineImpl(
    const Currency& ccy, bool isIndexCDS, const vector<string>& creditCurves,
    const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor,
    const QuantLib::Real fixedRecovery) {
    if (isIndexCDS) {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
        std::vector<Real> recovery;
        for (auto& c : creditCurves) {
            Real recoveryRate = market_->recoveryRate(c, configuration(MarketContext::pricing))->value();
            Handle<DefaultProbabilityTermStructure> targetCurve;
            
            auto it = globalParameters_.find("RunType");
            if (calibrateConstituentCurve() &&  it != globalParameters_.end() && it->second != "PortfolioAnalyser") {
                auto orgCurve = market_->defaultCurve(c, configuration(MarketContext::pricing))->curve();
                targetCurve =
                    SyntheticCDO::buildCalibratedConstiuentCurve(orgCurve, calibrationFactor);
            } else {
                targetCurve = market_->defaultCurve(c, configuration(MarketContext::pricing))->curve();
            }
            recovery.push_back(fixedRecovery != Null<Real>() ? fixedRecovery : recoveryRate);
            dpts.push_back(targetCurve);
        }
        if (optimizedSensitivityCalculation()) {
            dpts = buildPerformanceOptimizedDefaultCurves(dpts);
        }
        return QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(dpts, recovery, yts);
    } else {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantExt::IndexCdsTrancheEngine>(yts);
    }
}
} // namespace data
} // namespace ore
