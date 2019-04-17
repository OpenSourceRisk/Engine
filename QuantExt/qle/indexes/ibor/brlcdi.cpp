/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
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
    QL_REQUIRE(dcf > 0.0, "Cannot calculate " << name() << " forward rate between " << startDate << " and " << 
        endDate << ": non positive time (" << dcf << ") using " << dayCounter_.name() << " daycounter");
    
    QL_REQUIRE(!termStructure_.empty(), "Cannot calculate " << name() << " forward rate because term structure is empty");

    // For BRL CDI, we want:
    // DI(t, t_s, t_e) =  \left[ \frac{P(t, t_s)}{P(t, t_e)} \right] ^ {\frac{1}{\tau(t_s, t_e)}} - 1
    DiscountFactor discountStart = termStructure_->discount(startDate);
    DiscountFactor discountEnd = termStructure_->discount(endDate);
    return pow(discountStart / discountEnd, 1.0 / dcf) - 1.0;
}

}

