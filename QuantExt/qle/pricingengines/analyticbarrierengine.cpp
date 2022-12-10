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
#include <qle/pricingengines/analyticbarrierengine.hpp>
#include <utility>

namespace QuantExt {

using namespace QuantLib;

AnalyticBarrierEngine::AnalyticBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process, const Date& paymentDate)
    : QuantLib::AnalyticBarrierEngine(process), process_(std::move(process)), paymentDate_(paymentDate) {
    registerWith(process_);
}

void AnalyticBarrierEngine::calculate() const {
    QuantLib::AnalyticBarrierEngine::calculate();

    // If a payDate was provided (and is greater than the expiryDate)
    if (paymentDate_ > arguments_.exercise->lastDate()) {
        Rate payDateDiscount = process_->riskFreeRate()->discount(paymentDate_);
        Rate expiryDateDiscount = process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
        Rate factor = payDateDiscount / expiryDateDiscount;
        results_.value *= factor;
    }

    if (paymentDate_ != Date())
        results_.additionalResults["settlementDate"] = paymentDate_;
}

} // namespace QuantExt
