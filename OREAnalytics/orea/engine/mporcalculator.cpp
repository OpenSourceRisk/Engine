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

/*! \file engine/valuationcalculator.hpp
    \brief The cube valuation calculator interface
    \ingroup simulation
*/

#include <orea/engine/mporcalculator.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

void MPORCalculator::init(const boost::shared_ptr<Portfolio>& portfolio,
                          const boost::shared_ptr<SimMarket>& simMarket) {
    DLOG("init MPORCalculator")
    npvCalc_->init(portfolio, simMarket);
}

void MPORCalculator::initScenario() { npvCalc_->initScenario(); }

void MPORCalculator::calculate(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                               const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube,
                               boost::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                               Size sample, bool isCloseOut) {
    Size index = isCloseOut ? closeOutIndex_ : defaultIndex_;
    Real npv = npvCalc_->npv(tradeIndex, trade, simMarket);
    outputCube->set(npv * (isCloseOut ? simMarket->numeraire() : 1.0), tradeIndex, dateIndex, sample, index);
}

void MPORCalculator::calculateT0(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                                 const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube,
                                 boost::shared_ptr<NPVCube>& outputCubeNettingSet) {
    Real npv = npvCalc_->npv(tradeIndex, trade, simMarket);
    //! note that when we calculate t0 NPV we will always store it according to the defaultIndex_
    // the closeOutIndex_ is not utilised for t=0 and has no meaning
    outputCube->setT0(npv, tradeIndex, defaultIndex_);
}
} // namespace analytics
} // namespace ore
