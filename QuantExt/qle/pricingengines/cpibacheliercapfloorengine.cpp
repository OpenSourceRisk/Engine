/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*!
 \file cpibacheliercapfloorengines.cpp
 \brief Engines for CPI options
 \ingroup PricingEngines
 */

#include <ql/time/daycounters/actualactual.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <qle/pricingengines/cpibacheliercapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;

namespace QuantExt {

double CPIBachelierCapFloorEngine::optionPriceImpl(QuantLib::Option::Type type, double strike, double forward,
                                               double stdDev, double discount) const {
    return bachelierBlackFormula(type, strike, forward, stdDev, discount);
}

} // namespace QuantExt
