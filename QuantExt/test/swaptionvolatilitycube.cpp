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

#include "swaptionvolatilitycube.hpp"
#include <boost/make_shared.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/math/matrix.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <qle/termstructures/swaptionvolatilitycube.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/indexes/swap/euriborswap.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace testsuite {
/*
    Builds a swaption volatility cube using an atm swaption volatility surface and a cube of vol Spreads (swaptionVolCube2),
    and a swaption volatility cube using a cube of volatility quotes (QuantExt::SwaptionVolatilityCube).

    Checks that they return the same volatilities for pillar points, using extrapolation and when the underlying quotes have been updated

*/
void SwaptionVolatilityCubeTest::testSwaptionVolatilityCube() {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceCurve3...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);

    Date today = Settings::instance().evaluationDate();
    Calendar cal = TARGET();
    BusinessDayConvention bdc = Following;
    std::vector<Period> optionTenors;
    optionTenors.push_back(Period(1,Months));
    optionTenors.push_back(Period(6,Months));
    optionTenors.push_back(Period(1,Years));
    optionTenors.push_back(Period(5,Years));
    optionTenors.push_back(Period(10,Years));
    optionTenors.push_back(Period(30,Years));
    
    std::vector<Period> swapTenors;
    swapTenors.push_back(Period(1,Years));
    swapTenors.push_back(Period(5,Years));
    swapTenors.push_back(Period(10,Years));
    swapTenors.push_back(Period(30,Years));

    std::vector<Real> strikeSpreads;
    strikeSpreads.push_back(-0.020);
    strikeSpreads.push_back(-0.005);
    strikeSpreads.push_back(-0.000);
    strikeSpreads.push_back(+0.005);
    strikeSpreads.push_back(+0.020);
    
    Natural settlementDays = 0;
    DayCounter dc = ActualActual();
    VolatilityType volType = QuantLib::ShiftedLognormal; 
    std::vector<std::vector<Real> > shift(optionTenors.size(), std::vector<Real>(swapTenors.size(), 0.0050));

    
    //building vol quotes matrix
    std::vector<std::vector<std::vector<Handle<Quote> > > > volsHandle(strikeSpreads.size(), std::vector<std::vector<Handle<Quote> > >
                (optionTenors.size(), std::vector<Handle<Quote> >(swapTenors.size(),Handle<Quote>())));
    
    std::vector<std::vector<std::vector<boost::shared_ptr<SimpleQuote> > > > volsQuote(strikeSpreads.size(), 
                std::vector<std::vector<boost::shared_ptr<SimpleQuote> > > (optionTenors.size(), std::vector<boost::shared_ptr<SimpleQuote> >
                (swapTenors.size(),boost::shared_ptr<SimpleQuote>())));

    std::vector<std::vector<std::vector<Real> > > vols(strikeSpreads.size(), std::vector<std::vector<Real> >
                (optionTenors.size(), std::vector<Real>(swapTenors.size(),0)));

            //strikeSpread -0.02
            vols[0][0][0]=0.1320; vols[0][0][1]=0.1580; vols[0][0][2]=0.1410; vols[0][0][3]=0.1240;
            vols[0][1][0]=0.1460; vols[0][1][1]=0.1600; vols[0][1][2]=0.1480; vols[0][1][3]=0.1280;
            vols[0][2][0]=0.1620; vols[0][2][1]=0.1610; vols[0][2][2]=0.1490; vols[0][2][3]=0.1310;
            vols[0][3][0]=0.1660; vols[0][3][1]=0.1490; vols[0][3][2]=0.1390; vols[0][3][3]=0.1240;
            vols[0][4][0]=0.1420; vols[0][4][1]=0.1320; vols[0][4][2]=0.1270; vols[0][4][3]=0.1120;
            vols[0][5][0]=0.1150; vols[0][5][1]=0.1110; vols[0][5][2]=0.1090; vols[0][5][3]=0.0950;

            //strikeSpread -0.005
            vols[1][0][0]=0.1310; vols[1][0][1]=0.1570; vols[1][0][2]=0.1400; vols[1][0][3]=0.1230;
            vols[1][1][0]=0.1450; vols[1][1][1]=0.1590; vols[1][1][2]=0.1470; vols[1][1][3]=0.1270;
            vols[1][2][0]=0.1610; vols[1][2][1]=0.1600; vols[1][2][2]=0.1480; vols[1][2][3]=0.1300;
            vols[1][3][0]=0.1650; vols[1][3][1]=0.1480; vols[1][3][2]=0.1380; vols[1][3][3]=0.1230;
            vols[1][4][0]=0.1410; vols[1][4][1]=0.1310; vols[1][4][2]=0.1260; vols[1][4][3]=0.1110;
            vols[1][5][0]=0.1140; vols[1][5][1]=0.1100; vols[1][5][2]=0.1080; vols[1][5][3]=0.0940;
            
            //this is the ATM level
            vols[2][0][0]=0.1300; vols[2][0][1]=0.1560; vols[2][0][2]=0.1390; vols[2][0][3]=0.1220;
            vols[2][1][0]=0.1440; vols[2][1][1]=0.1580; vols[2][1][2]=0.1460; vols[2][1][3]=0.1260;
            vols[2][2][0]=0.1600; vols[2][2][1]=0.1590; vols[2][2][2]=0.1470; vols[2][2][3]=0.1290;
            vols[2][3][0]=0.1640; vols[2][3][1]=0.1470; vols[2][3][2]=0.1370; vols[2][3][3]=0.1220;
            vols[2][4][0]=0.1400; vols[2][4][1]=0.1300; vols[2][4][2]=0.1250; vols[2][4][3]=0.1100;
            vols[2][5][0]=0.1130; vols[2][5][1]=0.1090; vols[2][5][2]=0.1070; vols[2][5][3]=0.0930;

            //strikeSpread 0.005
            vols[3][0][0]=0.1290; vols[3][0][1]=0.1550; vols[3][0][2]=0.1380; vols[3][0][3]=0.1210;
            vols[3][1][0]=0.1430; vols[3][1][1]=0.1570; vols[3][1][2]=0.1450; vols[3][1][3]=0.1250;
            vols[3][2][0]=0.1590; vols[3][2][1]=0.1580; vols[3][2][2]=0.1460; vols[3][2][3]=0.1280;
            vols[3][3][0]=0.1630; vols[3][3][1]=0.1460; vols[3][3][2]=0.1360; vols[3][3][3]=0.1210;
            vols[3][4][0]=0.1390; vols[3][4][1]=0.1290; vols[3][4][2]=0.1240; vols[3][4][3]=0.1090;
            vols[3][5][0]=0.1120; vols[3][5][1]=0.1080; vols[3][5][2]=0.1060; vols[3][5][3]=0.0920;

            
            //strikeSpread 0.02
            vols[4][0][0]=0.1280; vols[4][0][1]=0.1540; vols[4][0][2]=0.1370; vols[4][0][3]=0.1200;
            vols[4][1][0]=0.1420; vols[4][1][1]=0.1560; vols[4][1][2]=0.1440; vols[4][1][3]=0.1240;
            vols[4][2][0]=0.1580; vols[4][2][1]=0.1570; vols[4][2][2]=0.1450; vols[4][2][3]=0.1270;
            vols[4][3][0]=0.1620; vols[4][3][1]=0.1450; vols[4][3][2]=0.1350; vols[4][3][3]=0.1200;
            vols[4][4][0]=0.1380; vols[4][4][1]=0.1280; vols[4][4][2]=0.1230; vols[4][4][3]=0.1080;
            vols[4][5][0]=0.1110; vols[4][5][1]=0.1070; vols[4][5][2]=0.1050; vols[4][5][3]=0.0910;

    for (Size i = 0; i < strikeSpreads.size(); i++) {
        for (Size j = 0; j < optionTenors.size(); j++) {
            for (Size k = 0; k < swapTenors.size(); k++) {
                volsQuote[i][j][k] = boost::shared_ptr<SimpleQuote>( new SimpleQuote(vols[i][j][k]));
                volsHandle[i][j][k] = Handle<Quote>(volsQuote[i][j][k]);
            }
        }
    }

    //storing the vol spreads
    std::vector<std::vector<Handle<Quote> > > volSpreadsHandle( optionTenors.size()*swapTenors.size(), 
            std::vector<Handle<Quote> >(strikeSpreads.size(), Handle<Quote>()));

    for (Size i = 0; i < strikeSpreads.size(); i++) {
        for (Size j = 0; j < optionTenors.size(); j++) {
            for (Size k = 0; k < swapTenors.size(); k++) {
                volSpreadsHandle[j*swapTenors.size()+k][i] = Handle<Quote>(boost::shared_ptr<Quote>(new
                        SimpleQuote(vols[i][j][k]-vols[2][j][k])));
            }
        }
    }
    
    //building swapVolCube2
    boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(today, 0.05, ActualActual()));
    Handle<YieldTermStructure> termStructure(yts);
    boost::shared_ptr<SwapIndex> swapIndexBase = boost::shared_ptr<SwapIndex>(new
            EuriborSwapIsdaFixA(2*Years, termStructure));
    boost::shared_ptr<SwapIndex> shortSwapIndexBase = boost::shared_ptr<SwapIndex>(new
            EuriborSwapIsdaFixA(1*Years, termStructure));
    
    boost::shared_ptr<SwaptionVolatilityStructure> atm = 
            boost::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
                    cal, bdc, optionTenors, swapTenors, volsHandle[2], dc,
                false, QuantLib::ShiftedLognormal, shift));

    Handle<SwaptionVolatilityStructure> hATM(atm);
    
    boost::shared_ptr<QuantExt::SwaptionVolCube2> cubeATM = boost::make_shared<QuantExt::SwaptionVolCube2>(
                hATM, optionTenors, swapTenors, strikeSpreads, volSpreadsHandle, swapIndexBase, shortSwapIndexBase,
                false, false);
    cubeATM->enableExtrapolation();

    //building swaptionvolatilitycube
    boost::shared_ptr<QuantExt::SwaptionVolatilityCube> cubeFull( new QuantExt::SwaptionVolatilityCube(                                          
                optionTenors, swapTenors, strikeSpreads, volsHandle, swapIndexBase, 
                shortSwapIndexBase, false, volType, bdc, dc, cal, 0, shift));
    cubeFull->enableExtrapolation();



    BOOST_TEST_MESSAGE("Check that bvcTest returns the expected values");
    // Check that the cubes return the expected values
    for (Size i = 0; i < strikeSpreads.size(); ++i) {
        for (Size j = 0; j < optionTenors.size(); ++j) {
            for (Size k = 0; k < swapTenors.size(); ++k) {
                Real atmStrike = cubeATM->atmStrike(optionTenors[j], swapTenors[k]);
                BOOST_CHECK_CLOSE(atmStrike, cubeFull->atmStrike(optionTenors[j], swapTenors[k]), 1e-12);
                BOOST_CHECK_CLOSE(cubeATM->volatility(optionTenors[j], swapTenors[k], atmStrike+strikeSpreads[i]), vols[i][j][k], 1e-12);
                BOOST_CHECK_CLOSE(cubeFull->volatility(optionTenors[j], swapTenors[k], atmStrike+strikeSpreads[i]), vols[i][j][k], 1e-12);
            }
        }
    }

    // Now check that they give the same vols (including extrapolation)
    for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
        for (Size j = 0; j < optionTenors.size(); ++j) {
            for (Size k = 0; k < swapTenors.size(); ++k) {
                BOOST_TEST_MESSAGE(strike<<" "<<j<<" "<<k);
                BOOST_CHECK_CLOSE(cubeATM->volatility(optionTenors[j], swapTenors[k], strike), 
                    cubeFull->volatility(optionTenors[j], swapTenors[k], strike), 1e-12);
            }
        }
    }

    // Now update the underlying quotes
    for (Size i = 0; i < strikeSpreads.size(); i++) {
        for (Size j = 0; j < optionTenors.size(); j++) {
            for (Size k = 0; k < swapTenors.size(); k++) {
                volsQuote[i][j][k]->setValue( volsQuote[i][j][k]->value() + 0.01);
            }
        }
    }
    // and check again
    for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
        for (Size j = 0; j < optionTenors.size(); ++j) {
            for (Size k = 0; k < swapTenors.size(); ++k) {
                BOOST_TEST_MESSAGE(strike<<" "<<j<<" "<<k);
                BOOST_CHECK_CLOSE(cubeATM->volatility(optionTenors[j], swapTenors[k], strike), 
                    cubeFull->volatility(optionTenors[j], swapTenors[k], strike), 1e-12);
            }
        }
    }
}

test_suite* SwaptionVolatilityCubeTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("SwaptionVolatilityCubeTest");
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityCubeTest::testSwaptionVolatilityCube));
    return suite;
}
} // namespace testsuite
