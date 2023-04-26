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

/*! \file orea/aggregation/creditmigrationcalculator.hpp
    \brief Exposure calculator
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/creditsimulationparameters.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>

#include <boost/shared_ptr.hpp>

namespace ore {
namespace analytics {

using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! Credit Migration Calculator
class CreditMigrationCalculator {
public:
    CreditMigrationCalculator(const boost::shared_ptr<Portfolio>& portfolio,
                              const boost::shared_ptr<CreditSimulationParameters>& creditSimulationParameters,
                              const boost::shared_ptr<NPVCube>& cube,
                              const boost::shared_ptr<CubeInterpretation> cubeInterpretation,
                              const boost::shared_ptr<NPVCube>& nettedcube,
                              const boost::shared_ptr<AggregationScenarioData>& aggregationScenarioData,
                              const std::vector<Real>& creditMigrationDistributionGrid,
                              const std::vector<Size>& creditMigrationTimeSteps,
                              const Matrix& creditStateCorrelationMatrix, const std::string baseCurrency);

    void build();

    const std::vector<Real> upperBucketBounds() const { return upperBucketBounds_; }
    const std::vector<std::vector<Real>> cdf() const { return cdf_; }
    const std::vector<std::vector<Real>> pdf() const { return pdf_; }

private:
    boost::shared_ptr<Portfolio> portfolio_;
    boost::shared_ptr<CreditSimulationParameters> creditSimulationParameters_;
    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<CubeInterpretation> cubeInterpretation_;
    boost::shared_ptr<NPVCube> nettedCube_;
    boost::shared_ptr<AggregationScenarioData> aggregationScenarioData_;
    std::vector<Real> creditMigrationDistributionGrid_;
    std::vector<Size> creditMigrationTimeSteps_;
    Matrix creditStateCorrelationMatrix_;
    std::string baseCurrency_;

    std::vector<Real> upperBucketBounds_;
    std::vector<std::vector<Real>> cdf_;
    std::vector<std::vector<Real>> pdf_;
};

} // namespace analytics
} // namespace ore
