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

ZeroToParCube::ZeroToParCube(const QuantLib::ext::shared_ptr<SensitivityCube>& zeroCube,
                             const QuantLib::ext::shared_ptr<ParSensitivityConverter>& parConverter,
                             const set<RiskFactorKey::KeyType>& typesDisabled, const bool continueOnError)
    : ZeroToParCube(std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>{zeroCube}, parConverter, typesDisabled,
                    continueOnError) {}

ZeroToParCube::ZeroToParCube(const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& zeroCubes,
                             const QuantLib::ext::shared_ptr<ParSensitivityConverter>& parConverter,
                             const set<RiskFactorKey::KeyType>& typesDisabled, const bool continueOnError)
    : zeroCubes_(zeroCubes), parConverter_(parConverter), typesDisabled_(typesDisabled),
      continueOnError_(continueOnError) {

    Size counter = 0;
    for (auto const& k : parConverter_->rawKeys()) {
        factorToIndex_[k] = counter++;
    }
}

map<RiskFactorKey, Real> ZeroToParCube::parDeltas(QuantLib::Size cubeIdx, QuantLib::Size tradeIdx) const {

    DLOG("Calculating par deltas for trade index " << tradeIdx);

    map<RiskFactorKey, Real> result;

    // Get the "par-convertible" zero deltas
    boost::numeric::ublas::vector<Real> zeroDeltas(parConverter_->rawKeys().size(), 0.0);

    QL_REQUIRE(cubeIdx < zeroCubes_.size(),
               "ZeroToParCube::parDeltas(): cubeIdx (" << cubeIdx << ") out of range 0..." << (zeroCubes_.size() - 1));

    const QuantLib::ext::shared_ptr<SensitivityCube>& zeroCube = zeroCubes_[cubeIdx];
    const QuantLib::ext::shared_ptr<NPVSensiCube>& sensiCube = zeroCube->npvCube();

    std::set<RiskFactorKey> rkeys;
    for (auto const& kv : sensiCube->getTradeNPVs(tradeIdx)) {
        if (auto k = zeroCube->upDownFactor(kv.first); k.keytype != RiskFactorKey::KeyType::None)
            rkeys.insert(k);
    }

    for (auto const& rk : rkeys) {
        auto it = factorToIndex_.find(rk);
        if (it == factorToIndex_.end()) {
            if (ParSensitivityAnalysis::isParType(rk.keytype) && typesDisabled_.count(rk.keytype) != 1) {
                if (continueOnError_) {
                    StructuredAnalyticsErrorMessage("Par conversion", "",
                                                    "Par factor " + ore::data::to_string(rk) +
                                                        " not found in factorToIndex map")
                        .log();
                } else {
                    QL_REQUIRE(!ParSensitivityAnalysis::isParType(rk.keytype) || typesDisabled_.count(rk.keytype) == 1,
                               "ZeroToParCube::parDeltas(): par factor " << rk << " not found in factorToIndex map");
                }
            }
        } else {
            zeroDeltas[it->second] = zeroCube->delta(tradeIdx, rk);
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
    for (const auto& f : rkeys) {
        if (!ParSensitivityAnalysis::isParType(f.keytype) || typesDisabled_.count(f.keytype) == 1) {
            Real delta = zeroCube->delta(tradeIdx, f);
            if (!close(delta, 0.0)) {
                result[f] = delta;
            }
        }
    }

    DLOG("Finished calculating par deltas for cube index " << cubeIdx << ", trade index " << tradeIdx);

    return result;
}

map<RiskFactorKey, Real> ZeroToParCube::parDeltas(const string& tradeId) const {

    DLOG("Calculating par deltas for trade " << tradeId);
    map<RiskFactorKey, Real> result;

    Size cubeIdx;
    Size tradeIdx = Null<Size>();
    for (cubeIdx = 0; cubeIdx < zeroCubes_.size() && tradeIdx == Null<Size>(); ++cubeIdx) {
        try {
            tradeIdx = zeroCubes_[cubeIdx]->npvCube()->getTradeIndex(tradeId);
            break;
        } catch (...) {
        }
    }
    QL_REQUIRE(tradeIdx != Null<Size>(), "ZeroToParCube::parDeltas(): tradeId '"
                                             << tradeId << "' not found in " << zeroCubes_.size() << " zero cubes.");

    result = parDeltas(cubeIdx, tradeIdx);

    DLOG("Finished calculating par deltas for trade " << tradeId);

    return result;
}

} // namespace analytics
} // namespace ore
