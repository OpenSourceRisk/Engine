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

#include <qle/indexes/ibor/brlcdi.hpp>

using namespace QuantLib;
using std::pow;

namespace QuantExt {

Rate BRLCdi::forecastFixing(const Date& fixingDate) const {

    Date startDate = valueDate(fixingDate);
    Date endDate = maturityDate(startDate);
    Time dcf = dayCounter_.yearFraction(startDate, endDate);

    // Some checks
    QL_REQUIRE(dcf > 0.0, "Cannot calculate " << name() << " forward rate between " << startDate << " and " << endDate
                                              << ": non positive time (" << dcf << ") using " << dayCounter_.name()
                                              << " daycounter");

    QL_REQUIRE(!termStructure_.empty(),
               "Cannot calculate " << name() << " forward rate because term structure is empty");

    // For BRL CDI, we want:
    // DI(t, t_s, t_e) =  \left[ \frac{P(t, t_s)}{P(t, t_e)} \right] ^ {\frac{1}{\tau(t_s, t_e)}} - 1
    DiscountFactor discountStart = termStructure_->discount(startDate);
    DiscountFactor discountEnd = termStructure_->discount(endDate);
    return pow(discountStart / discountEnd, 1.0 / dcf) - 1.0;
}

QuantLib::ext::shared_ptr<IborIndex> BRLCdi::clone(const Handle<YieldTermStructure>& h) const {
    return QuantLib::ext::make_shared<BRLCdi>(h);
}

} // namespace QuantExt
