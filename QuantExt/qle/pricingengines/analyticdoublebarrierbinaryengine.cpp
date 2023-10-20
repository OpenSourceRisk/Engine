/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ql/exercise.hpp>
#include <qle/pricingengines/analyticdoublebarrierbinaryengine.hpp>
#include <utility>

namespace QuantExt {

using namespace QuantLib;

void AnalyticDoubleBarrierBinaryEngine::calculate() const {

    QuantLib::AnalyticDoubleBarrierBinaryEngine::calculate();

    // If a payDate was provided (and is greater than the expiryDate)
    if (payDate_ > arguments_.exercise->lastDate()) {
        Rate payDateDiscount = process_->riskFreeRate()->discount(payDate_);
        Rate expiryDateDiscount = process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
        Rate factor = payDateDiscount / expiryDateDiscount;
        results_.value *= factor;
    }

    if (flipResults_) {
        // Invert spot, costOfCarry
        auto it = results_.additionalResults.find("spot");
        if (it != results_.additionalResults.end())
            it->second = 1. / boost::any_cast<Real>(it->second);
        it = results_.additionalResults.find("costOfCarry");
        if (it != results_.additionalResults.end())
            it->second = -1. * boost::any_cast<Real>(it->second);

        // Swap riskFreeRate and dividendYield
        auto rfDiscountIt = results_.additionalResults.find("riskFreeRate");
        auto divDiscountIt = results_.additionalResults.find("dividendYield");
        if (rfDiscountIt != results_.additionalResults.end() && divDiscountIt != results_.additionalResults.end())
            std::swap(rfDiscountIt->second, divDiscountIt->second);

        // Invert and swap barrierLow and barrierHigh
        auto barrierLowIt = results_.additionalResults.find("barrierLow");
        auto barrierHighIt = results_.additionalResults.find("barrierHigh");
        if (barrierLowIt != results_.additionalResults.end() && barrierHighIt != results_.additionalResults.end()) {
            barrierLowIt->second = 1. / boost::any_cast<Real>(barrierLowIt->second);
            barrierHighIt->second = 1. / boost::any_cast<Real>(barrierHighIt->second);
            std::swap(barrierLowIt->second, barrierHighIt->second);
        }
    }
}

} // namespace QuantExt
