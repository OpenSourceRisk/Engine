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
    : cube_(cube), currency_(currency), tradeIdx_(0), itCurrent_(currentDeltas_.begin()) {

    // Call init
    init();
}

SensitivityRecord ParSensitivityCubeStream::next() {

    SensitivityRecord sr;

    // Underlying cube's trade IDs
    const auto& tradeIds = cube_->zeroCube()->tradeIds();

    while (itCurrent_ == currentDeltas_.end() && tradeIdx_ < tradeIds.size()) {
        // Move to next trade
        tradeIdx_++;

        // If no more trade IDs, return empty record
        if (tradeIdx_ == tradeIds.size())
            return sr;
        tradeId_ = tradeIds[tradeIdx_];

        // If more trades, update par deltas and iterator to them
        // They may be empty, in which case, itCurrent_ is still end()
        DLOG("Retrieving par deltas for trade " << tradeId_);
        currentDeltas_ = cube_->parDeltas(tradeIdx_);
        itCurrent_ = currentDeltas_.begin();
        DLOG("There are " << currentDeltas_.size() << " par deltas for trade " << tradeId_);
    }

    // Populate current sensitivity record
    DLOG("Processing par delta [" << itCurrent_->first << ", " << itCurrent_->second << "]");
    sr.tradeId = tradeId_;
    sr.isPar = true;
    sr.currency = currency_;
    sr.key_1 = itCurrent_->first;
    auto fullDescription = cube_->zeroCube()->factorDescription(sr.key_1);
    sr.desc_1 = deconstructFactor(fullDescription).second;
    sr.shift_1 = cube_->zeroCube()->shiftSize(sr.key_1);
    sr.baseNpv = cube_->zeroCube()->npv(tradeIdx_);
    sr.delta = itCurrent_->second;
    sr.gamma = Null<Real>();

    // Move iterator to next par delta
    itCurrent_++;

    // Return record
    return sr;
}

void ParSensitivityCubeStream::reset() {
    // Reset all
    tradeIdx_ = 0;
    tradeId_ = cube_->zeroCube()->tradeIds().front();
    currentDeltas_ = {};
    itCurrent_ = currentDeltas_.begin();

    // Call init
    init();
}

void ParSensitivityCubeStream::init() {
    // If we have trade IDs in the underlying cube
    if (!cube_->zeroCube()->tradeIds().empty()) {
        tradeId_ = cube_->zeroCube()->tradeIds().front();
        DLOG("Retrieving par deltas for trade " << tradeId_);
        currentDeltas_ = cube_->parDeltas(tradeId_);
        itCurrent_ = currentDeltas_.begin();
        DLOG("There are " << currentDeltas_.size() << " par deltas for trade " << tradeId_);
    }
}

} // namespace analytics
} // namespace ore
