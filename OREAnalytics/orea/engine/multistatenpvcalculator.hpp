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

#pragma once

#include <orea/engine/valuationcalculator.hpp>

namespace ore {
namespace analytics {

//! MultiStateNPVCalculator
/*! Calculate multiple state NPVs (use addtional result field stateNpv)
    See NPVCalculator for more conventions of the stored NPVs. */
class MultiStateNPVCalculator : public NPVCalculator {
public:
    //! base ccy and index to write to
    MultiStateNPVCalculator(const std::string& baseCcyCode, Size index, Size states)
        : NPVCalculator(baseCcyCode, index), states_(states) {}

    void calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                   const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                   QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex, Size sample,
                   bool isCloseOut = false) override;

    void calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                     const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                     QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) override;

    std::vector<Real> multiStateNpv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade,
                                    const QuantLib::ext::shared_ptr<SimMarket>& simMarket);

protected:
    Size states_;
};

} // namespace analytics
} // namespace ore
