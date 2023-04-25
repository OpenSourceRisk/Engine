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

namespace ore {
namespace analytics {

CreditMigrationCalculator::CreditMigrationCalculator(
    const boost::shared_ptr<Portfolio>& portfolio,
    const boost::shared_ptr<CreditSimulationParameters>& creditSimulationParameters,
    const boost::shared_ptr<NPVCube>& cube, const boost::shared_ptr<CubeInterpretation> cubeInterpretation,
    const boost::shared_ptr<NPVCube>& nettedCube,
    const boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData,
    const std::vector<Real>& creditMigrationDistributionGrid, const std::vector<Size>& creditMigrationTimeSteps,
    const Matrix& creditStateCorrelationMatrix, const std::string baseCurrency)
    : portfolio_(portfolio), creditSimulationParameters_(creditSimulationParameters),
      cubeInterpretation_(cubeInterpretation), nettedCube_(nettedCube),
      aggregationScenarioData_(aggregationScenarioData),
      creditMigrationDistributionGrid_(creditMigrationDistributionGrid),
      creditMigrationTimeSteps_(creditMigrationTimeSteps), creditStateCorrelationMatrix_(creditStateCorrelationMatrix),
      baseCurrency_(baseCurrency) {}

void CreditMigrationCalculator::build() {

    // checks
    
    QL_REQUIRE(portfolio_ != nullptr, "CreditMigrationCalculator::build(): portfolio is null");
    QL_REQUIRE(creditSimulationParameters_ != nullptr, "CreditMigrationCalculator::build(): creditSimulationParameters is null");
    QL_REQUIRE(cube_ != nullptr, "CreditMigrationCalculator::build(): cube is null");
    QL_REQUIRE(cubeInterpretation_ != nullptr, "CreditMigrationCalculator::build(): cube interpretation is null");
    QL_REQUIRE(nettedCube_ != nullptr, "CreditMigrationCalculator::build(): netted cube is null");
    QL_REQUIRE(aggregationScenarioData_ != nullptr, "CreditMigrationCalculator::build(): aggregation scenario data is null");
    QL_REQUIRE(!baseCurrency_.empty(), "CreditMigrationCalculator::build(): base currency is empty");

    QL_REQUIRE(creditStateCorrelationMatrix_.rows() == creditStateCorrelationMatrix_.columns(),
               "CreditMigrationCalculator::build(): credit state correlation matrix is not square ("
                   << creditStateCorrelationMatrix_.rows() << " x " << creditStateCorrelationMatrix_.columns() << ")");

    QL_REQUIRE(creditStateCorrelationMatrix_.rows() == cubeInterpretation_->storeCreditStateNPVs(),
               "CreditMigrationCalculator::build(): credit state correlation matrix dimension ("
                   << creditStateCorrelationMatrix_.rows() << " x " << creditStateCorrelationMatrix_.columns()
                   << ") is inconsistent with the number of credit states stored in the npv cube ("
                   << cubeInterpretation_->storeCreditStateNPVs());

    // create helper

    
    

    // set output 

    std::vector<Real> upperBucketBounds_;
    std::vector<std::vector<Real>> cdf_;
    std::vector<std::vector<Real>> pdf_;


}

} // namespace analytics
} // namespace ore
