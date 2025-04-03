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

#include <orea/aggregation/dimdirectcalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/version.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace analytics {

DirectDynamicInitialMarginCalculator::DirectDynamicInitialMarginCalculator(
    const QuantLib::ext::shared_ptr<InputParameters>& inputs, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
    const QuantLib::ext::shared_ptr<NPVCube>& imCube, const std::map<std::string, Real>& currentIM)
    : DynamicInitialMarginCalculator(inputs, portfolio, cube, cubeInterpretation, scenarioData, Null<Real>(), 0,
                                     currentIM),
      imCube_(imCube) {}

void DirectDynamicInitialMarginCalculator::build() {
    DLOG("DirectDynamicInitialMarginCalculator:build() called");
    for (const auto& n : nettingSetIds_) {
        DLOG("Process netting set " << n);
        auto ns = imCube_->idsAndIndexes().find(n);
        QL_REQUIRE(ns != imCube_->idsAndIndexes().end(), "DirectDynamicInitialMarginCalculator::build(): netting set '"
                                                             << n << "' not found in im-cube. Internal error.");
        unscaledCurrentDIM_[n] = imCube_->getT0(ns->second, 0);
        for (Size j = 0; j < cube_->dates().size(); ++j) {
            nettingSetExpectedDIM_[n][j] = 0.0;
            for (Size k = 0; k < cube_->samples(); ++k) {
                nettingSetDIM_[n][j][k] = imCube_->get(ns->second, j, k, 0);
                nettingSetExpectedDIM_[n][j] += imCube_->get(ns->second, j, k, 0);
                dimCube_->set(imCube_->get(ns->second, j, k, 0), ns->second, j, k);
            }
            nettingSetExpectedDIM_[n][j] /= static_cast<double>(cube_->samples());
        }
    }
}

void DirectDynamicInitialMarginCalculator::exportDimEvolution(ore::data::Report& dimEvolutionReport) const {

    Date asof = cube_->asof();
    Size samples = dimCube_->samples();

    dimEvolutionReport.addColumn("TimeStep", Size())
        .addColumn("Date", Date())
        .addColumn("DaysInPeriod", Size())
        .addColumn("ZeroOrderDIM", Real(), 6)
        .addColumn("AverageDIM", Real(), 6)
        .addColumn("AverageFLOW", Real(), 6)
        .addColumn("SimpleDIM", Real(), 6)
        .addColumn("NettingSet", string())
        .addColumn("Time", Real(), 6);

    for (const auto& [nettingSet, _] : dimCube_->idsAndIndexes()) {
        DLOG("Export DIM evolution for netting set " << nettingSet);
        for (Size i = 0; i < datesLoopSize_; ++i) {
            Date defaultDate = dimCube_->dates()[i];
            Time t = ActualActual(ActualActual::ISDA).yearFraction(asof, defaultDate);
            Size days = cubeInterpretation_->getMporCalendarDays(dimCube_, i);
            auto it = nettingSetExpectedDIM_.find(nettingSet);
            Real dim = it->second.at(i);
            Real expectedFlow = 0.0;
            for (Size j = 0; j < samples; ++j) {
                expectedFlow += nettingSetFLOW_.find(nettingSet)->second[i][j] / samples;
            }
            dimEvolutionReport.next()
                .add(i)
                .add(defaultDate)
                .add(days)
                .add(dim)
                .add(dim)
                .add(expectedFlow)
                .add(dim)
                .add(nettingSet)
                .add(t);
        }
    }
    dimEvolutionReport.end();
}

} // namespace analytics
} // namespace ore
