/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/engine/cvasensitivitycubestream.hpp>
#include <ored/utilities/log.hpp>

using namespace std;
using namespace ore::analytics;

namespace ore {
namespace analytics {

CvaSensitivityCubeStream::CvaSensitivityCubeStream(
    const QuantLib::ext::shared_ptr<NPVSensiCube>& cube, const vector<CvaRiskFactorKey>& descriptions,
    const set<string>& nettingSetIds, const vector<pair<ShiftType, Real>>& shifts, const vector<Period>& cdsGrid,
    const map<string, vector<Real>>& cdsSensis, pair<ShiftType, Real>& cdsShift,
    const map<string, string>& counterpartyMap, const string& currency)
    : cube_(cube), descriptions_(descriptions), nettingSetIds_(nettingSetIds), shifts_(shifts), cdsGrid_(cdsGrid),
      cdsSensis_(cdsSensis), cdsShift_(cdsShift), counterpartyMap_(counterpartyMap), currency_(currency),
      scenarioIdx_(0), nettingIdx_(0), cdsGridIdx_(0), nettingSetIdIter_(nettingSetIds_.begin()) {

    QL_REQUIRE(descriptions_.size() == shifts.size(), "Scenario descriptions and shift vectors must be the same size");
}

/*! Returns the next SensitivityRecord in the stream

    \warning the cube must not change during successive calls to next()!
*/
CvaSensitivityRecord CvaSensitivityCubeStream::next() {

    CvaSensitivityRecord sr;


    // if at last scenario, move to next netting set
    if (scenarioIdx_ >= descriptions_.size() && cdsGridIdx_ >= cdsGrid_.size()) {
        nettingIdx_++;
        nettingSetIdIter_++;
        // reset scenarios
        scenarioIdx_ = 0;
        cdsGridIdx_ = 0;
    }

    // Give back next record if we have a valid trade index
    
    if (nettingIdx_ < nettingSetIds_.size()) {

        sr.nettingSetId = *nettingSetIdIter_;
        sr.baseCva = cube_->getT0(nettingIdx_);
        sr.currency = currency_;

        // handle scenarios first
        if (scenarioIdx_ < descriptions_.size()){
            sr.key = descriptions_[scenarioIdx_];

            sr.shiftType = shifts_[scenarioIdx_].first;
            sr.shiftSize = shifts_[scenarioIdx_].second;

            sr.delta = cube_->get(nettingIdx_, scenarioIdx_) - sr.baseCva;

            // move to next scenario
            scenarioIdx_++;
        } else {
            auto itrc = counterpartyMap_.find(sr.nettingSetId);
            QL_REQUIRE(itrc != counterpartyMap_.end(), "Could not find counterparty for netting set " << sr.nettingSetId);
            sr.key = CvaRiskFactorKey(CvaRiskFactorKey::KeyType::CreditCounterparty, CvaRiskFactorKey::MarginType::Delta, counterpartyMap_[sr.nettingSetId], cdsGrid_[cdsGridIdx_]);

            sr.shiftType = cdsShift_.first;
            sr.shiftSize = cdsShift_.second;

            Real delta = 0.0;
            auto itr = cdsSensis_.find(sr.nettingSetId);
            if (itr != cdsSensis_.end()) {
                vector<Real> cdsSensis = cdsSensis_[sr.nettingSetId];
                if (cdsGridIdx_ >= cdsSensis.size()) {
                    WLOG("No CDS sesnitivitity for " << sr.key);
                } else {
		  //delta = cdsSensis_[sr.nettingSetId].at(cdsGridIdx_) * sr.shiftSize;
		  // CDS sensis are deltas, no need to scale with shift size
		  delta = cdsSensis_[sr.nettingSetId].at(cdsGridIdx_);
                }
            }
            sr.delta = delta;

            // move cds grid on
            cdsGridIdx_++;
        }
    }

    // If we get to here, no more cube sensitivities to process so return empty record
    return sr;
}

//! Resets the stream so that SensitivityRecord objects can be streamed again
void CvaSensitivityCubeStream::reset() {
    scenarioIdx_ = 0;
    nettingIdx_ = 0;
    cdsGridIdx_ = 0;
    nettingSetIdIter_ = nettingSetIds_.begin();
}

} // namespace analytics
} // namespace ore
