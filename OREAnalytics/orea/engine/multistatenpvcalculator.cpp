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

/*! \file engine/multistatenpvcalculator.hpp
    \brief a calculator that computes npvs for a vector of credit states
    \ingroup simulation
*/

#include <orea/engine/multistatenpvcalculator.hpp>

#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

void MultiStateNPVCalculator::calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                        QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                        QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date,
                                        Size dateIndex, Size sample, bool isCloseOut) {
    if (!isCloseOut) {
        std::vector<Real> stateNpvs = multiStateNpv(tradeIndex, trade, simMarket);
        for (Size i = 0; i < stateNpvs.size(); ++i) {
            outputCube->set(stateNpvs[i], tradeIndex, dateIndex, sample, index_ + i);
        }
    }
}

void MultiStateNPVCalculator::calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                          const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                          QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                          QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) {
    std::vector<Real> stateNpvs = multiStateNpv(tradeIndex, trade, simMarket);
    for (Size i = 0; i < stateNpvs.size(); ++i) {
        outputCube->setT0(stateNpvs[i], tradeIndex, index_ + i);
    }
}

std::vector<Real> MultiStateNPVCalculator::multiStateNpv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade,
                                                         const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    // handle expired trades

    if (trade->instrument()->qlInstrument()->isExpired()) {
        return std::vector<Real>(states_, 0.0);
    }

    if (auto tmp = trade->instrument()->qlInstrument()->additionalResults().find("stateNpv");
        tmp != trade->instrument()->qlInstrument()->additionalResults().end()) {

        // we have a stateNpv result

        std::vector<Real> stateNpv;
        try {
            stateNpv = boost::any_cast<std::vector<Real>>(tmp->second);
        } catch (const std::exception& e) {
            QL_FAIL("unexpected type of result stateNpv: " << e.what());
        }
        for (auto& n : stateNpv) {
            if (!close_enough(n, 0.0)) {
                n *= trade->instrument()->multiplier() * trade->instrument()->multiplier2();
                Real fx = fxRates_[tradeCcyIndex_[tradeIndex]];
                Real numeraire = simMarket->numeraire();
                n *= fx / numeraire;
            }
        }

        return stateNpv;

    } else {

        // we do not have a stateNpv result and use the usual npv for all states

        std::vector<Real> npv(states_, NPVCalculator::npv(tradeIndex, trade, simMarket));
        return npv;
    }
}

} // namespace analytics
} // namespace ore
