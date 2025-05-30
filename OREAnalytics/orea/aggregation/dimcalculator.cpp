/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/aggregation/dimcalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>
#include <ored/portfolio/trade.hpp>

#include <qle/math/nadarayawatson.hpp>
#include <qle/math/stabilisedglls.hpp>
#include <qle/math/distributioncount.hpp>

#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/kernelfunctions.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

DynamicInitialMarginCalculator::DynamicInitialMarginCalculator(
    const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData, Real quantile, Size horizonCalendarDays,
    const std::map<std::string, Real>& currentIM)
    : inputs_(inputs), portfolio_(portfolio), cube_(cube), cubeInterpretation_(cubeInterpretation), scenarioData_(scenarioData),
      quantile_(quantile), horizonCalendarDays_(horizonCalendarDays), currentIM_(currentIM) {

    QL_REQUIRE(portfolio_, "portfolio is null");
    QL_REQUIRE(cube_, "cube is null");
    QL_REQUIRE(cubeInterpretation_, "cube interpretation is null");
    QL_REQUIRE(scenarioData_, "aggregation scenario data is null");

    cubeIsRegular_ = !cubeInterpretation_->withCloseOutLag();
    datesLoopSize_ = cubeIsRegular_ ? cube_->dates().size() - 1 : cube_->dates().size();

    Size dates = cube_->dates().size();
    Size samples = cube_->samples();

    if (!cubeInterpretation_->storeFlows()) {
        WLOG("cube holds no mpor flows, will assume no flows in the dim calculation");
    }

    // initialise aggregate NPV and Flow by date and scenario
    set<string> nettingSets;
    for (auto const& [tradeId, trade] : portfolio_->trades()) {
        string nettingSetId = trade->envelope().nettingSetId();
        std::size_t i = cube_->getTradeIndex(tradeId);
        if (nettingSets.find(nettingSetId) == nettingSets.end()) {
            nettingSets.insert(nettingSetId);
            nettingSetNPV_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetCloseOutNPV_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetFLOW_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetDeltaNPV_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetDIM_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetExpectedDIM_[nettingSetId] = vector<Real>(dates, 0.0);
        }
        for (Size j = 0; j < datesLoopSize_; ++j) {
            for (Size k = 0; k < samples; ++k) {
                Real defaultNpv = cubeInterpretation_->getDefaultNpv(cube_, i, j, k);
                Real closeOutNpv = cubeInterpretation_->getCloseOutNpv(cube_, i, j, k, scenarioData_);
                Real mporFlow =
                    cubeInterpretation_->storeFlows() ? cubeInterpretation_->getMporFlows(cube_, i, j, k) : 0.0;
                nettingSetNPV_[nettingSetId][j][k] += defaultNpv;
                nettingSetCloseOutNPV_[nettingSetId][j][k] += closeOutNpv;
                nettingSetFLOW_[nettingSetId][j][k] += mporFlow;
            }
        }
    }

    nettingSetIds_ = std::move(nettingSets);

    if (cube_->usesDoublePrecision()) {
        dimCube_ = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(cube_->asof(), nettingSetIds_, cube_->dates(),
                                                                       cube_->samples());
    } else {
        dimCube_ = QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(cube_->asof(), nettingSetIds_, cube_->dates(),
                                                                      cube_->samples());
    }
}

const vector<vector<Real>>& DynamicInitialMarginCalculator::dynamicIM(const std::string& nettingSet) const {
    if (nettingSetDIM_.find(nettingSet) != nettingSetDIM_.end())
        return nettingSetDIM_.at(nettingSet);
    else
        QL_FAIL("netting set " << nettingSet << " not found in DIM results");
}

const vector<Real>& DynamicInitialMarginCalculator::expectedIM(const std::string& nettingSet) const {
    if (nettingSetExpectedDIM_.find(nettingSet) != nettingSetExpectedDIM_.end())
        return nettingSetExpectedDIM_.at(nettingSet);
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected DIM results");
}

const vector<vector<Real>>& DynamicInitialMarginCalculator::cashFlow(const std::string& nettingSet) const {
    if (nettingSetFLOW_.find(nettingSet) != nettingSetFLOW_.end())
        return nettingSetFLOW_.at(nettingSet);
    else
        QL_FAIL("netting set " << nettingSet << " not found in DIM results");
}

void DynamicInitialMarginCalculator::exportDimEvolution(ore::data::Report& dimEvolutionReport) const {

    Size samples = dimCube_->samples();
    Size stopDatesLoop = datesLoopSize_;
    Date asof = cube_->asof();

    dimEvolutionReport.addColumn("TimeStep", Size())
        .addColumn("Date", Date())
        .addColumn("DaysInPeriod", Size())
        .addColumn("AverageDIM", Real(), 6)
        .addColumn("AverageFLOW", Real(), 6)
        .addColumn("NettingSet", string())
        .addColumn("Time", Real(), 6);

    for (const auto& [nettingSet, _] : dimCube_->idsAndIndexes()) {

        LOG("Export DIM evolution for netting set " << nettingSet);
        for (Size i = 0; i < stopDatesLoop; ++i) {
            Real expectedFlow = 0.0;
            for (Size j = 0; j < samples; ++j) {
                expectedFlow += nettingSetFLOW_.find(nettingSet)->second[i][j] / samples;
            }

            Date defaultDate = dimCube_->dates()[i];
	    Time t = ActualActual(ActualActual::ISDA).yearFraction(asof, defaultDate);
            Size days = cubeInterpretation_->getMporCalendarDays(dimCube_, i);
            dimEvolutionReport.next()
                .add(i)
                .add(defaultDate)
                .add(days)
                .add(nettingSetExpectedDIM_.find(nettingSet)->second[i])
                .add(expectedFlow)
                .add(nettingSet)
	        .add(t);
        }
    }
    dimEvolutionReport.end();
    LOG("Exporting expected DIM through time done");
}

void DynamicInitialMarginCalculator::exportDimDistribution(ore::data::Report& dimDistributionReport,
                                                           const Size gridSize, const Real coveredStdDevs) const {

    dimDistributionReport.addColumn("NettingSet", string())
        .addColumn("TimeStep", Size())
        .addColumn("Date", Date())
        .addColumn("Bound", Real(), 6)
        .addColumn("Count", Size());

    std::vector<Real> bounds;
    std::vector<Size> counts;

    for (const auto& [nettingSet, _] : dimCube_->idsAndIndexes()) {

        for (Size i = 0; i < datesLoopSize_; ++i) {
            distributionCount(nettingSetDIM_.at(nettingSet).at(i).begin(), nettingSetDIM_.at(nettingSet).at(i).end(),
                              gridSize, bounds, counts, coveredStdDevs);
            for (Size j = 0; j < gridSize; ++j)
                dimDistributionReport.next()
                    .add(nettingSet)
                    .add(i)
                    .add(dimCube_->dates()[i])
                    .add(bounds[j])
                    .add(counts[j]);
        }
    }

    dimDistributionReport.end();
}

} // namespace analytics
} // namespace ore
