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
#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace analytics {

std::string CorrelationReport::mapRiskFactorToAssetType(RiskFactorKey::KeyType keyF) { 
    std::vector<std::string> ir = {"DiscountCurve", "IndexCurve", "OptionletVolatility"};
    std::vector<std::string> fx = {"FXSpot", "FXVolatility"};
    std::vector<std::string> inf = {"ZeroInflationCurve"};
    std::vector<std::string> cr = {"SurvivalProbability"};
    std::vector<std::string> eq = {"EquitySpot", "EquityVolatility"};
    std::vector<std::string> com = {};
    std::vector<std::string> crstate = {};

    std::unordered_map<std::string, std::vector<std::string>> mapping;
    mapping["IR"] = ir;
    mapping["FX"] = fx;
    mapping["INF"] = inf;
    mapping["CR"] = cr;
    mapping["EQ"] = eq;
    mapping["COM"] = com;
    mapping["CrState"] = crstate;
    
    for (const auto& pair : mapping) {
        const auto& keyMap = pair.first;
        const auto& vec = pair.second;
        if (std::find(vec.begin(), vec.end(), ore::data::to_string(keyF)) != vec.end()) {
            return keyMap;
        }
    }
    return "";
}

void CorrelationReport::calculate(const ext::shared_ptr<Report>& report) {
    
    hisScenGen_ = QuantLib::ext::make_shared<HistoricalScenarioGeneratorWithFilteredDates>(timePeriods(), hisScenGen_);
    
    ext::shared_ptr<Scenario> sc = hisScenGen_->next(hisScenGen_->baseScenario()->asof());
    std::vector<RiskFactorKey> deltaKeys = sc->keys();

    ext::shared_ptr<NPVCube>cube;
    ext::shared_ptr<CovarianceCalculator> covCalculator;
    covCalculator = ext::make_shared<CovarianceCalculator>(covariancePeriod());

    sensiPnlCalculator_ = ext::make_shared<HistoricalSensiPnlCalculator>(hisScenGen_, nullptr);
    sensiPnlCalculator_->populateSensiShifts(cube, deltaKeys, shiftCalc_);
    sensiPnlCalculator_->calculateSensiPnl({}, deltaKeys, cube, pnlCalculators_, covCalculator, {},
                                           false, false, false);

    DLOG("Computation of the Correlation Matrix Method = " << correlationMethod_);
    CorrelationMatrixBuilder corrMatrix;
    if (correlationMethod_ == "Pearson") {
        covarianceMatrix_ = covCalculator->covariance();
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
            //The cube has plenty of 0 values, meaning 0 correlations
            //We filter them
            if (correlationMatrix_[col][row] != 0) {
                correlationPairs_[std::make_pair(a, b)] = correlationMatrix_[col][row];
            }
        }
    }
    writeReports(report);

    // Instantaneous Correlation si a pair of smth "IR:USD, IR:GBP, EQ:SP5 etc.
    /*std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>> mapInstantaneousCor;
    for (auto const& cor : correlationPairs_) {
        RiskFactorKey pair1 = cor.first.first;
        RiskFactorKey pair2 = cor.first.second;
        string asset1 = mapRiskFactorToAssetType(pair1.keytype);
        string asset2 = mapRiskFactorToAssetType(pair2.keytype);
        CorrelationFactor corrFactor1 {parseCamAssetType(asset1), pair1.name, pair1.index};
        CorrelationFactor corrFactor2 {parseCamAssetType(asset2), pair2.name, pair2.index};
        std::pair<CorrelationFactor, CorrelationFactor> correlationKey = std::make_pair(corrFactor1, corrFactor2);
        mapInstantaneousCor[correlationKey] =
            QuantLib::Handle<QuantLib::Quote>(QuantLib::ext::make_shared<SimpleQuote>(cor.second));
    }
    instantaneousCorrelation_ = ext::make_shared<InstantaneousCorrelations>(mapInstantaneousCor);*/
}

void CorrelationReport::writeReports(const ext::shared_ptr<Report>& report) {

    report->addColumn("RiskFactor1", string())
         .addColumn("RiskFactor2", string())
         .addColumn("Correlation", Real(), 6);

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
