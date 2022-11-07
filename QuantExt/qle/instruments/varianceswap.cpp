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

#include <qle/instruments/varianceswap.hpp>

namespace QuantExt {

VarianceSwap::VarianceSwap(Position::Type position, Real strike, Real notional, const Date& startDate,
                           const Date& maturityDate, const Calendar& calendar, bool addPastDividends)
    : QuantLib::VarianceSwap(position, strike, notional, startDate, maturityDate), calendar_(calendar),
      addPastDividends_(addPastDividends) {}

void VarianceSwap::setupArguments(PricingEngine::arguments* args) const {
    QuantLib::VarianceSwap::setupArguments(args);
    QuantExt::VarianceSwap::arguments* arguments = dynamic_cast<QuantExt::VarianceSwap::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->calendar = calendar_;
    arguments->addPastDividends = addPastDividends_;
}

} // namespace QuantExt
