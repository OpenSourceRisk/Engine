/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 pr The license is also available online at <http://opensourcerisk.org>

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

SensitivityCubeStream::SensitivityCubeStream(const QuantLib::ext::shared_ptr<SensitivityCube>& cube, const string& currency)
    : SensitivityCubeStream(std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>{cube}, currency) {}

SensitivityCubeStream::SensitivityCubeStream(const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& cubes,
                                             const string& currency)
    : cubes_(cubes), currency_(currency), canComputeGamma_(false) {

    // Set the value of canComputeGamma_ based on up and down risk factors.

    canComputeGamma_ = true;
    for (const auto& cube : cubes_) {
        const auto& upFactors = cube->upFactors();
        const auto& downFactors = cube->downFactors();
        if (upFactors.size() == downFactors.size() &&
            equal(upFactors.begin(), upFactors.end(), downFactors.begin(),
                  [](const auto& a, const auto& b) { return a.first == b.first; }))
            continue;
        canComputeGamma_ = false;
        break;
    }

    reset();
}

SensitivityRecord SensitivityCubeStream::next() {

    if (cubes_.size() == 0)
        return SensitivityRecord();

    while (tradeIdx_ != cubes_[currentCubeIdx_]->tradeIdx().end() && currentDeltaKey_ == currentDeltaKeys_.end() &&
           currentCrossGammaKey_ == currentCrossGammaKeys_.end()) {
        ++tradeIdx_;
        updateForNewTrade();
    }

    if (tradeIdx_ == cubes_[currentCubeIdx_]->tradeIdx().end()) {
        if (currentCubeIdx_ < cubes_.size() - 1) {
            ++currentCubeIdx_;
            tradeIdx_ = cubes_[currentCubeIdx_]->tradeIdx().begin();
            updateForNewTrade();
            return next();
        } else {
            return SensitivityRecord();
        }
    }

    SensitivityRecord sr;
    Size tradeIdx = tradeIdx_->second;
    sr.tradeId = tradeIdx_->first;
    sr.isPar = false;
    sr.currency = currency_;
    sr.baseNpv = cubes_[currentCubeIdx_]->npv(tradeIdx);

    if (currentDeltaKey_ != currentDeltaKeys_.end()) {
        auto const& fd = cubes_[currentCubeIdx_]->upThenDownFactorData(*currentDeltaKey_);
        sr.key_1 = *currentDeltaKey_;
        sr.desc_1 = fd.factorDesc;
        sr.shift_1 = fd.targetShiftSize;
        sr.delta = cubes_[currentCubeIdx_]->delta(tradeIdx_->first, *currentDeltaKey_);
        if (canComputeGamma_)
            sr.gamma = cubes_[currentCubeIdx_]->gamma(tradeIdx_->first, *currentDeltaKey_);
        else
            sr.gamma = Null<Real>();
        ++currentDeltaKey_;
    } else if (currentCrossGammaKey_ != currentCrossGammaKeys_.end()) {
        auto const& fd = cubes_[currentCubeIdx_]->crossFactors().at(*currentCrossGammaKey_);
        sr.key_1 = currentCrossGammaKey_->first;
        sr.desc_1 = std::get<0>(fd).factorDesc;
        sr.shift_1 = std::get<0>(fd).targetShiftSize;
        sr.key_2 = currentCrossGammaKey_->second;
        sr.desc_2 = std::get<1>(fd).factorDesc;
        sr.shift_2 = std::get<1>(fd).targetShiftSize;
        sr.gamma = cubes_[currentCubeIdx_]->crossGamma(tradeIdx_->first, *currentCrossGammaKey_);
        ++currentCrossGammaKey_;
    }

    TLOG("Next record is: " << sr);
    return sr;
}

void SensitivityCubeStream::updateForNewTrade() {
    currentDeltaKeys_.clear();
    currentCrossGammaKeys_.clear();

    if (tradeIdx_ != cubes_[currentCubeIdx_]->tradeIdx().end()) {

        // add delta keys

        for (auto const& [idx, _] : cubes_[currentCubeIdx_]->npvCube()->getTradeNPVs(tradeIdx_->second)) {
            if (auto k = cubes_[currentCubeIdx_]->upDownFactor(idx); k.keytype != RiskFactorKey::KeyType::None)
                currentDeltaKeys_.insert(k);
        }

        // add cross gamma keys

        for (auto const& [crossPair, data] : cubes_[currentCubeIdx_]->crossFactors()) {
            // scaling of cross gamma is not relevant here, so we use 1
            if (!close_enough(cubes_[currentCubeIdx_]->crossGamma(tradeIdx_->second, std::get<0>(data).index,
                                                                  std::get<1>(data).index, std::get<2>(data), 1.0, 1.0),
                              0.0)) {
                currentCrossGammaKeys_.insert(crossPair);

                // make sure, delta keys contain both cross keys, that's a guarantee of the SensitivityCubeStream

                currentDeltaKeys_.insert(crossPair.first);
                currentDeltaKeys_.insert(crossPair.second);
            }
        }
    }

    currentDeltaKey_ = currentDeltaKeys_.begin();
    currentCrossGammaKey_ = currentCrossGammaKeys_.begin();
}

void SensitivityCubeStream::reset() {
    currentCubeIdx_ = 0;
    if (cubes_.size() > 0) {
        tradeIdx_ = cubes_[currentCubeIdx_]->tradeIdx().begin();
        updateForNewTrade();
    }
}

} // namespace analytics
} // namespace ore
