/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/aggregation/creditmigrationcalculator.hpp>
#include <orea/aggregation/creditmigrationhelper.hpp>

namespace ore {
namespace analytics {

CreditMigrationCalculator::CreditMigrationCalculator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<CreditSimulationParameters>& creditSimulationParameters,
    const QuantLib::ext::shared_ptr<NPVCube>& cube, const QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation,
    const QuantLib::ext::shared_ptr<NPVCube>& nettedCube,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& aggregationScenarioData,
    const std::vector<Real>& creditMigrationDistributionGrid, const std::vector<Size>& creditMigrationTimeSteps,
    const Matrix& creditStateCorrelationMatrix, const std::string baseCurrency)
    : portfolio_(portfolio), creditSimulationParameters_(creditSimulationParameters), cube_(cube),
      cubeInterpretation_(cubeInterpretation), nettedCube_(nettedCube),
      aggregationScenarioData_(aggregationScenarioData),
      creditMigrationDistributionGrid_(creditMigrationDistributionGrid),
      creditMigrationTimeSteps_(creditMigrationTimeSteps), creditStateCorrelationMatrix_(creditStateCorrelationMatrix),
      baseCurrency_(baseCurrency) {}

void CreditMigrationCalculator::build() {

    LOG("Credit migration computation started.");

    // checks

    QL_REQUIRE(portfolio_ != nullptr, "CreditMigrationCalculator::build(): portfolio is null");
    QL_REQUIRE(creditSimulationParameters_ != nullptr,
               "CreditMigrationCalculator::build(): creditSimulationParameters is null");
    QL_REQUIRE(cube_ != nullptr, "CreditMigrationCalculator::build(): cube is null");
    QL_REQUIRE(cubeInterpretation_ != nullptr, "CreditMigrationCalculator::build(): cube interpretation is null");
    QL_REQUIRE(nettedCube_ != nullptr, "CreditMigrationCalculator::build(): netted cube is null");
    QL_REQUIRE(aggregationScenarioData_ != nullptr,
               "CreditMigrationCalculator::build(): aggregation scenario data is null");
    QL_REQUIRE(!baseCurrency_.empty(), "CreditMigrationCalculator::build(): base currency is empty");
    QL_REQUIRE(creditStateCorrelationMatrix_.rows() == creditStateCorrelationMatrix_.columns(),
               "CreditMigrationCalculator::build(): credit state correlation matrix is not square ("
                   << creditStateCorrelationMatrix_.rows() << " x " << creditStateCorrelationMatrix_.columns() << ")");
    QL_REQUIRE(
        creditMigrationDistributionGrid_.size() == 3,
        "CreditMigrationCalculator::build(): credit migration distribution grid spec must consist of 3 numbers (got "
            << creditMigrationDistributionGrid_.size() << ")");

    // create helper

    CreditMigrationHelper hlp(creditSimulationParameters_, cube_, nettedCube_, aggregationScenarioData_,
                              cubeInterpretation_->mporFlowsIndex(), cubeInterpretation_->creditStateNPVsIndex(),
                              creditMigrationDistributionGrid_[0], creditMigrationDistributionGrid_[1],
                              static_cast<Size>(creditMigrationDistributionGrid_[2]), creditStateCorrelationMatrix_,
                              baseCurrency_);

    hlp.build(portfolio_->trades());

    // compute output

    upperBucketBounds_ = hlp.upperBucketBound();
    if (!upperBucketBounds_.empty())
        upperBucketBounds_.pop_back();
    cdf_.clear();
    pdf_.clear();

    for (Size i = 0; i < creditMigrationTimeSteps_.size(); ++i) {
        DLOG("Generating pnl distribution for timestep " << creditMigrationTimeSteps_[i]);
        cdf_.push_back({});
        pdf_.push_back({});
        Array dist = hlp.pnlDistribution(creditMigrationTimeSteps_[i]);
        Real mean = 0.0, stdev = 0.0;
        Real sum = 0.0;
        for (Size j = 1; j < hlp.upperBucketBound().size() - 1; ++j) {
            Real pnl = 0.5 * (hlp.upperBucketBound()[j] + hlp.upperBucketBound()[j - 1]);
            mean += pnl * dist[j];
            stdev += pnl * pnl * dist[j];
            sum += dist[j];
        }
        stdev = sqrt(stdev - mean * mean);
        TLOG("Total PnL at time step " << creditMigrationTimeSteps_[i] << ": Mean " << std::fixed << mean << " StdDev "
                                       << stdev << " Prob " << std::scientific << sum << " Left " << dist[0]
                                       << " Right " << dist[hlp.upperBucketBound().size() - 1]);
        Real p = 0.0;
        for (Size j = 0; j < hlp.upperBucketBound().size() - 1; ++j) {
            p += dist[j];
            cdf_.back().push_back(p);
            pdf_.back().push_back(dist[j]);
        }
    }

    LOG("Credit migration computation finished.");
}

} // namespace analytics
} // namespace ore
