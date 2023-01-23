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

#include <orea/engine/zerotoparcube.hpp>

#include <orea/app/structuredanalyticserror.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/numeric/ublas/vector.hpp>

using namespace QuantLib;
using namespace ore::analytics;

using ore::data::Report;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

ZeroToParCube::ZeroToParCube(const boost::shared_ptr<SensitivityCube>& zeroCube,
                             const boost::shared_ptr<ParSensitivityConverter>& parConverter,
                             const set<RiskFactorKey::KeyType>& typesDisabled,
                             const bool continueOnError)
    : zeroCube_(zeroCube), parConverter_(parConverter), typesDisabled_(typesDisabled),
    continueOnError_(continueOnError) {

    Size counter = 0;
    for (auto const& k : parConverter_->rawKeys()) {
        factorToIndex_[k] = counter++;
    }
}

map<RiskFactorKey, Real> ZeroToParCube::parDeltas(QuantLib::Size tradeIdx) const {

    DLOG("Calculating par deltas for trade index " << tradeIdx);

    map<RiskFactorKey, Real> result;

    // Get the "par-convertible" zero deltas
    boost::numeric::ublas::vector<Real> zeroDeltas(parConverter_->rawKeys().size(), 0.0);
    const boost::shared_ptr<NPVSensiCube>& sensiCube = zeroCube_->npvCube();

    for (auto const& kv : sensiCube->getTradeNPVs(tradeIdx)) {
        auto factor = zeroCube_->upDownFactor(kv.first);
        // index might not belong to an up/down scenario
        if (factor.keytype != RiskFactorKey::KeyType::None) {
            auto it = factorToIndex_.find(factor);
            if (it == factorToIndex_.end()) {
                if (ParSensitivityAnalysis::isParType(factor.keytype) && typesDisabled_.count(factor.keytype) != 1) {
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("ParConversion", "par factor " + ore::data::to_string(factor) + " not found in factorToIndex map"));
                    } else {
                        QL_REQUIRE(!ParSensitivityAnalysis::isParType(factor.keytype) ||
                                typesDisabled_.count(factor.keytype) == 1,
                                "ZeroToParCube::parDeltas(): par factor " << factor << " not found in factorToIndex map");
                    }
                }
            } else if (!zeroCube_->twoSidedDelta(factor.keytype)) {
                zeroDeltas[it->second] = zeroCube_->delta(tradeIdx, kv.first);
            } else {
                Size downIdx = zeroCube_->downFactors().at(factor).index;
                zeroDeltas[it->second] = zeroCube_->delta(tradeIdx, kv.first, downIdx);
            }
        }
    }

    // Convert the zero deltas to par deltas
    boost::numeric::ublas::vector<Real> parDeltas = parConverter_->convertSensitivity(zeroDeltas);
    Size counter = 0;
    for (const auto& key : parConverter_->parKeys()) {
        if (!close(parDeltas[counter], 0.0)) {
            result[key] = parDeltas[counter];
        }
        counter++;
    }

    // Add non-zero deltas that do not need to be converted from underlying zero cube
    for (const auto& key : zeroCube_->upFactors().left) {
        if (!ParSensitivityAnalysis::isParType(key.first.keytype) || typesDisabled_.count(key.first.keytype) == 1) {
            Real delta = 0.0;
            if (!zeroCube_->twoSidedDelta(key.first.keytype)) {
                delta = zeroCube_->delta(tradeIdx, key.second.index);
            } else {
                Size downIdx = zeroCube_->downFactors().at(key.first).index;
                delta = zeroCube_->delta(tradeIdx, key.second.index, downIdx);
            }
            if (!close(delta, 0.0)) {
                result[key.first] = delta;
            }
        }
    }

    DLOG("Finished calculating par deltas for trade index " << tradeIdx);

    return result;
}

map<RiskFactorKey, Real> ZeroToParCube::parDeltas(const string& tradeId) const {

    DLOG("Calculating par deltas for trade " << tradeId);
    map<RiskFactorKey, Real> result;
    result = parDeltas(zeroCube_->npvCube()->getTradeIndex(tradeId));

    DLOG("Finished calculating par deltas for trade " << tradeId);

    return result;
}

} // namespace analytics
} // namespace ore
