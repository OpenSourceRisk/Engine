/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/mclgmswapengine.hpp>

#include <ql/exercise.hpp>

namespace QuantExt {

void McLgmSwapEngine::calculate() const {
    leg_ = arguments_.legs;
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());
    payer_ = arguments_.payer;
    exercise_ = nullptr;
    McMultiLegBaseEngine::calculate();
    results_.value = resultValue_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
} // McLgmSwaptionEngine::calculate

} // namespace QuantExt
