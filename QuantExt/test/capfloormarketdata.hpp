/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file test/capfloormarketdata.hpp
    \brief structs containing capfloor market data that can be used in tests
    \ingroup
*/

#ifndef quantext_test_capfloormarketdata_hpp
#define quantext_test_capfloormarketdata_hpp

#include <ql/math/matrix.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/period.hpp>
#include <boost/make_shared.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
struct CapFloorVolatilityEUR {
    // Constructor
    CapFloorVolatilityEUR() : tenors(5), strikes(6), nVols(tenors.size(), strikes.size(), 0.0), 
        atmTenors(6), nAtmVols(6) {

        // Populate capfloor tenors
        tenors[0] = Period(1, Years);
        tenors[1] = Period(5, Years);
        tenors[2] = Period(7, Years);
        tenors[3] = Period(10, Years);
        tenors[4] = Period(20, Years);

        // Populate capfloor strikes
        strikes[0] = -0.005;
        strikes[1] = 0.0;
        strikes[2] = 0.005;
        strikes[3] = 0.01;
        strikes[4] = 0.02;
        strikes[5] = 0.03;

        // Populate the capfloor normal volatility matrix
        nVols[0][0] = 0.002457; nVols[0][1] = 0.002667; nVols[0][2] = 0.003953; nVols[0][3] = 0.005144;
        nVols[0][4] = 0.007629; nVols[0][5] = 0.009938;
        nVols[1][0] = 0.004757; nVols[1][1] = 0.004573; nVols[1][2] = 0.005362; nVols[1][3] = 0.006090;
        nVols[1][4] = 0.007442; nVols[1][5] = 0.008805;
        nVols[2][0] = 0.005462; nVols[2][1] = 0.005337; nVols[2][2] = 0.005826; nVols[2][3] = 0.006369;
        nVols[2][4] = 0.007391; nVols[2][5] = 0.008431;
        nVols[3][0] = 0.005970; nVols[3][1] = 0.005910; nVols[3][2] = 0.006144; nVols[3][3] = 0.006395;
        nVols[3][4] = 0.007082; nVols[3][5] = 0.007767;
        nVols[4][0] = 0.005873; nVols[4][1] = 0.005988; nVols[4][2] = 0.006102; nVols[4][3] = 0.006220;
        nVols[4][4] = 0.006609; nVols[4][5] = 0.006973;

        // Normal ATM volatility curve
        atmTenors[0] = Period(1, Years);  nAtmVols[0] = 0.002610;
        atmTenors[1] = Period(2, Years);  nAtmVols[1] = 0.002972;
        atmTenors[2] = Period(5, Years);  nAtmVols[2] = 0.005200;
        atmTenors[3] = Period(7, Years);  nAtmVols[3] = 0.005800;
        atmTenors[4] = Period(10, Years); nAtmVols[4] = 0.006300;
        atmTenors[5] = Period(20, Years); nAtmVols[5] = 0.006400;

        //// Populate the lognormal volatility matrix
        //lnVols[0][0] = 2.187660; lnVols[0][1] = 1.748360; lnVols[0][2] = 0.834972; lnVols[0][3] = 0.663957;
        //lnVols[1][0] = 0.891725; lnVols[1][1] = 0.642449; lnVols[1][2] = 0.585798; lnVols[1][3] = 0.512169;
        //lnVols[2][0] = 0.549946; lnVols[2][1] = 0.552918; lnVols[2][2] = 0.528572; lnVols[2][3] = 0.476202;
        //lnVols[3][0] = 0.531597; lnVols[3][1] = 0.534495; lnVols[3][2] = 0.526216; lnVols[3][3] = 0.462007;

        //// Populate the first and second set of shifted lognormal volatilities
        //slnVols_1[0][0] = 0.929848; slnVols_1[0][1] = 0.924660; slnVols_1[0][2] = 0.610868; slnVols_1[0][3] = 0.495445;
        //slnVols_1[1][0] = 0.689737; slnVols_1[1][1] = 0.521342; slnVols_1[1][2] = 0.472902; slnVols_1[1][3] = 0.396814;
        //slnVols_1[2][0] = 0.474667; slnVols_1[2][1] = 0.463982; slnVols_1[2][2] = 0.432899; slnVols_1[2][3] = 0.371330;
        //slnVols_1[3][0] = 0.460333; slnVols_1[3][1] = 0.447973; slnVols_1[3][2] = 0.428017; slnVols_1[3][3] = 0.358081;

        //slnVols_2[0][0] = 0.732040; slnVols_2[0][1] = 0.754222; slnVols_2[0][2] = 0.539085; slnVols_2[0][3] = 0.439887;
        //slnVols_2[1][0] = 0.622370; slnVols_2[1][1] = 0.477238; slnVols_2[1][2] = 0.431955; slnVols_2[1][3] = 0.357137;
        //slnVols_2[2][0] = 0.444718; slnVols_2[2][1] = 0.430028; slnVols_2[2][2] = 0.397564; slnVols_2[2][3] = 0.335037;
        //slnVols_2[3][0] = 0.432003; slnVols_2[3][1] = 0.415209; slnVols_2[3][2] = 0.392379; slnVols_2[3][3] = 0.322612;


        //// Populate first and second set of shifts
        //shifts_1[0][0] = 0.002000; shifts_1[0][1] = 0.002500; shifts_1[0][2] = 0.003000; shifts_1[0][3] = 0.004000;
        //shifts_1[1][0] = 0.002000; shifts_1[1][1] = 0.002500; shifts_1[1][2] = 0.003000; shifts_1[1][3] = 0.004000;
        //shifts_1[2][0] = 0.002000; shifts_1[2][1] = 0.002500; shifts_1[2][2] = 0.003000; shifts_1[2][3] = 0.004000;
        //shifts_1[3][0] = 0.002000; shifts_1[3][1] = 0.002500; shifts_1[3][2] = 0.003000; shifts_1[3][3] = 0.004000;

        //shifts_2[0][0] = 0.003000; shifts_2[0][1] = 0.003750; shifts_2[0][2] = 0.004500; shifts_2[0][3] = 0.006000;
        //shifts_2[1][0] = 0.003000; shifts_2[1][1] = 0.003750; shifts_2[1][2] = 0.004500; shifts_2[1][3] = 0.006000;
        //shifts_2[2][0] = 0.003000; shifts_2[2][1] = 0.003750; shifts_2[2][2] = 0.004500; shifts_2[2][3] = 0.006000;
        //shifts_2[3][0] = 0.003000; shifts_2[3][1] = 0.003750; shifts_2[3][2] = 0.004500; shifts_2[3][3] = 0.006000;
    }

    // Members
    vector<Period> tenors;
    vector<Rate> strikes;
    Matrix nVols;
    vector<Period> atmTenors;
    vector<Volatility> nAtmVols;
    /*Matrix lnVols;
    Matrix slnVols_1;
    Matrix slnVols_2;
    Matrix shifts_1;
    Matrix shifts_2;*/
};

}

#endif
