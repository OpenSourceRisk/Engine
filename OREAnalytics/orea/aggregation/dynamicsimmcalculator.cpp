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

#include <orea/app/inputparameters.hpp>
#include <orea/aggregation/dynamicsimmcalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/version.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;
using namespace boost::accumulators;
using namespace ore::data;

namespace ore {
namespace analytics {

DynamicSimmCalculator::DynamicSimmCalculator(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                             const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                             const QuantLib::ext::shared_ptr<NPVCube>& cube,
                                             const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
                                             const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
                                             const QuantLib::ext::shared_ptr<SimmHelper>& simmHelper,
                                             QuantLib::Real quantile, QuantLib::Size horizonCalendarDays,
                                             const std::map<std::string, Real>& currentIM, const Size& simmCubeDepth)
    : DynamicInitialMarginCalculator(inputs, portfolio, cube, cubeInterpretation, scenarioData, quantile,
                                     horizonCalendarDays, currentIM, simmCubeDepth),
      simmHelper_(simmHelper) {}

const map<string, Real>& DynamicSimmCalculator::unscaledCurrentDIM() const { return currentDIM_; }

void DynamicSimmCalculator::build() {

    map<string, Real> currentDim = unscaledCurrentDIM();

    Size stopDatesLoop = datesLoopSize_;
    Size samples = cube_->samples();

    map<string, std::vector<QuantLib::ext::shared_ptr<Trade>>> nettingSetTrades;
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        string nettingSetId = trade->envelope().nettingSetId();
        nettingSetTrades[nettingSetId].push_back(trade);
    }

    size_t i = 0;
    for (const auto& nid : nettingSetIds_) {
        LOG("Process netting set " << nid);

        Real nettingSetDimScaling = 1.0;

	// compute and store delta/vega/curvature margin components 
	Real tmp = simmHelper_->initialMargin(nid, Null<Size>(), Null<Size>());

        dimCube_->setT0(tmp, 0, 0);
        if (dimCube_->depth() > 3) {
            dimCube_->setT0(simmHelper_->deltaMargin(), 0, 1);
            dimCube_->setT0(simmHelper_->vegaMargin(), 0, 2);
            dimCube_->setT0(simmHelper_->curvatureMargin(), 0, 3);
        }
        if (dimCube_->depth() > 5) {
            dimCube_->setT0(simmHelper_->irDeltaMargin(), 0, 4);
            dimCube_->setT0(simmHelper_->fxDeltaMargin(), 0, 5);
	}
	
        for (Size j = 0; j < stopDatesLoop; ++j)
            nettingSetExpectedDIM_[nid][j] = 0.0;
	
        for (Size j = 0; j < stopDatesLoop; ++j) {

            for (Size k = 0; k < samples; ++k) {
                Real num = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                Real tmp = simmHelper_->initialMargin(nid, j, k);
                tmp *= nettingSetDimScaling / num;

                nettingSetDIM_[nid][j][k] = tmp;
                nettingSetExpectedDIM_[nid][j] += tmp / samples;
                dimCube_->set(tmp, i, j, k);
                if (dimCube_->depth() > 3) {
                    dimCube_->set(simmHelper_->deltaMargin() * nettingSetDimScaling / num, i, j, k, 1);
                    dimCube_->set(simmHelper_->vegaMargin()  * nettingSetDimScaling / num, i, j, k, 2);
                    dimCube_->set(simmHelper_->curvatureMargin()  * nettingSetDimScaling / num, i, j, k, 3);
                }
		if (dimCube_->depth() > 5) {
                    dimCube_->set(simmHelper_->irDeltaMargin() * nettingSetDimScaling / num, i, j, k, 4);
                    dimCube_->set(simmHelper_->fxDeltaMargin() * nettingSetDimScaling / num, i, j, k, 5);
		}
           }
        }

        i++;
    }
    
    // current dim

    currentDIM_.clear();
    for (const auto& nid : nettingSetIds_) {
        DLOG("Process netting set " << nid);
        currentDIM_[nid] = simmHelper_->initialMargin(nid);
        DLOG("T0 DIM - {" << nid << "} = " << currentDIM_[nid]);
    }
}

} // namespace analytics
} // namespace ore
