/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/mclgmbondengine.hpp>

namespace QuantExt {

void McLgmBondEngine::calculate() const {


    leg_ = { arguments_.cashflows };

    // todo: base currency of CAM or NPV currency of underlying bond ???
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());

    // fixed for  bonds ...
    payer_.resize(true);

    //no option
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    results_.value = resultValue_;

    results_.additionalResults["amcCalculator"] = amcCalculator();

} // McLgmBondEngine::calculate

} // namespace QuantExt
