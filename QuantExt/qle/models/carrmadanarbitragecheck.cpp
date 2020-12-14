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

#include <qle/models/carrmadanarbitragecheck.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <sstream>

namespace QuantExt {

CarrMadanMarginalProbability::CarrMadanMarginalProbability(const std::vector<Real>& strikes,
                                                           const std::vector<Real>& callPrices)
    : strikes_(strikes), callPrices_(callPrices) {

    // check input
    QL_REQUIRE(strikes.size() == callPrices.size(), "CarrMadanMarginalProbability: strikes ("
                                                        << strikes.size() << ") inconsistent to callPrices ("
                                                        << callPrices.size() << ")";)

    QL_REQUIRE(strikes.size() >= 2,
               "CarrMadanMarginalProbability: at least 2 strikes required (got " << strikes.size() << ")");

    // compute Q
    std::vector<Real> Q(strikes.size() - 1);
    for (Size i = 1; i < strikes.size(); ++i) {
        Q[i - 1] = (callPrices[i - 1] - callPrices[i]) / (strikes[i] - strikes[i - 1]);
    }

    // compute BS
    std::vector<Real> BS(strikes.size() - 2);
    for (Size i = 1; i < strikes.size() - 1; ++i) {
        BS[i - 1] = callPrices[i - 1] -
                    (strikes[i + 1] - strikes[i - 1]) / (strikes[i + 1] - strikes[i]) * callPrices[i] +
                    (strikes[i] - strikes[i - 1]) / (strikes[i + 1] - strikes[i]) * callPrices[i + 1];
    }

    // perform the checks 1, 2 from the paper, and populate the set of violations

    violationsType1_.resize(Q.size(), false);
    violationsType2_.resize(BS.size(), false);

    // check 1: Q(i,j) in [0,1]
    for (Size i = 0; i < Q.size(); ++i) {
        if ((Q[i] < 0.0 && !close_enough(Q[i], 0.0)) || (Q[i] > 1.0 && !close_enough(Q[i], 1.0))) {
            violationsType1_[i] = true;
        }
    }

    // check 2: BS(i,j) >= 0
    for (Size i = 0; i < BS.size(); ++i) {
        if (BS[i] < 0.0 && !close_enough(BS[i], 0.0)) {
            violationsType2_[i] = true;
        }
    }

    // compute q
    q_.resize(Q.size() - 1);
    for (Size i = 0; i < Q.size() - 1; ++i) {
        q_[i] = Q[i] - Q[i + 1];
    }
}

const std::vector<Real>& CarrMadanMarginalProbability::strikes() const { return strikes_; }

const std::vector<Real>& CarrMadanMarginalProbability::callPrices() const { return callPrices_; }

bool CarrMadanMarginalProbability::arbitrageFree() const {
    return violationsType1_.empty() && violationsType2_.empty();
}

const std::vector<bool> CarrMadanMarginalProbability::violationsType1() const { return violationsType1_; }

const std::vector<bool> CarrMadanMarginalProbability::violationsType2() const { return violationsType2_; }

std::string arbitrageViolationsAsString(const CarrMadanMarginalProbability& cm) {
    std::ostringstream out;
    out << "strikes = " << cm.strikes().front() << " ... " << cm.strikes().back() << ", type 1: ";
    for (Size i = 0; i < cm.violationsType1().size(); ++i) {
        out << (cm.violationsType1()[i] ? "x" : ".");
    }
    out << " type 2: ";
    for (Size i = 0; i < cm.violationsType2().size(); ++i) {
        out << (cm.violationsType2()[i] ? "x" : ".");
    }
    return out.str();
}

const std::vector<Real> CarrMadanMarginalProbability::density() const { return q_; }

} // namespace QuantExt
