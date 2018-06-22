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
#include <ored/utilities/log.hpp>

using QuantLib::Real;

using std::map;

namespace ore {
namespace analytics {

using crossPair = SensitivityCube::crossPair;

SensitivityCubeStream::SensitivityCubeStream(const boost::shared_ptr<SensitivityCube>& cube, 
    const string& currency) 
    : cube_(cube), currency_(currency), itRiskFactor_(cube_->factors().begin()), 
      itCrossPair_(cube_->crossFactors().begin()), tradeIdx_(0) {}

SensitivityRecord SensitivityCubeStream::next() {
    
    SensitivityRecord sr;

    // If exhausted deltas, gammas AND cross gammas, update to next trade and reset iterators
    if (itRiskFactor_ == cube_->factors().end() && itCrossPair_ == cube_->crossFactors().end()) {
        tradeIdx_++;
        itRiskFactor_ = cube_->factors().begin();
        itCrossPair_ = cube_->crossFactors().begin();
    }

    // Give back next record if we have a valid trade index
    if (tradeIdx_ < cube_->tradeIds().size()) {
        sr.tradeId = cube_->tradeIds()[tradeIdx_];
        sr.isPar = false;
        sr.currency = currency_;
        sr.baseNpv = cube_->npv(sr.tradeId);
        
        // Are there more deltas and gammas for current trade ID
        if (itRiskFactor_ != cube_->factors().end()) {
            sr.key_1 = *itRiskFactor_;
            sr.desc_1 = deconstructFactor(cube_->factorDescription(sr.key_1)).second;
            sr.shift_1 = cube_->shiftSize(sr.key_1);
            sr.delta = cube_->delta(sr.tradeId, sr.key_1);
            sr.gamma = cube_->gamma(sr.tradeId, sr.key_1);
            
            itRiskFactor_++;

            TLOG("Next record is: " << sr);
            return sr;
        }
        
        // Are there more cross pairs for current trade ID
        if (itCrossPair_ != cube_->crossFactors().end()) {
            sr.key_1 = itCrossPair_->first;
            sr.desc_1 = deconstructFactor(cube_->factorDescription(sr.key_1)).second;
            sr.shift_1 = cube_->shiftSize(sr.key_1);
            
            sr.key_2 = itCrossPair_->second;
            sr.desc_2 = deconstructFactor(cube_->factorDescription(sr.key_2)).second;
            sr.shift_2 = cube_->shiftSize(sr.key_2);

            sr.gamma = cube_->crossGamma(sr.tradeId, *itCrossPair_);

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
    tradeIdx_ = 0;
    itRiskFactor_ = cube_->factors().begin();
    itCrossPair_ = cube_->crossFactors().begin();
}

}
}
