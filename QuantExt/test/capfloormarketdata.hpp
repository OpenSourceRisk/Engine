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

/*! \file test/capfloormarketdata.hpp
    \brief structs containing capfloor market data that can be used in tests
*/

#ifndef quantext_test_capfloormarketdata_hpp
#define quantext_test_capfloormarketdata_hpp

#include <boost/make_shared.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/math/matrix.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/period.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
struct CapFloorVolatilityEUR {
    // Constructor
    CapFloorVolatilityEUR()
        : tenors(5), strikes(6), nVols(tenors.size(), strikes.size(), 0.0), slnVols_1(nVols), slnVols_2(nVols),
          shift_1(0.01), shift_2(0.015), atmTenors(6), nAtmVols(6), slnAtmVols_1(6), slnAtmVols_2(6) {

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
        nVols[0][0] = 0.002457;
        nVols[0][1] = 0.002667;
        nVols[0][2] = 0.003953;
        nVols[0][3] = 0.005144;
        nVols[0][4] = 0.007629;
        nVols[0][5] = 0.009938;
        nVols[1][0] = 0.004757;
        nVols[1][1] = 0.004573;
        nVols[1][2] = 0.005362;
        nVols[1][3] = 0.006090;
        nVols[1][4] = 0.007442;
        nVols[1][5] = 0.008805;
        nVols[2][0] = 0.005462;
        nVols[2][1] = 0.005337;
        nVols[2][2] = 0.005826;
        nVols[2][3] = 0.006369;
        nVols[2][4] = 0.007391;
        nVols[2][5] = 0.008431;
        nVols[3][0] = 0.005970;
        nVols[3][1] = 0.005910;
        nVols[3][2] = 0.006144;
        nVols[3][3] = 0.006395;
        nVols[3][4] = 0.007082;
        nVols[3][5] = 0.007767;
        nVols[4][0] = 0.005873;
        nVols[4][1] = 0.005988;
        nVols[4][2] = 0.006102;
        nVols[4][3] = 0.006220;
        nVols[4][4] = 0.006609;
        nVols[4][5] = 0.006973;

        // Populate the shifted lognormal volatility matrix, shift = 1%
        slnVols_1[0][0] = 0.354048;
        slnVols_1[0][1] = 0.275908;
        slnVols_1[0][2] = 0.331131;
        slnVols_1[0][3] = 0.367941;
        slnVols_1[0][4] = 0.431984;
        slnVols_1[0][5] = 0.473055;
        slnVols_1[1][0] = 0.629394;
        slnVols_1[1][1] = 0.429765;
        slnVols_1[1][2] = 0.408771;
        slnVols_1[1][3] = 0.397830;
        slnVols_1[1][4] = 0.387533;
        slnVols_1[1][5] = 0.387566;
        slnVols_1[2][0] = 0.660362;
        slnVols_1[2][1] = 0.459424;
        slnVols_1[2][2] = 0.402383;
        slnVols_1[2][3] = 0.371339;
        slnVols_1[2][4] = 0.334462;
        slnVols_1[2][5] = 0.317835;
        slnVols_1[3][0] = 0.646952;
        slnVols_1[3][1] = 0.457781;
        slnVols_1[3][2] = 0.381007;
        slnVols_1[3][3] = 0.332858;
        slnVols_1[3][4] = 0.285027;
        slnVols_1[3][5] = 0.261010;
        slnVols_1[4][0] = 0.581476;
        slnVols_1[4][1] = 0.420004;
        slnVols_1[4][2] = 0.344897;
        slnVols_1[4][3] = 0.298501;
        slnVols_1[4][4] = 0.250767;
        slnVols_1[4][5] = 0.223903;

        // Populate the shifted lognormal volatility matrix, shift = 1.5%
        slnVols_2[0][0] = 0.204022;
        slnVols_2[0][1] = 0.181767;
        slnVols_2[0][2] = 0.232378;
        slnVols_2[0][3] = 0.268359;
        slnVols_2[0][4] = 0.329940;
        slnVols_2[0][5] = 0.371421;
        slnVols_2[1][0] = 0.368526;
        slnVols_2[1][1] = 0.290783;
        slnVols_2[1][2] = 0.294452;
        slnVols_2[1][3] = 0.297475;
        slnVols_2[1][4] = 0.302807;
        slnVols_2[1][5] = 0.310822;
        slnVols_2[2][0] = 0.395097;
        slnVols_2[2][1] = 0.317747;
        slnVols_2[2][2] = 0.297146;
        slnVols_2[2][3] = 0.285691;
        slnVols_2[2][4] = 0.270621;
        slnVols_2[2][5] = 0.264740;
        slnVols_2[3][0] = 0.396368;
        slnVols_2[3][1] = 0.324204;
        slnVols_2[3][2] = 0.288276;
        slnVols_2[3][3] = 0.262625;
        slnVols_2[3][4] = 0.236265;
        slnVols_2[3][5] = 0.222242;
        slnVols_2[4][0] = 0.357254;
        slnVols_2[4][1] = 0.301862;
        slnVols_2[4][2] = 0.265140;
        slnVols_2[4][3] = 0.238823;
        slnVols_2[4][4] = 0.209753;
        slnVols_2[4][5] = 0.191882;

        // ATM volatility curves (Normal, Shifted Lognormal (1%), Shifted Lognormal (1.5%))
        atmTenors[0] = Period(1, Years);
        nAtmVols[0] = 0.002610;
        slnAtmVols_1[0] = 0.279001;
        slnAtmVols_2[0] = 0.181754;
        atmTenors[1] = Period(2, Years);
        nAtmVols[1] = 0.002972;
        slnAtmVols_1[1] = 0.265115;
        slnAtmVols_2[1] = 0.183202;
        atmTenors[2] = Period(5, Years);
        nAtmVols[2] = 0.005200;
        slnAtmVols_1[2] = 0.453827;
        slnAtmVols_2[2] = 0.314258;
        atmTenors[3] = Period(7, Years);
        nAtmVols[3] = 0.005800;
        slnAtmVols_1[3] = 0.418210;
        slnAtmVols_2[3] = 0.305215;
        atmTenors[4] = Period(10, Years);
        nAtmVols[4] = 0.006300;
        slnAtmVols_1[4] = 0.359696;
        slnAtmVols_2[4] = 0.277812;
        atmTenors[5] = Period(20, Years);
        nAtmVols[5] = 0.006400;
        slnAtmVols_1[5] = 0.295877;
        slnAtmVols_2[5] = 0.238554;
    }

    // Members
    vector<Period> tenors;
    vector<Rate> strikes;
    Matrix nVols;
    Matrix slnVols_1;
    Matrix slnVols_2;
    Real shift_1;
    Real shift_2;
    vector<Period> atmTenors;
    vector<Volatility> nAtmVols;
    vector<Volatility> slnAtmVols_1;
    vector<Volatility> slnAtmVols_2;
};
} // namespace QuantExt

#endif
