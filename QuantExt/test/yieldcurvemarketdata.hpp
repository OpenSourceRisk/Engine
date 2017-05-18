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

/*! \file test/yieldcurvemarketdata.hpp
    \brief structs containing yield curve market data that can be used in tests
*/

#ifndef quantext_test_yieldcurvemarketdata_hpp
#define quantext_test_yieldcurvemarketdata_hpp

#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <boost/make_shared.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
struct YieldCurveEUR {
    // Constructor
    YieldCurveEUR() : dayCounter(Actual365Fixed()) {
        // Vectors to hold dates and disc
        vector<Date> dates(7);
        vector<Rate> discEonia(7);
        vector<Rate> disc3M(7);
        vector<Rate> disc6M(7);

        // Populate the vectors
        dates[0] = Date(5, Feb, 2016);
        discEonia[0] = 1.000000000;
        disc3M[0] = 1.000000000;
        disc6M[0] = 1.000000000;
        dates[1] = Date(5, Aug, 2016);
        discEonia[1] = 1.001296118;
        disc3M[1] = 1.000482960;
        disc6M[1] = 0.999875649;
        dates[2] = Date(6, Feb, 2017);
        discEonia[2] = 1.003183503;
        disc3M[2] = 1.001536429;
        disc6M[2] = 1.000221239;
        dates[3] = Date(5, Feb, 2021);
        discEonia[3] = 1.008950857;
        disc3M[3] = 0.999534627;
        disc6M[3] = 0.992455644;
        dates[4] = Date(6, Feb, 2023);
        discEonia[4] = 0.996461253;
        disc3M[4] = 0.984474484;
        disc6M[4] = 0.974435009;
        dates[5] = Date(5, Feb, 2026);
        discEonia[5] = 0.960894135;
        disc3M[5] = 0.944011343;
        disc6M[5] = 0.932147253;
        dates[6] = Date(5, Feb, 2036);
        discEonia[6] = 0.830169833;
        disc3M[6] = 0.807585583;
        disc6M[6] = 0.794115491;

        // Create the discount curves
        discountEonia = Handle<YieldTermStructure>(boost::make_shared<DiscountCurve>(dates, discEonia, dayCounter));
        forward3M = Handle<YieldTermStructure>(boost::make_shared<DiscountCurve>(dates, disc3M, dayCounter));
        forward6M = Handle<YieldTermStructure>(boost::make_shared<DiscountCurve>(dates, disc6M, dayCounter));

        // Enable extrapolation on all curves by default
        discountEonia->enableExtrapolation();
        forward3M->enableExtrapolation();
        forward6M->enableExtrapolation();
    }

    // Members
    Handle<YieldTermStructure> discountEonia;
    Handle<YieldTermStructure> forward3M;
    Handle<YieldTermStructure> forward6M;
    DayCounter dayCounter;
};
} // namespace QuantExt

#endif
