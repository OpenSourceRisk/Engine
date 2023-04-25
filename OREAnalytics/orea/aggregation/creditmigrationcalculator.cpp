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
    const std::vector<Real>& creditMigrationDistributionGrid, const std::string baseCurrency)
    : portfolio_(portfolio), creditSimulationParameters_(creditSimulationParameters),
      cubeInterpretation_(cubeInterpretation), nettedCube_(nettedCube),
      aggregationScenarioData_(aggregationScenarioData),
      creditMigrationDistributionGrid_(creditMigrationDistributionGrid), baseCurrency_(baseCurrency) {}

void CreditMigrationCalculator::build() {}

} // namespace analytics
} // namespace ore
