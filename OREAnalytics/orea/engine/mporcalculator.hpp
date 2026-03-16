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

/*! \file engine/mporcalculator.hpp
    \brief The cube valuation calculator interface
    \ingroup simulation
*/

#pragma once

#include <orea/engine/valuationcalculator.hpp>

namespace ore {
namespace analytics {

//! MPORCalculator
/*! Calculate NPV for default and close-out time grids
 *  Implicit assumption that MPOR-style date grid is being used
 *  Utilises NPVCalculator for actual NPV calculation
 *
 */
class MPORCalculator : public ValuationCalculator {
public:
    //! base ccy and index to write to
    MPORCalculator(const QuantLib::ext::shared_ptr<NPVCalculator>& npvCalc, Size defaultIndex = 0, Size closeOutIndex = 1)
        : npvCalc_(npvCalc), defaultIndex_(defaultIndex), closeOutIndex_(closeOutIndex) {}

    void calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                   const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                   QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex, Size sample,
                   bool isCloseOut = false) override;

    void calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                     const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                     QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) override;

    void init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) override;
    void initScenario() override;

private:
    QuantLib::ext::shared_ptr<NPVCalculator> npvCalc_;
    Size defaultIndex_, closeOutIndex_;
};

} // namespace analytics
} // namespace ore
