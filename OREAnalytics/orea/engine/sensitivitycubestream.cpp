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

#pragma once

#include <orea/engine/sensitivitycubestream.hpp>

#include <orea/scenario/shiftscenariogenerator.hpp>

using QuantLib::Real;

using std::map;

namespace ore {
namespace analytics {

using crossPair = SensitivityCube::crossPair;

SensitivityCubeStream::SensitivityCubeStream(const boost::shared_ptr<SensitivityCube>& cube, 
    const string& currency) 
    : cube_(cube), currency_(currency), riskFactorIdx_(0), crossPairIdx_(0), tradeIdx_(0) {}

SensitivityRecord SensitivityCubeStream::next() {
    
    SensitivityRecord sr;

    // If trivial case, i.e. cube has no sensitivities, return empty record
    if (cube_->factors().size() == 0 && cube_->crossFactors().size() == 0) {
        return sr;
    }

    // If exhausted deltas, gammas AND cross gammas, update to next trade and reset sensi index
    if (riskFactorIdx_ == cube_->factors().size() && crossPairIdx_ == cube_->crossFactors().size()) {
        tradeIdx_++;
        riskFactorIdx_ = 0;
        crossPairIdx_ = 0;
    }

    // Give back next record if we have a valid trade index
    if (tradeIdx_ < cube_->tradeIds().size()) {
        sr.tradeId = cube_->tradeIds()[tradeIdx_];
        sr.isPar = false;
        sr.currency = currency_;
        sr.baseNpv = cube_->npv(sr.tradeId);
        
        // Are there more deltas and gammas for current trade ID
        if (riskFactorIdx_ < cube_->factors().size()) {
            sr.key_1 = *std::next(cube_->factors().begin(), riskFactorIdx_);
            sr.desc_1 = deconstructFactor(cube_->factorDescription(sr.key_1)).second;
            sr.shift_1 = cube_->shiftSize(sr.key_1);
            sr.delta = cube_->delta(sr.tradeId, sr.key_1);
            sr.gamma = cube_->gamma(sr.tradeId, sr.key_1);
            
            riskFactorIdx_++;

            return sr;
        }
        
        // Are there more cross pairs for current trade ID
        if (crossPairIdx_ < cube_->crossFactors().size()) {
            crossPair cp = *std::next(cube_->crossFactors().begin(), crossPairIdx_);
            
            sr.key_1 = cp.first;
            sr.desc_1 = deconstructFactor(cube_->factorDescription(sr.key_1)).second;
            sr.shift_1 = cube_->shiftSize(sr.key_1);
            
            sr.key_2 = cp.second;
            sr.desc_2 = deconstructFactor(cube_->factorDescription(sr.key_2)).second;
            sr.shift_2 = cube_->shiftSize(sr.key_2);

            sr.gamma = cube_->crossGamma(sr.tradeId, cp);

            crossPairIdx_++;

            return sr;
        }
    }

    // If we get to here, no more cube sensitivities to process so return empty record
    return sr;
}

void SensitivityCubeStream::reset() {
    // Reset all indices to 0
    tradeIdx_ = 0;
    riskFactorIdx_ = 0;
    crossPairIdx_ = 0;
}

}
}
