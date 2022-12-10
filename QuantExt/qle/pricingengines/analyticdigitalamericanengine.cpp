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
#include <qle/pricingengines/analyticdigitalamericanengine.hpp>
#include <utility>

using std::string;
using std::vector;

namespace QuantExt {

using namespace QuantLib;

void AnalyticDigitalAmericanEngine::calculate() const {

    QuantLib::AnalyticDigitalAmericanEngine::calculate();

    // If a payDate was provided (and is greater than the expiryDate)
    if (payDate_ > arguments_.exercise->lastDate()) {
        Rate payDateDiscount = process_->riskFreeRate()->discount(payDate_);
        Rate expiryDateDiscount = process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
        Rate factor = payDateDiscount / expiryDateDiscount;
        results_.value *= factor;

        auto discTouchProbIt = results_.additionalResults.find("discountedTouchProbability");
        if (discTouchProbIt != results_.additionalResults.end()) {
            if (knock_in())
                discTouchProbIt->second = boost::any_cast<Real>(discTouchProbIt->second) * factor;
            else
                discTouchProbIt->second = 1.0 - (factor * (1.0 - boost::any_cast<Real>(discTouchProbIt->second)));
        }
    }

    if (flipResults_) {
        // Invert spot, forward, strike
        auto resToInvert = vector<string>({"spot", "forward", "strike"});
        for (const string& res : resToInvert) {
            auto it = results_.additionalResults.find(res);
            if (it != results_.additionalResults.end())
                it->second = 1. / boost::any_cast<Real>(it->second);
        }

        // Swap riskFreeDiscount and dividendDiscount
        auto rfDiscountIt = results_.additionalResults.find("riskFreeDiscount");
        auto divDiscountIt = results_.additionalResults.find("dividendDiscount");
        if (rfDiscountIt != results_.additionalResults.end() && divDiscountIt != results_.additionalResults.end())
            std::swap(rfDiscountIt->second, divDiscountIt->second);
    }
}

} // namespace QuantExt
