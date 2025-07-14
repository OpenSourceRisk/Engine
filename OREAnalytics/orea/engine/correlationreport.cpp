/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <orea/engine/correlationreport.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/math/deltagammavar.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <qle/math/kendallrankcorrelation.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/cube/inmemorycube.hpp>

namespace ore {
namespace analytics {

std::vector<Real> extractLowerTriangle(const QuantLib::Matrix& corrMatrix) {
    Size n = corrMatrix.rows();
    std::vector<Real> result;
    for (Size col = 0; col < n; col++) {
        for (Size row = col+1; row<n;row++){
            result.push_back(corrMatrix[row][col]);
        }
    }
    return result;
}

void CorrelationReport::calculate(const ext::shared_ptr<Report>& report) {
    
    hisScenGen_ = QuantLib::ext::make_shared<HistoricalScenarioGeneratorWithFilteredDates>(timePeriods(), hisScenGen_);
    
    auto vSc = hisScenGen_->next(hisScenGen_->baseScenario()->asof());
    auto deltaKeys = vSc->keys();

    ext::shared_ptr<NPVCube>cube;
    ext::shared_ptr<CovarianceCalculator> covCalculator;
    covCalculator = ext::make_shared<CovarianceCalculator>(covariancePeriod());

    sensiPnlCalculator_ = ext::make_shared<HistoricalSensiPnlCalculator>(hisScenGen_, nullptr);
    sensiPnlCalculator_->populateSensiShifts(cube, deltaKeys, shiftCalc_);
    sensiPnlCalculator_->calculateSensiPnl({}, deltaKeys, cube, pnlCalculators_, covCalculator, {},
                                           false, false, false);

    covarianceMatrix_ = covCalculator->covariance();

    DLOG("Computation of the Correlation Matrix Method = " << correlationMethod_);
    CorrelationMatrixBuilder corrMatrix;
    if (correlationMethod_ == "Pearson") {
        // Correlation Matrix computed by default from the covariance
        correlationMatrix_ = covCalculator->correlation();
    } else if (correlationMethod_ == "KendallRank") {
        std::set<std::string> ids = cube->ids();
        std::vector<QuantLib::Date> d = cube->dates();
        Size i = 0;
        int nbScenario = sensiPnlCalculator_->getScenarioNumber();
        QuantLib::Matrix mSensi(nbScenario, deltaKeys.size());
        for (auto it = ids.begin(); it != ids.end(); it++, i++) {
            for (int j = 0; j < nbScenario; j++) {
                QuantLib::Real cubeValue = cube->get(*it, d[0], j);
                mSensi[j][i] = cubeValue;
            }
        }
        correlationMatrix_ = corrMatrix.kendallCorrelation(mSensi);
    } else {
        QL_FAIL("Accepted Correlations Methods: Pearson, KendallRank");
    }
    // Creation of RiskFactor Pairs Matching the Correlation Matrix Lower Triangular Part
    for (Size col = 0; col < deltaKeys.size(); col++) {
        for (Size row = col + 1; row < deltaKeys.size(); row++) {
            RiskFactorKey a = deltaKeys[col];
            RiskFactorKey b = deltaKeys[row];
            correlationPairs_[std::make_pair(a, b)] = correlationMatrix_[col][row];
        }
    }
    writeReports(report);

    //InstantaneousCorrelation is std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>
    //std::pair<CorrelationFactor, CorrelationFactor> CorrelationKey;
    /*struct CorrelationFactor {
        QuantExt::CrossAssetModel::AssetType type;
        std::string name;
        QuantLib::Size index;
    };*/
}

void CorrelationReport::writeReports(const ext::shared_ptr<Report>& report) {

    report->addColumn("RiskFactor1", string())
         .addColumn("RiskFactor2", string())
         .addColumn("Correlation", Real(), 6);

    vector<Real> corrFormatted = extractLowerTriangle(correlationMatrix_);

    for (auto const& entry : correlationPairs_) {
        const auto& key = entry.first;
        const Real value = entry.second;
        report->next()
            .add(ore::data::to_string(key.first))
            .add(ore::data::to_string(key.second))
            .add(value);
    }
    report->end();
}

} // namespace analytics
} // namespace ore
