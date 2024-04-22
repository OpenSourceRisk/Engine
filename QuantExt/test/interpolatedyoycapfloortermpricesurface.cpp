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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CapFloordTermPriceSurfaceTest)

BOOST_AUTO_TEST_CASE(testInterpolatedYoyCapFloorTermPriceSurface) {

    BOOST_TEST_MESSAGE("Testing InterpolatedYoyCapFloorTermPriceSurface");

    Date asof_ = Date(18, July, 2016);
    Settings::instance().evaluationDate() = asof_;

    Handle<YieldTermStructure> nominalTs =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, TARGET(), 0.005, Actual365Fixed()));

    std::vector<Rate> capStrikes;
    capStrikes.push_back(0.01);
    capStrikes.push_back(0.02);
    capStrikes.push_back(0.03);
    capStrikes.push_back(0.04);
    capStrikes.push_back(0.05);

    std::vector<Rate> floorStrikes;
    floorStrikes.push_back(-0.02);
    floorStrikes.push_back(-0.01);
    floorStrikes.push_back(0.0);
    floorStrikes.push_back(0.01);
    floorStrikes.push_back(0.02);

    std::vector<Period> maturities;
    maturities.push_back(Period(2, Years));
    maturities.push_back(Period(5, Years));
    maturities.push_back(Period(7, Years));
    maturities.push_back(Period(10, Years));
    maturities.push_back(Period(15, Years));
    maturities.push_back(Period(20, Years));

    Matrix capPrice(capStrikes.size(), maturities.size(), Null<Real>()),
        floorPrice(floorStrikes.size(), maturities.size(), Null<Real>());

    capPrice[0][0] = 0.00874;
    capPrice[1][0] = 0.00146;
    capPrice[2][0] = 0.00019;
    capPrice[3][0] = 0.00003;
    capPrice[4][0] = 0.00001;
    capPrice[0][1] = 0.02946;
    capPrice[1][1] = 0.00793;
    capPrice[2][1] = 0.00214;
    capPrice[3][1] = 0.00074;
    capPrice[4][1] = 0.00032;
    capPrice[0][2] = 0.04626;
    capPrice[1][2] = 0.01448;
    capPrice[2][2] = 0.00481;
    capPrice[3][2] = 0.00206;
    capPrice[4][2] = 0.00107;
    capPrice[0][3] = 0.07622;
    capPrice[1][3] = 0.02778;
    capPrice[2][3] = 0.01106;
    capPrice[3][3] = 0.00565;
    capPrice[4][3] = 0.00343;
    capPrice[0][4] = 0.13218;
    capPrice[1][4] = 0.05476;
    capPrice[2][4] = 0.0245;
    capPrice[3][4] = 0.01393;
    capPrice[4][4] = 0.00927;
    capPrice[0][5] = 0.18889;
    capPrice[1][5] = 0.08297;
    capPrice[2][5] = 0.03909;
    capPrice[3][5] = 0.02369;
    capPrice[4][5] = 0.01674;
    floorPrice[0][0] = 0.000000001;
    floorPrice[1][0] = 0.00005;
    floorPrice[2][0] = 0.00057;
    floorPrice[3][0] = 0.00415;
    floorPrice[4][0] = 0.01695;
    floorPrice[0][1] = 0.00005;
    floorPrice[1][1] = 0.00071;
    floorPrice[2][1] = 0.00259;
    floorPrice[3][1] = 0.01135;
    floorPrice[4][1] = 0.03983;
    floorPrice[0][2] = 0.00035;
    floorPrice[1][2] = 0.00131;
    floorPrice[2][2] = 0.00482;
    floorPrice[3][2] = 0.0169;
    floorPrice[4][2] = 0.05463;
    floorPrice[0][3] = 0.0014;
    floorPrice[1][3] = 0.0036;
    floorPrice[2][3] = 0.00943;
    floorPrice[3][3] = 0.02584;
    floorPrice[4][3] = 0.07515;
    floorPrice[0][4] = 0.00481;
    floorPrice[1][4] = 0.00904;
    floorPrice[2][4] = 0.01814;
    floorPrice[3][4] = 0.04028;
    floorPrice[4][4] = 0.10449;
    floorPrice[0][5] = 0.00832;
    floorPrice[1][5] = 0.01433;
    floorPrice[2][5] = 0.02612;
    floorPrice[3][5] = 0.05269;
    floorPrice[4][5] = 0.12839;

    // build a
    std::vector<Date> datesZCII;
    datesZCII.push_back(asof_ + 1 * Years);
    std::vector<Rate> ratesZCII;
    ratesZCII.push_back(1.1625);

    // build EUHICPXT fixing history
    Schedule fixingDatesEUHICPXT =
        MakeSchedule().from(Date(1, May, 2015)).to(Date(1, July, 2016)).withTenor(1 * Months);
    std::vector<Real> fixingRatesEUHICPXT(15, 100);

    Handle<ZeroInflationIndex> hEUHICPXT;
    QuantLib::ext::shared_ptr<EUHICPXT> ii = QuantLib::ext::shared_ptr<EUHICPXT>(new EUHICPXT());
    QuantLib::ext::shared_ptr<ZeroInflationTermStructure> cpiTS;
    for (Size i = 0; i < fixingDatesEUHICPXT.size(); i++) {
        ii->addFixing(fixingDatesEUHICPXT[i], fixingRatesEUHICPXT[i], true);
    };
    // now build the helpers ...
    std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure> > > instruments;
    for (Size i = 0; i < datesZCII.size(); i++) {
        Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(ratesZCII[i] / 100.0)));
        QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure> > anInstrument(new ZeroCouponInflationSwapHelper(
        quote, Period(3, Months), datesZCII[i], TARGET(), ModifiedFollowing, Actual365Fixed(), ii, CPI::AsIndex, nominalTs));
        instruments.push_back(anInstrument);
    };

    Rate baseZeroRate = ratesZCII[0] / 100.0;
    QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(new PiecewiseZeroInflationCurve<Linear>(
        asof_, TARGET(), Actual365Fixed(), Period(3, Months), Monthly, baseZeroRate, instruments));
    pCPIts->recalculate();
    cpiTS = QuantLib::ext::dynamic_pointer_cast<ZeroInflationTermStructure>(pCPIts);

    QuantLib::ext::shared_ptr<EUHICPXT> zii(new EUHICPXT(Handle<ZeroInflationTermStructure>(pCPIts)));
    QuantLib::ext::shared_ptr<ZeroInflationIndex> zeroIndex = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(zii);

    QuantLib::ext::shared_ptr<YoYInflationIndex> yoyIndex;

    yoyIndex =
        QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, true, Handle<YoYInflationTermStructure>());

    QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> ys(
        0, Period(3, Months), yoyIndex, 1, nominalTs, Actual365Fixed(), TARGET(), Following, capStrikes, floorStrikes,
        maturities, capPrice, floorPrice);

    QuantLib::ext::shared_ptr<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> > yoySurface =
        QuantLib::ext::make_shared<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> >(ys);

    // check the cap and floor prices from the surface
    Real tol = 1.0E-8;
    for (Size i = 0; i < maturities.size(); i++) {
        Date m = yoySurface->yoyOptionDateFromTenor(maturities[i]);
        for (Size j = 0; j < capStrikes.size(); j++) {
            BOOST_CHECK_CLOSE(yoySurface->capPrice(m, capStrikes[j]), capPrice[j][i], tol);
        }
        for (Size j = 0; j < floorStrikes.size(); j++) {
            BOOST_CHECK_CLOSE(yoySurface->floorPrice(m, floorStrikes[j]), floorPrice[j][i], tol);
        }
    }
} // testInterpolatedYoyCapFloorTermPriceSurface

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
