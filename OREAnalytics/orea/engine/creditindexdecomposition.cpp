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

#include <orea/engine/creditindexdecomposition.hpp>
#include <ored/portfolio/cdo.hpp>
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
namespace ore {
namespace analytics {

void decomposeCreditIndex(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                          const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                          std::map<std::string, QuantLib::Real>& sensitivityDecompositionWeights, bool& decompose) {

    auto indexCDS = QuantLib::ext::dynamic_pointer_cast<ore::data::IndexCreditDefaultSwap>(trade);
    auto indexCDSwapOption = QuantLib::ext::dynamic_pointer_cast<ore::data::IndexCreditDefaultSwapOption>(trade);
    auto cdo = QuantLib::ext::dynamic_pointer_cast<ore::data::SyntheticCDO>(trade);

    if (indexCDS == nullptr && indexCDSwapOption == nullptr && cdo == nullptr) {
        decompose = false;
        sensitivityDecompositionWeights.clear();
        return;
    }

    if (cdo) {
        decompose = cdo->useSensitivitySimplification();
        sensitivityDecompositionWeights = cdo->basketConstituents();
        return;
    }

    try {
        ore::data::CreditPortfolioSensitivityDecomposition sensitivityDecomposition;
        map<string, QuantLib::Real> constituents;
        if (indexCDS) {
            sensitivityDecomposition = indexCDS->sensitivityDecomposition();
            constituents = indexCDS->constituents();
        }
        if (indexCDSwapOption) {
            sensitivityDecomposition = indexCDSwapOption->sensitivityDecomposition();
            constituents = indexCDSwapOption->constituents();
        }

        // Decomposition Weights in case of index pricing
        sensitivityDecompositionWeights.clear();

        if (sensitivityDecomposition == ore::data::CreditPortfolioSensitivityDecomposition::NotionalWeighted) {
            QuantLib::Real totalNotional = std::accumulate(
                constituents.begin(), constituents.end(), 0,
                [](QuantLib::Real v, map<string, QuantLib::Real>::value_type& p) { return v + p.second; });
            for (auto& c : constituents) {
                if (sensitivityDecompositionWeights.find(c.first) == sensitivityDecompositionWeights.end())
                    sensitivityDecompositionWeights.emplace(c.first, c.second / totalNotional);
            }
            decompose = true;
        } else if (sensitivityDecomposition == ore::data::CreditPortfolioSensitivityDecomposition::LossWeighted) {
            QuantLib::Real totalWeight = 0;
            for (auto& c : constituents) {
                string creditName = c.first;
                QuantLib::Real notional = c.second;
                auto defaultCurve = market->defaultCurve(creditName, ore::data::Market::defaultConfiguration)->curve();
                QuantLib::Real constituentRecovery =
                    market->recoveryRate(creditName, ore::data::Market::defaultConfiguration)->value();
                QuantLib::Real recovery = constituentRecovery;
                if (indexCDSwapOption) {
                    if (indexCDSwapOption->swap().recoveryRate() != QuantLib::Null<QuantLib::Real>())
                        recovery = indexCDSwapOption->swap().recoveryRate();
                }
                QuantLib::Real constituentPD = defaultCurve->defaultProbability(trade->maturity());
                QuantLib::Real expLoss = notional * constituentPD * (1 - recovery);

                if (sensitivityDecompositionWeights.find(creditName) == sensitivityDecompositionWeights.end())
                    sensitivityDecompositionWeights.emplace(creditName, expLoss);
                totalWeight += expLoss;
            }
            // Normalize
            for (auto& decompWeight : sensitivityDecompositionWeights) {
                decompWeight.second /= totalWeight;
            }
            decompose = true;
        } else if (sensitivityDecomposition == ore::data::CreditPortfolioSensitivityDecomposition::DeltaWeighted) {
            QuantLib::Real totalWeight = 0;
            for (auto& c : constituents) {
                string creditName = c.first;
                QuantLib::Real notional = c.second;
                auto defaultCurve = market->defaultCurve(creditName, ore::data::Market::defaultConfiguration)->curve();
                QuantLib::Real constituentSurvivalProb = defaultCurve->survivalProbability(trade->maturity());
                QuantLib::Time t = defaultCurve->timeFromReference(trade->maturity());
                QuantLib::Real CR01 = t * constituentSurvivalProb * notional;
                if (sensitivityDecompositionWeights.find(creditName) == sensitivityDecompositionWeights.end())
                    sensitivityDecompositionWeights.emplace(creditName, CR01);
                totalWeight += CR01;
            }
            // Normalize
            for (auto& decompWeight : sensitivityDecompositionWeights) {
                decompWeight.second /= totalWeight;
            }
            decompose = true;
        }
    } catch (const std::exception& e) {
        QL_FAIL("Can not decompose credit risk in CRIF for trade id '" << trade->id() << "': " << e.what());
        sensitivityDecompositionWeights.clear();
    }
}

} // namespace analytics
} // namespace ore