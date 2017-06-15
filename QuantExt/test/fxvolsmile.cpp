/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file fxvolsmile.cpp
    \brief fx vol smile
*/

#include "fxvolsmile.hpp"
#include "utilities.hpp"

#include <iostream>

#include <qle/termstructures/blackvariancesurface2.hpp>
#include <qle/termstructures/blackvariancecurve2.hpp>
#include <qle/termstructures/blackstickydeltasurface.hpp>
#include <qle/termstructures/fxvannavolgasmilesection.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <qle/termstructures/blackinvertedvoltermstructure.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {

    struct CommonVars {

        /* ------ GLOBAL VARIABLES ------ */
        Date today;
        DayCounter dc;
        vector<Date> dates;
        vector<Real> strikes;
        Matrix vols;
        vector<Real> atmVols;

        vector<Volatility> rrs;
        vector<Volatility> bfs;

        Handle<Quote> baseSpot;
        Handle<Quote> shiftedSpot;
        Handle<YieldTermStructure> baseDomesticYield;
        Handle<YieldTermStructure> baseForeignYield;
        Handle<YieldTermStructure> shiftedDomesticYield;
        Handle<YieldTermStructure> shiftedForeignYield;

        CommonVars() {

            today = Date(1, Jan, 2014);
            dc = ActualActual();

            Settings::instance().evaluationDate() = today;

            dates.push_back(Date(1, Feb, 2014));
            dates.push_back(Date(1, Mar, 2014));
            dates.push_back(Date(1, Apr, 2014));
            dates.push_back(Date(1, Jan, 2015));

            strikes.push_back(90);
            strikes.push_back(100);
            strikes.push_back(110);

            vols = Matrix(3, 4);
            vols[0][0] = 0.12; vols[1][0] = 0.10; vols[2][0] = 0.13;
            vols[0][1] = 0.22; vols[1][1] = 0.20; vols[2][1] = 0.23;
            vols[0][2] = 0.32; vols[1][2] = 0.30; vols[2][2] = 0.33;
            vols[0][3] = 0.42; vols[1][3] = 0.40; vols[2][3] = 0.43;

            atmVols.push_back(0.1);
            atmVols.push_back(0.2);
            atmVols.push_back(0.3);
            atmVols.push_back(0.4);

            rrs = vector<Volatility>(atmVols.size(), 0.01);
            bfs = vector<Volatility>(atmVols.size(), 0.001);

            baseSpot = Handle<Quote> (boost::shared_ptr<Quote> (new SimpleQuote (100)));
            shiftedSpot = Handle<Quote> (boost::shared_ptr<Quote> (new SimpleQuote (110)));

            baseDomesticYield    = Handle<YieldTermStructure> (QuantExt::flatRate(today, 0.03, dc));
            shiftedDomesticYield = Handle<YieldTermStructure> (QuantExt::flatRate(today, 0.032, dc));
            baseForeignYield     = Handle<YieldTermStructure> (QuantExt::flatRate(today, 0.01, dc));
            shiftedForeignYield  = Handle<YieldTermStructure> (QuantExt::flatRate(today, 0.011, dc));
        }
    };

    Real fwdDelta (bool isCall, Time t, Handle<YieldTermStructure> domYTS, Handle<YieldTermStructure> forYTS,
                    Real spot, Real strike, Real sigma) {

        Real r_d = domYTS->zeroRate(t, Continuous);
        Real r_f = forYTS->zeroRate(t, Continuous);
        Real fwd = spot * exp((r_d - r_f)*t);
        Real tmp = sigma * ::sqrt(t);
        Real d_plus = (::log(fwd/strike) + 0.5*tmp*tmp)/tmp;
        Real phi = isCall ? 1 : -1;
        CumulativeNormalDistribution cnd;
        return phi * cnd(d_plus);
    }

}

void FxVolSmileTest::testSurface2() {

    BOOST_MESSAGE("Testing fx black vol surface 2");

    SavedSettings backup;

    CommonVars vars;

    BlackVarianceSurface2 surface(vars.dates, vars.strikes, vars.vols, vars.dc);

    Time t = vars.dc.yearFraction(vars.today, Date(1,Apr,2014));

    if (surface.blackVol(Date(1, Apr, 2014), 90) != vars.vols[0][2])
        BOOST_FAIL("Invalid vol returned by date");
    if (surface.blackVol(Date(1, Mar, 2014), 110) != vars.vols[2][1])
        BOOST_FAIL("Invalid vol returned by date");
    if (surface.blackVol(t, 90) != vars.vols[0][2])
        BOOST_FAIL("Invalid vol returned by time");

    // Now bump the eval date by 2 years
    Settings::instance().evaluationDate() = Date(1, Jan, 2016);
    if (surface.blackVol(t, 90) != vars.vols[0][2])
        BOOST_FAIL("Invalid vol returned by time (when shifted)");

}

void FxVolSmileTest::testStickyDeltaSurfaceWithCurve() {

    BOOST_MESSAGE("Testing fx sticky delta surface with underlying ATM curve");

    SavedSettings backup;

    CommonVars vars;

    Handle<BlackVolTermStructure> base (boost::shared_ptr<BlackVolTermStructure> (
        new BlackVarianceCurve2 (vars.dates, vars.atmVols, vars.dc)));

    Real vol_6m_50  = base->blackVol(0.5, 50);
    Real vol_6m_100 = base->blackVol(0.5, 100);
    if (vol_6m_50 != vol_6m_100)
        BOOST_FAIL("ATM Vol Curve is exhibiting a smile");

    BlackStickyDeltaSurface bsd (base,
        vars.baseSpot, vars.baseDomesticYield, vars.baseForeignYield,
        vars.shiftedSpot, vars.shiftedDomesticYield, vars.shiftedForeignYield);

    // Now ask the sticky delta surface for vols at the same expiry
    if (bsd.blackVol(0.5, 50) != vol_6m_50)
        BOOST_FAIL("BlackStickyDeltaSurface should return the same vol with an ATM Curve");
    if (bsd.blackVol(0.2, 100) != base->blackVol(0.2, 80))
        BOOST_FAIL("BlackStickyDeltaSurface should return the same vol with an ATM Curve");
}

void FxVolSmileTest::testStickyDeltaSurfaceWithSurface() {

    BOOST_MESSAGE("Testing fx sticky delta surface with underlying VOL surface");

    SavedSettings backup;

    CommonVars vars;

    Handle<BlackVolTermStructure> base (boost::shared_ptr<BlackVolTermStructure> (
        new BlackVarianceSurface2 (vars.dates, vars.strikes, vars.vols, vars.dc)));

    BlackStickyDeltaSurface bsd (base,
        vars.baseSpot, vars.baseDomesticYield, vars.baseForeignYield,
        vars.shiftedSpot, vars.shiftedDomesticYield, vars.shiftedForeignYield);

    // Get the base Vol for time = 0.4 and strike = 105
    Real t = 0.4;
    Real baseStrike = 109;
    Real baseVol = base->blackVol(t, baseStrike);

    // Calculate delta.
    Real baseDelta = fwdDelta (true, t, vars.baseDomesticYield, vars.baseForeignYield,
                               vars.baseSpot->value(), baseStrike, baseVol);

    //cout << " S " << vars.baseSpot->value() << " K " << baseStrike << " Vol "
    //     << baseVol << " D " << baseDelta << endl;

    // Shifted strike
    Real baseFwd = vars.baseSpot->value() * vars.baseForeignYield->discount(t)
                                          / vars.baseDomesticYield->discount(t);
    Real shiftedFwd = vars.shiftedSpot->value() * vars.shiftedForeignYield->discount(t)
                                          / vars.shiftedDomesticYield->discount(t);

    Real shiftedStrike = baseStrike * shiftedFwd / baseFwd;
    Real shiftedVol = bsd.blackVol(t, shiftedStrike);

    // Calculate delta.
    Real shiftedDelta = fwdDelta (true, t, vars.shiftedDomesticYield, vars.shiftedForeignYield,
                                    vars.shiftedSpot->value(), shiftedStrike, shiftedVol);

    //cout << " S " << vars.shiftedSpot->value() << " K " << shiftedStrike << " Vol "
    //     << shiftedVol << " D " << shiftedDelta << endl;

    if (fabs(baseDelta - shiftedDelta) > 0.00001)
        BOOST_FAIL("BlackStickyDeltaSurface failed to preserve delta.");

}

void FxVolSmileTest::testVannaVolgaFxSmileSection() {

    BOOST_MESSAGE("Testing fx vanna volga smile");

    SavedSettings backup;

    // test numbers from Castagna & Mercurio (2006)
    // http://papers.ssrn.com/sol3/papers.cfm?abstract_id=873788
    // page 5
    Settings::instance().evaluationDate() = Date(1, July, 2005);
    Time t = 94 / (double)365;
    Real S0 = 1.205;
    Volatility sig_atm = 0.0905;
    Volatility sig_rr = -0.005;
    Volatility sig_bf = 0.0013;
    // page 11
    DiscountFactor df_usd = 0.9902752;
    DiscountFactor df_eur = 0.9945049;

    // Rates
    Real rd = -::log(df_usd) / t;
    Real rf = -::log(df_eur) / t;

    VannaVolgaSmileSection vvss (S0, rd, rf, t, sig_atm, sig_rr, sig_bf);

    // Check the Strike and Vol values from the paper
    Real tolerance = 0.0001; // 4 decimal places
    if (fabs(vvss.k_atm() - 1.2114) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculte ATM strike, got " << vvss.k_atm());
    if (fabs(vvss.k_25p() - 1.1733) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculate 25P strike, got " << vvss.k_25p());
    if (fabs(vvss.k_25c() - 1.2487) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculate 25C strike, got " << vvss.k_25c());
    if (fabs(vvss.vol_atm() - 0.0905) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculate ATM vol, got " << vvss.vol_atm());
    if (fabs(vvss.vol_25p() - 0.0943) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculate 25P vol, got " << vvss.vol_25p());
    if (fabs(vvss.vol_25c() - 0.0893) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to calculate 25C vol, got " << vvss.vol_25c());

    // Now check that our smile returns these
    if (fabs(vvss.volatility(vvss.k_atm()) - vvss.vol_atm()) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to recover ATM vol, got " << vvss.volatility(vvss.k_atm()));
    if (fabs(vvss.volatility(vvss.k_25p()) - vvss.vol_25p()) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to recover 25P vol, got " << vvss.volatility(vvss.k_25p()));
    if (fabs(vvss.volatility(vvss.k_25c()) - vvss.vol_25c()) > tolerance)
        BOOST_FAIL ("VannaVolgaSmileSection failed to recover 25C vol, got " << vvss.volatility(vvss.k_25c()));

    // To graph the smile, uncomment this code
    /*
    cout << "strike,vol" << endl;
    //for (Real k = 1.1; k < 1.35; k += 0.002) // normal (as per paper)
    //for (Real k = 0.9; k < 1.5; k += 0.01) // large
    for (Real k = 0.1; k < 3; k += 0.05) // extreme
        cout << k << "," << vvss.volatility(k) << endl;
    */
}

struct VolData {
  const char* tenor;
  Volatility atm;
  Volatility rr;
  Volatility bf;
  Time time;
  Real df_d;
  Real df_f;
};

void FxVolSmileTest::testVannaVolgaFxVolSurface() {

    BOOST_MESSAGE("Testing fx vanna volga surface");

    SavedSettings backup;

    // Data from
    // "Consistent pricing and hedging of an FX options book" (2005)
    // L. Bisesti, A. Castagna and F. Mercurio
    // http://www.fabiomercurio.it/fxbook.pdf
    Date asof (12, Feb, 2004);
    Settings::instance().evaluationDate() = asof;

    Handle<Quote> fxSpot (boost::shared_ptr<Quote> (new SimpleQuote(1.2832)));

    // vols are % here
    // tenor, atm, rr, bf, T, p_d, p_f
    VolData volData[] = {
        {"1W", 11.75, 0.50, 0.190, 0.0192, 0.999804, 0.999606},
        {"2W", 11.60, 0.50, 0.190, 0.0384, 0.999595, 0.999208},
        {"1M", 11.50, 0.60, 0.190, 0.0877, 0.999044, 0.998179},
        {"2M", 11.25, 0.60, 0.210, 0.1726, 0.998083, 0.996404},
        {"3M", 11.00, 0.60, 0.220, 0.2493, 0.997187, 0.994803},
        {"6M", 10.87, 0.65, 0.235, 0.5014, 0.993959, 0.989548},
        {"9M", 10.83, 0.69, 0.235, 0.7589, 0.990101, 0.984040},
        {"1Y", 10.80, 0.70, 0.240, 1.0110, 0.985469, 0.978479},
        {"2Y", 10.70, 0.65, 0.255, 2.0110, 0.960102, 0.951092}
    };

    // Assume act/act
    DayCounter dc = ActualActual();

    // set up vectors
    Size len = sizeof(volData) / sizeof(volData[0]);
    vector<Date> dates(len);
    vector<Volatility> atm(len);
    vector<Volatility> rr(len);
    vector<Volatility> bf(len);
    // For DiscountCurve we need the T=0 points.
    vector<Date> discountDates(len+1);
    vector<DiscountFactor> dfDom(len+1);
    vector<DiscountFactor> dfFor(len+1);
    discountDates[0] = asof;
    dfDom[0] = 1.0;
    dfFor[0] = 1.0;

    for (Size i=0; i<sizeof(volData) / sizeof(volData[0]); i++) {
        dates[i] = asof + PeriodParser::parse(volData[i].tenor);
        // check time == volData[i].time
        /*
        if (fabs(dc.yearFraction(asof, dates[i]) - volData[i].time) > 0.001)
            BOOST_FAIL("Did not match vol data time (" << volData[i].time <<
                       ") with aosf " << asof << " and maturity " << dates[i] <<
                       " got year fraction of " << dc.yearFraction(asof, dates[i]));
         */

        atm[i] = volData[i].atm / 100;
        rr[i] = volData[i].rr / 100;
        bf[i] = volData[i].bf / 100;
        
        discountDates[i+1] = dates[i];
        dfDom[i+1] = volData[i].df_d;
        dfFor[i+1] = volData[i].df_f;
    }

    // Now build discount curves
    Handle<YieldTermStructure> domYTS (boost::shared_ptr<YieldTermStructure>
      (new DiscountCurve (discountDates, dfDom, dc)));
    Handle<YieldTermStructure> forYTS (boost::shared_ptr<YieldTermStructure>
      (new DiscountCurve (discountDates, dfFor, dc)));

    // build surface
    FxBlackVannaVolgaVolatilitySurface volSurface (dates, atm, rr, bf, dc, fxSpot, domYTS, forYTS);

    //1.55,1.75,0.121507
    Real vol = volSurface.blackVol(1.75, 1.55);
    Real expected = 0.121507;
    if (fabs(vol - expected) > 0.00001)
        BOOST_FAIL("Failed to get expected vol from surface");
    /*
    cout << "strike,time,vol" << endl;
    for (Real k = 1.0; k < 1.6; k += 0.01) // extreme
        for (Time tt = 0.1; tt < 2; tt+= 0.05)
            cout << k << "," << tt << "," << volSurface.blackVol(tt, k) << endl;
     */
 
}

void FxVolSmileTest::testInvertedVolTermStructure () {

    BOOST_MESSAGE("Testing inverted vol term structure");

    SavedSettings backup;

    CommonVars vars;

    Handle<BlackVolTermStructure> surface (boost::shared_ptr<BlackVolTermStructure>
        (new BlackVarianceSurface (vars.today, TARGET(), vars.dates, vars.strikes, vars.vols, vars.dc)));

    BlackInvertedVolTermStructure bivt (surface);

    if (surface->maxDate() != bivt.maxDate())
        BOOST_FAIL("inverted maxDate() vol surface does not match base");

    if (surface->referenceDate() != bivt.referenceDate())
        BOOST_FAIL("inverted referenceDate() vol surface does not match base");

    // base spot is 100
    // test cases <Time, Strike>
    double testCases[][2] = {
        { 0.1, 104 },
        { 0.5, 90 },
        { 0.6, 110 },
        { 0.9, 90 },
        { 0.9, 95 },
        { 0.9, 100 },
        { 0.9, 105 },
        { 0.9, 110 }
    };

    for (Size i = 0; i < sizeof(testCases)/sizeof(testCases[0]); i++) {
        Time t = testCases[i][0];
        Real k = testCases[i][1];

        Real vol1 = surface->blackVol(t, k);

        Real invertedStrike = 1.0/k;
        Real vol2 = bivt.blackVol(t, invertedStrike);
        if (fabs(vol1 - vol2) > 0.00001)
            BOOST_FAIL("Failed to get expected vol ("<<vol1<<") from inverted vol surface, got ("<<vol2<<")");
    }

}

void FxVolSmileTest::testATMvolFromSurface () {

    BOOST_MESSAGE("Testing getting ATM vols from a surface with strike = 0");

    SavedSettings backup;

    CommonVars vars;

    Handle<BlackVolTermStructure> curve (boost::shared_ptr<BlackVolTermStructure> (
        new BlackVarianceCurve2 (vars.dates, vars.atmVols, vars.dc)));

    Handle<YieldTermStructure> domYTS (boost::shared_ptr<YieldTermStructure> (
        new FlatForward (vars.today, 0.01, vars.dc)));
    Handle<YieldTermStructure> forYTS (boost::shared_ptr<YieldTermStructure> (
        new FlatForward (vars.today, 0.02, vars.dc)));
    Handle<BlackVolTermStructure> surface (boost::shared_ptr<BlackVolTermStructure> (
        new FxBlackVannaVolgaVolatilitySurface (vars.dates, vars.atmVols, vars.rrs, vars.bfs,
                                                vars.dc, vars.baseSpot, domYTS, forYTS)));

    Handle<BlackVolTermStructure> invCurve (boost::shared_ptr<BlackVolTermStructure> (
        new BlackInvertedVolTermStructure (curve)));

    Handle<BlackVolTermStructure> invSurface (boost::shared_ptr<BlackVolTermStructure> (
        new BlackInvertedVolTermStructure (surface)));

    // test we get the same vol (the ATM vol from the surface when we set strike = 0)
    Time times[] = { 0.1, 0.14, 0.2, 0.45, 0.55, 0.77 };

    for (Size i = 0; i < sizeof(times)/sizeof(times[0]); i++) {
        Time t = times[i];
        Volatility vol1 = curve->blackVol(t, 100);
        Volatility vol2 = surface->blackVol(t, 0);
        Volatility vol3 = invCurve->blackVol(t, 0);
        Volatility vol4 = invSurface->blackVol(t, 0);
        if (fabs(vol1 - vol2) > 0.0000001)
            BOOST_FAIL("Failed to get expected ATM vol ("<<vol1<<") from vol surface, got ("<<vol2<<")");
        if (fabs(vol1 - vol3) > 0.0000001)
            BOOST_FAIL("Failed to get expected ATM vol ("<<vol1<<") from inverted vol curve, got ("<<vol3<<")");
        if (fabs(vol1 - vol4) > 0.0000001)
            BOOST_FAIL("Failed to get expected ATM vol ("<<vol1<<") from inverted vol surface, got ("<<vol4<<")");
    }
}

test_suite* FxVolSmileTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("FxVolSmileTests");
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testSurface2));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testStickyDeltaSurfaceWithCurve));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testStickyDeltaSurfaceWithSurface));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testVannaVolgaFxSmileSection));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testVannaVolgaFxVolSurface));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testInvertedVolTermStructure));
    suite->add(QUANTEXT_TEST_CASE(&FxVolSmileTest::testATMvolFromSurface));
    return suite;
}

