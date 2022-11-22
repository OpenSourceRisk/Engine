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

/*! \file qle/pricingengines/volatilityfromvarianceswapengine.cpp
    \brief volatility swap engine
    \ingroup engines
*/

#include <qle/pricingengines/volatilityfromvarianceswapengine.hpp>

namespace QuantExt {

void VolatilityFromVarianceSwapEngine::calculate() const {

    GeneralisedReplicatingVarianceSwapEngine::calculate();

    const DiscountFactor df = discountingTS_->discount(arguments_.maturityDate);
    const Real multiplier = arguments_.position == Position::Long ? 1.0 : -1.0;
    const Real volatility = sqrt(boost::any_cast<Real>(results_.additionalResults.at("totalVariance")));
    const Real volatilityStrike = sqrt(arguments_.strike);

    results_.value = multiplier * df * arguments_.notional * 100.0 * (volatility - volatilityStrike);
}

} // namespace QuantExt
