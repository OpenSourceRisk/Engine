/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/engine/parsensitivitycubestream.hpp>

#include <orea/engine/sensitivityrecord.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>

using ore::analytics::deconstructFactor;
using ore::analytics::SensitivityRecord;

namespace ore {
namespace analytics {

// Note: iterator initialisation below works because currentDeltas_ is
//       (empty) initialised before itCurrent_
ParSensitivityCubeStream::ParSensitivityCubeStream(const boost::shared_ptr<ZeroToParCube>& cube, const string& currency)
    : cube_(cube), currency_(currency), tradeIdx_(cube_->zeroCube()->tradeIdx().begin()), itCurrent_(currentDeltas_.begin()) {
    // Call init
    init();
}

SensitivityRecord ParSensitivityCubeStream::next() {

    SensitivityRecord sr;

    while (itCurrent_ == currentDeltas_.end() && tradeIdx_ != cube_->zeroCube()->tradeIdx().end()) {
        // Move to next trade
        tradeIdx_++;
        // update par deltas
        if (tradeIdx_ != cube_->zeroCube()->tradeIdx().end()) {
            DLOG("Retrieving par deltas for trade " << tradeIdx_->first);
            currentDeltas_ = cube_->parDeltas(tradeIdx_->second);
            itCurrent_ = currentDeltas_.begin();
            DLOG("There are " << currentDeltas_.size() << " par deltas for trade " << tradeIdx_->first);
        }

    }
    if (tradeIdx_ != cube_->zeroCube()->tradeIdx().end()) {
        // Populate current sensitivity record
        
        sr.tradeId = tradeIdx_->first;
        sr.isPar = true;
        sr.currency = currency_;
        sr.baseNpv = cube_->zeroCube()->npv(tradeIdx_->second);

        if (itCurrent_ != currentDeltas_.end()) {
            DLOG("Processing par delta [" << itCurrent_->first << ", " << itCurrent_->second << "]");
            sr.key_1 = itCurrent_->first;
            auto fullDescription = cube_->zeroCube()->factorDescription(sr.key_1);
            sr.desc_1 = deconstructFactor(fullDescription).second;
            sr.shift_1 = cube_->zeroCube()->shiftSize(sr.key_1);
            sr.delta = itCurrent_->second;
            sr.gamma = Null<Real>();
            // Move iterator to next par delta
            itCurrent_++;
        }
    } 
    return sr;
}

void ParSensitivityCubeStream::reset() {
    // Reset all
    tradeIdx_ = cube_->zeroCube()->tradeIdx().begin();
    currentDeltas_ = {};
    itCurrent_ = currentDeltas_.begin();
    // Call init
    init();
}

void ParSensitivityCubeStream::init() {
    // If we have trade IDs in the underlying cube
    if (!cube_->zeroCube()->tradeIdx().empty()) {
        tradeIdx_ = cube_->zeroCube()->tradeIdx().begin();
        DLOG("Retrieving par deltas for trade " << tradeIdx_->first);
        currentDeltas_ = cube_->parDeltas(tradeIdx_->second);
        itCurrent_ = currentDeltas_.begin();
        DLOG("There are " << currentDeltas_.size() << " par deltas for trade " << tradeIdx_->first);
    }
}

} // namespace analytics
} // namespace ore
