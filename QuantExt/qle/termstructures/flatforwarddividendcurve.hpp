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

/*! \file qle/termstructures/flatforwarddividendcurve.hpp
    \brief Term structure for a forward dividend curve. If extrapolation is set
      we extrapolate with the forecast curve
*/
 
#ifndef quantext_flat_forward_dividend_curve_hpp
#define quantext_flat_forward_dividend_curve_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

class FlatForwardDividendCurve : public YieldTermStructure {
public:
    FlatForwardDividendCurve(const Date& asof, const Handle<YieldTermStructure>& dividendCurve,
                             const Handle<YieldTermStructure>& forecastCurve)
        : YieldTermStructure(asof, dividendCurve->calendar(), dividendCurve->dayCounter()), dividendCurve_(dividendCurve),
          forecastCurve_(forecastCurve) {}

    Date maxDate() const override;
    DiscountFactor discountImpl(Time) const override;

private: 
    Handle<YieldTermStructure> dividendCurve_;
    Handle<YieldTermStructure> forecastCurve_;
};

} // QuantExt

#endif
