/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ored/model/utilities.hpp>

#include <ored/model/hestonmodelbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/processes/hestonslvprocess.hpp>

#include <ql/math/optimization/differentialevolution.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/models/equity/hestonmodelhelper.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticpdfhestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticptdhestonengine.hpp>
#include <ql/pricingengines/vanilla/coshestonengine.hpp>
#include <ql/pricingengines/vanilla/exponentialfittinghestonengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/fdhestonvanillaengine.hpp>
#include <ql/quotes/simplequote.hpp>

#include <ored/model/hestonmodelcalibration.hpp>

namespace ore {
namespace data {

HestonModelBuilder::HestonModelBuilder(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const std::vector<Period>& calibrationExpiries, const std::vector<Real>& calibrationMoneyness,
    const std::vector<Period>& calibrationVarianceTerms,
    // theta, kappa, sigma, rho, v0 (same order as in the Heston model, not the Heston process)
    const std::vector<Real>& initialValues, const std::vector<bool>& fixedValues, Real relaxedFellerConstraint,
    Size calibrationRestarts, Real tolerance, const std::string& referenceCalibrationGrid, const bool dontCalibrate,
    const Handle<YieldTermStructure>& baseCurve)
    : AssetModelBuilderBase(curves, processes, simulationDates, addDates, timeStepsPerYear, baseCurve),
      calibrationExpiries_(calibrationExpiries), calibrationMoneyness_(calibrationMoneyness),
      calibrationVarianceTerms_(calibrationVarianceTerms), initialValues_(initialValues), fixedValues_(fixedValues),
      relaxedFellerConstraint_(relaxedFellerConstraint), calibrationRestarts_(calibrationRestarts),
      tolerance_(tolerance), referenceCalibrationGrid_(referenceCalibrationGrid), dontCalibrate_(dontCalibrate) {}

std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> HestonModelBuilder::getCalibratedProcesses() const {

    DLOG("HestonModelBuilder::getCalibratedProcesses() called");

    calculate();

    std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> processes;

    DLOG("loop over " << processes_.size() << " processes");
    DLOG("dontCalibrate: " << dontCalibrate_);
    DLOG("initial values: " << to_string(initialValues_));

    QL_REQUIRE(initialValues_.size() == 5, "5 initial values expected, found " << initialValues_.size());

    for (Size i = 0; i < processes_.size(); ++i) {

        HestonModelCalibration hmc(processes_[i], calibrationExpiries_, calibrationMoneyness_, calibrationVarianceTerms_, initialValues_,
                                   fixedValues_, relaxedFellerConstraint_, calibrationRestarts_, tolerance_, dontCalibrate_);

        processes.push_back(hmc.model()->process());
    }

    return processes;
}

std::vector<std::vector<Real>> HestonModelBuilder::getCurveTimes() const {
    std::vector<Real> timesExt(discretisationTimeGrid_.begin() + 1, discretisationTimeGrid_.end());
    for (auto const& d : addDates_) {
        if (d > curves_.front()->referenceDate()) {
            timesExt.push_back(curves_.front()->timeFromReference(d));
        }
    }
    std::sort(timesExt.begin(), timesExt.end());
    auto it = std::unique(timesExt.begin(), timesExt.end(),
                          [](const Real x, const Real y) { return QuantLib::close_enough(x, y); });
    timesExt.resize(std::distance(timesExt.begin(), it));
    return std::vector<std::vector<Real>>(allCurves_.size(), timesExt);
}

std::vector<std::vector<std::pair<Real, Real>>> HestonModelBuilder::getVolTimesStrikes() const {
    std::vector<std::vector<std::pair<Real, Real>>> volTimesStrikes;
    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        if (d > curves_.front()->referenceDate())
            times.push_back(processes_.front()->riskFreeRate()->timeFromReference(d));
    }
    for (auto const& p : processes_) {
        volTimesStrikes.push_back(std::vector<std::pair<Real, Real>>());
        for (auto const t : times) {
            Real atmLevel = atmForward(p->x0(), p->riskFreeRate(), p->dividendYield(), t);
            Real atmMarketVol = std::max(1e-4, p->blackVolatility()->blackVol(t, atmLevel));
            for (auto const m : calibrationMoneyness_) {
                Real strike = atmLevel * std::exp(m * atmMarketVol * std::sqrt(t));
                volTimesStrikes.back().push_back(std::make_pair(t, strike));
            }
        }
    }
    return volTimesStrikes;
}

AssetModelWrapper::ProcessType HestonModelBuilder::processType() const {
    return AssetModelWrapper::ProcessType::Heston;
}

} // namespace data
} // namespace ore
