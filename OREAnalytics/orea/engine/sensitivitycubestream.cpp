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

#include <orea/engine/sensitivitycubestream.hpp>

#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/log.hpp>

using QuantLib::Real;

using std::map;

namespace ore {
namespace analytics {

using crossPair = SensitivityCube::crossPair;

SensitivityCubeStream::SensitivityCubeStream(const boost::shared_ptr<SensitivityCube>& cube, const string& currency)
    : cube_(cube), currency_(currency), upRiskFactor_(cube_->upFactors().begin()),
      downRiskFactor_(cube_->downFactors().begin()), itCrossPair_(cube_->crossFactors().begin()),
      tradeIdx_(cube_->tradeIdx().begin()), canComputeGamma_(false) {

    // Set the value of canComputeGamma_ based on up and down risk factors.
    const auto& upFactors = cube_->upFactors();
    const auto& downFactors = cube_->downFactors();
    if (upFactors.size() == downFactors.size()) {
        auto pred = [](decltype(*upFactors.left.begin()) a, pair<RiskFactorKey, SensitivityCube::FactorData> b) {
            return a.first == b.first;
        };
        canComputeGamma_ = equal(upFactors.left.begin(), upFactors.left.end(), downFactors.begin(), pred);
    }
}

SensitivityRecord SensitivityCubeStream::next() {

    SensitivityRecord sr;

    const auto& upFactors = cube_->upFactors();
    const auto& downFactors = cube_->downFactors();

    // If exhausted deltas, gammas AND cross gammas, update to next trade and reset iterators
    if (upRiskFactor_ == upFactors.end() && itCrossPair_ == cube_->crossFactors().end()) {
        tradeIdx_++;
        upRiskFactor_ = upFactors.begin();
        downRiskFactor_ = downFactors.begin();
        itCrossPair_ = cube_->crossFactors().begin();
    }

    // Give back next record if we have a valid trade index
    if (tradeIdx_ != cube_->tradeIdx().end()) {
        Size tradeIdx = tradeIdx_->second;
        sr.tradeId = tradeIdx_->first;
        sr.isPar = false;
        sr.currency = currency_;
        sr.baseNpv = cube_->npv(tradeIdx);

        // Are there more deltas and gammas for current trade ID
        if (upRiskFactor_ != upFactors.end()) {
            Size usrx = upRiskFactor_->right.index;
            sr.key_1 = upRiskFactor_->left;
            sr.desc_1 = upRiskFactor_->right.factorDesc;
            sr.shift_1 = upRiskFactor_->right.shiftSize;

            if (!cube_->twoSidedDelta(sr.key_1.keytype)) {
                sr.delta = cube_->delta(tradeIdx, usrx);
            } else {
                Size downIdx = downFactors.at(sr.key_1).index;
                sr.delta = cube_->delta(tradeIdx, usrx, downIdx);
            }

            if (canComputeGamma_ && downRiskFactor_ != downFactors.end()) {
                Size dsrx = downRiskFactor_->second.index;
                sr.gamma = cube_->gamma(tradeIdx, usrx, dsrx);
                downRiskFactor_++;
            } else {
                sr.gamma = Null<Real>(); // marks na result
            }

            upRiskFactor_++;

            TLOG("Next record is: " << sr);
            return sr;
        }

        // Are there more cross pairs for current trade ID
        if (itCrossPair_ != cube_->crossFactors().end()) {
            sr.key_1 = itCrossPair_->first.first;
            sr.desc_1 = std::get<0>(itCrossPair_->second).factorDesc;
            sr.shift_1 = std::get<0>(itCrossPair_->second).shiftSize;

            sr.key_2 = itCrossPair_->first.second;
            sr.desc_2 = std::get<1>(itCrossPair_->second).factorDesc;
            sr.shift_2 = std::get<1>(itCrossPair_->second).shiftSize;

            Size id_1, id_2, id_x;
            id_1 = std::get<0>(itCrossPair_->second).index;
            id_2 = std::get<1>(itCrossPair_->second).index;
            id_x = std::get<2>(itCrossPair_->second);

            sr.gamma = cube_->crossGamma(tradeIdx, id_1, id_2, id_x);

            itCrossPair_++;

            TLOG("Next record is: " << sr);
            return sr;
        }
    }

    // If we get to here, no more cube sensitivities to process so return empty record
    TLOG("Next record is: " << sr);
    return sr;
}

void SensitivityCubeStream::reset() {
    // Reset indices and iterators
    tradeIdx_ = cube_->tradeIdx().begin();
    upRiskFactor_ = cube_->upFactors().begin();
    downRiskFactor_ = cube_->downFactors().begin();
    itCrossPair_ = cube_->crossFactors().begin();
}

} // namespace analytics
} // namespace ore
