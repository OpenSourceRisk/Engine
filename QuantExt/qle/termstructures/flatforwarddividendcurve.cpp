/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/termstructures/flatforwarddividendcurve.hpp>

namespace QuantExt {

Date FlatForwardDividendCurve::maxDate() const { 
	return dividendCurve_->maxDate(); 
}

DiscountFactor FlatForwardDividendCurve::discountImpl(Time t) const {
    Time maxTime = dividendCurve_->maxTime();
	if (t > maxTime) {
        if (allowsExtrapolation())
            return dividendCurve_->discount(maxTime) / forecastCurve_->discount(maxTime) * forecastCurve_->discount(t);
        else
            return dividendCurve_->discount(maxTime);
	}

	return dividendCurve_->discount(t); 
}

} // QuantExt
