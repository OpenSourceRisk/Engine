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

/*! \file test/swaptionmarketdata.hpp
    \brief structs containing swaption market data that can be used in tests
*/

#ifndef quantext_test_swaptionmarketdata_hpp
#define quantext_test_swaptionmarketdata_hpp

#include <boost/make_shared.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/period.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
struct SwaptionVolatilityEUR {
    // Constructor
    SwaptionVolatilityEUR()
        : optionTenors(4), swapTenors(4), strikeSpreads(5), nVols(optionTenors.size(), swapTenors.size(), 0.0),
          lnVols(nVols), slnVols_1(nVols), slnVols_2(nVols), shifts_1(nVols), shifts_2(nVols),
          nVolSpreads(16, vector<Handle<Quote> >(5, Handle<Quote>())),
          lnVolSpreads(16, vector<Handle<Quote> >(5, Handle<Quote>())),
          slnVolSpreads(16, vector<Handle<Quote> >(5, Handle<Quote>())) {

        // Populate option tenors
        optionTenors[0] = Period(1, Years);
        optionTenors[1] = Period(5, Years);
        optionTenors[2] = Period(7, Years);
        optionTenors[3] = Period(10, Years);

        // Populate swap tenors
        swapTenors[0] = Period(1, Years);
        swapTenors[1] = Period(5, Years);
        swapTenors[2] = Period(10, Years);
        swapTenors[3] = Period(20, Years);

        // Populate the normal volatility matrix
        nVols[0][0] = 0.003543;
        nVols[0][1] = 0.005270;
        nVols[0][2] = 0.006978;
        nVols[0][3] = 0.007918;
        nVols[1][0] = 0.007013;
        nVols[1][1] = 0.007443;
        nVols[1][2] = 0.007820;
        nVols[1][3] = 0.007363;
        nVols[2][0] = 0.007519;
        nVols[2][1] = 0.007807;
        nVols[2][2] = 0.007698;
        nVols[2][3] = 0.007117;
        nVols[3][0] = 0.007668;
        nVols[3][1] = 0.007705;
        nVols[3][2] = 0.007611;
        nVols[3][3] = 0.006848;

        // Populate the lognormal volatility matrix
        lnVols[0][0] = 2.187660;
        lnVols[0][1] = 1.748360;
        lnVols[0][2] = 0.834972;
        lnVols[0][3] = 0.663957;
        lnVols[1][0] = 0.891725;
        lnVols[1][1] = 0.642449;
        lnVols[1][2] = 0.585798;
        lnVols[1][3] = 0.512169;
        lnVols[2][0] = 0.549946;
        lnVols[2][1] = 0.552918;
        lnVols[2][2] = 0.528572;
        lnVols[2][3] = 0.476202;
        lnVols[3][0] = 0.531597;
        lnVols[3][1] = 0.534495;
        lnVols[3][2] = 0.526216;
        lnVols[3][3] = 0.462007;

        // Populate the first and second set of shifted lognormal volatilities
        slnVols_1[0][0] = 0.929848;
        slnVols_1[0][1] = 0.924660;
        slnVols_1[0][2] = 0.610868;
        slnVols_1[0][3] = 0.495445;
        slnVols_1[1][0] = 0.689737;
        slnVols_1[1][1] = 0.521342;
        slnVols_1[1][2] = 0.472902;
        slnVols_1[1][3] = 0.396814;
        slnVols_1[2][0] = 0.474667;
        slnVols_1[2][1] = 0.463982;
        slnVols_1[2][2] = 0.432899;
        slnVols_1[2][3] = 0.371330;
        slnVols_1[3][0] = 0.460333;
        slnVols_1[3][1] = 0.447973;
        slnVols_1[3][2] = 0.428017;
        slnVols_1[3][3] = 0.358081;

        slnVols_2[0][0] = 0.732040;
        slnVols_2[0][1] = 0.754222;
        slnVols_2[0][2] = 0.539085;
        slnVols_2[0][3] = 0.439887;
        slnVols_2[1][0] = 0.622370;
        slnVols_2[1][1] = 0.477238;
        slnVols_2[1][2] = 0.431955;
        slnVols_2[1][3] = 0.357137;
        slnVols_2[2][0] = 0.444718;
        slnVols_2[2][1] = 0.430028;
        slnVols_2[2][2] = 0.397564;
        slnVols_2[2][3] = 0.335037;
        slnVols_2[3][0] = 0.432003;
        slnVols_2[3][1] = 0.415209;
        slnVols_2[3][2] = 0.392379;
        slnVols_2[3][3] = 0.322612;

        // Populate first and second set of shifts
        shifts_1[0][0] = 0.002000;
        shifts_1[0][1] = 0.002500;
        shifts_1[0][2] = 0.003000;
        shifts_1[0][3] = 0.004000;
        shifts_1[1][0] = 0.002000;
        shifts_1[1][1] = 0.002500;
        shifts_1[1][2] = 0.003000;
        shifts_1[1][3] = 0.004000;
        shifts_1[2][0] = 0.002000;
        shifts_1[2][1] = 0.002500;
        shifts_1[2][2] = 0.003000;
        shifts_1[2][3] = 0.004000;
        shifts_1[3][0] = 0.002000;
        shifts_1[3][1] = 0.002500;
        shifts_1[3][2] = 0.003000;
        shifts_1[3][3] = 0.004000;

        shifts_2[0][0] = 0.003000;
        shifts_2[0][1] = 0.003750;
        shifts_2[0][2] = 0.004500;
        shifts_2[0][3] = 0.006000;
        shifts_2[1][0] = 0.003000;
        shifts_2[1][1] = 0.003750;
        shifts_2[1][2] = 0.004500;
        shifts_2[1][3] = 0.006000;
        shifts_2[2][0] = 0.003000;
        shifts_2[2][1] = 0.003750;
        shifts_2[2][2] = 0.004500;
        shifts_2[2][3] = 0.006000;
        shifts_2[3][0] = 0.003000;
        shifts_2[3][1] = 0.003750;
        shifts_2[3][2] = 0.004500;
        shifts_2[3][3] = 0.006000;

        strikeSpreads[0] = -0.02;
        strikeSpreads[1] = -0.01;
        strikeSpreads[2] = 0.00;
        strikeSpreads[3] = +0.01;
        strikeSpreads[4] = +0.02;
        Size atmStrikeIndex = 2;

        // random smile
        MersenneTwisterUniformRng rng(42);
        Real nVolSpreadMin = 0.0010;
        Real nVolSpreadMax = 0.0050;
        Real lnVolSpreadMin = 0.1;
        Real lnVolSpreadMax = 0.3;
        Real slnVolSpreadMin = 0.05;
        Real slnVolSpreadMax = 0.25;
        for (Size i = 0; i < optionTenors.size(); ++i) {
            for (Size j = 0; j < swapTenors.size(); ++j) {
                for (Size k = 0; k < strikeSpreads.size(); ++k) {
                    Size index = j * optionTenors.size() + i;
                    Real nVolSpread =
                        k == atmStrikeIndex ? 0.0 : nVolSpreadMin + rng.nextReal() * (nVolSpreadMax - nVolSpreadMin);
                    nVolSpreads[index][k] = Handle<Quote>(boost::shared_ptr<SimpleQuote>(new SimpleQuote(nVolSpread)));
                    Real lnVolSpread =
                        k == atmStrikeIndex ? 0.0 : lnVolSpreadMin + rng.nextReal() * (lnVolSpreadMax - lnVolSpreadMin);
                    lnVolSpreads[index][k] =
                        Handle<Quote>(boost::shared_ptr<SimpleQuote>(new SimpleQuote(lnVolSpread)));
                    Real slnVolSpread = k == atmStrikeIndex
                                            ? 0.0
                                            : slnVolSpreadMin + rng.nextReal() * (slnVolSpreadMax - slnVolSpreadMin);
                    slnVolSpreads[index][k] =
                        Handle<Quote>(boost::shared_ptr<SimpleQuote>(new SimpleQuote(slnVolSpread)));
                }
            }
        }
    }

    // Members
    vector<Period> optionTenors;
    vector<Period> swapTenors;
    vector<Real> strikeSpreads;
    Matrix nVols;
    Matrix lnVols;
    Matrix slnVols_1;
    Matrix slnVols_2;
    Matrix shifts_1;
    Matrix shifts_2;

    vector<vector<Handle<Quote> > > nVolSpreads, lnVolSpreads, slnVolSpreads;
};

struct SwaptionConventionsEUR {
    // Constructor
    SwaptionConventionsEUR()
        : settlementDays(2), fixedTenor(Period(1, Years)), fixedCalendar(TARGET()), fixedConvention(ModifiedFollowing),
          fixedDayCounter(Thirty360(Thirty360::BondBasis)), floatIndex(boost::make_shared<Euribor6M>()),
          swapIndex(boost::make_shared<EuriborSwapIsdaFixA>(10 * Years)),
          shortSwapIndex(boost::make_shared<EuriborSwapIsdaFixA>(2 * Years)) {}

    // Members
    Natural settlementDays;
    Period fixedTenor;
    Calendar fixedCalendar;
    BusinessDayConvention fixedConvention;
    DayCounter fixedDayCounter;
    boost::shared_ptr<IborIndex> floatIndex;
    boost::shared_ptr<SwapIndex> swapIndex, shortSwapIndex;
};
} // namespace QuantExt

#endif
