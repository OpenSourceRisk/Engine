/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/aggregation/dimflatcalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/version.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace analytics {

FlatDynamicInitialMarginCalculator::FlatDynamicInitialMarginCalculator(
    const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData)
    : DynamicInitialMarginCalculator(inputs, portfolio, cube, cubeInterpretation, scenarioData) {
}


const vector<Real>& FlatDynamicInitialMarginCalculator::dimResults(const std::string& nettingSet) const {
    auto it = nettingSetExpectedDIM_.find(nettingSet);
    if (it != nettingSetExpectedDIM_.end())
        return it->second;
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected DIM results");
}

void FlatDynamicInitialMarginCalculator::build() {
    LOG("FlatDynamicInitialMarginCalculator:build() called");

    Size samples = cube_->samples();

    if (!inputs_->collateralBalances()) {
        ALOG("collateral balances not set");
    }
    
    for (const auto& n : nettingSetIds_) {
        LOG("Process netting set " << n);

        Real currentIM = 0;
        if (inputs_->collateralBalances() && inputs_->collateralBalances()->has(n)) {
            currentIM = inputs_->collateralBalances()->get(n)->initialMargin();
            LOG("Found initial margin balance " << currentIM << " for netting set " << n);
        }
        
        for (Size j = 0; j < cube_->dates().size(); ++j) {
            nettingSetExpectedDIM_[n][j] = currentIM;
            for (Size k = 0; k < samples; ++k)
                nettingSetDIM_[n][j][k] = currentIM;                
        }
    }
    LOG("DIM by flat extraplation of initial IM done");
}

void FlatDynamicInitialMarginCalculator::exportDimEvolution(ore::data::Report& dimEvolutionReport) const {

    // Size samples = dimCube_->samples();
    Size stopDatesLoop = datesLoopSize_;
    Date asof = cube_->asof();

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

        LOG("Export DIM evolution for netting set " << nettingSet);
        for (Size i = 0; i < stopDatesLoop; ++i) {
            Date defaultDate = dimCube_->dates()[i];
            Time t = ActualActual(ActualActual::ISDA).yearFraction(asof, defaultDate);
            Size days = cubeInterpretation_->getMporCalendarDays(dimCube_, i);
            auto it = nettingSetExpectedDIM_.find(nettingSet);
            Real dim = it->second.at(i);
            dimEvolutionReport.next()
                .add(i)
                .add(defaultDate)
                .add(days)
                .add(dim)
                .add(dim)
                .add(0.0)
                .add(dim)
                .add(nettingSet)
                .add(t);
        }
    }
    dimEvolutionReport.end();
    LOG("Exporting expected DIM through time done");
}

} // namespace analytics
} // namespace ore
