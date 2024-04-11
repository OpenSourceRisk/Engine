/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackdeltautilities.hpp>
#include <qle/termstructures/blackvolsurfacebfrr.hpp>

#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

struct BFRRVolFixture : public qle::test::TopLevelFixture {
public:
    BFRRVolFixture() { Settings::instance().evaluationDate() = refDate; }
    Date refDate = Date(13, April, 2021);
    std::vector<Date> dates = {refDate + 1 * Years, refDate + 3 * Years};
    std::vector<Real> deltas = {0.10, 0.25};
    std::vector<std::vector<Real>> bfQuotes = {{0.02, 0.01}, {0.01, 0.0050}};
    std::vector<std::vector<Real>> rrQuotes = {{-0.015, -0.012}, {-0.011, -0.009}};
    std::vector<Real> atmQuotes = {0.09, 0.08};
    Actual365Fixed dc;
    NullCalendar cal;
    Handle<Quote> spot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.2));
    Size spotDays = 2;
    Handle<YieldTermStructure> domesticTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(refDate, 0.01, dc));
    Handle<YieldTermStructure> foreignTS =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(refDate, 0.015, dc));
    DeltaVolQuote::DeltaType dt = DeltaVolQuote::DeltaType::PaSpot;
    DeltaVolQuote::AtmType at = DeltaVolQuote::AtmType::AtmDeltaNeutral;
    Period switchTenor = 2 * Years;
    DeltaVolQuote::DeltaType ltdt = DeltaVolQuote::DeltaType::PaFwd;
    DeltaVolQuote::AtmType ltat = DeltaVolQuote::AtmType::AtmDeltaNeutral;
    Option::Type rrInFavorOf = Option::Call;
    BlackVolatilitySurfaceBFRR::SmileInterpolation smileInterpolation =
        BlackVolatilitySurfaceBFRR::SmileInterpolation::Cubic;
};

BOOST_AUTO_TEST_SUITE(QuantExtTestSuite)

BOOST_FIXTURE_TEST_SUITE(BFRRVolSurfaceTest, BFRRVolFixture)

BOOST_AUTO_TEST_CASE(testSmileBF) {

    BOOST_TEST_MESSAGE("Testing bf/rr vol surface with smile bf quotes...");

    Real tol1 = 1E-5;

    auto vol1 = QuantLib::ext::make_shared<BlackVolatilitySurfaceBFRR>(
        refDate, dates, deltas, bfQuotes, rrQuotes, atmQuotes, dc, cal, spot, spotDays, cal, domesticTS, foreignTS, dt,
        at, switchTenor, ltdt, ltat, rrInFavorOf, false, smileInterpolation);

    Real t1 = vol1->timeFromReference(dates[0]);
    Real domDisc1 = domesticTS->discount(dates[0] + spotDays) / domesticTS->discount(refDate + spotDays);
    Real forDisc1 = foreignTS->discount(dates[0] + spotDays) / foreignTS->discount(refDate + spotDays);
    Real k1_1_10p = getStrikeFromDelta(Option::Put, -deltas[0], dt, spot->value(), domDisc1, forDisc1, vol1, t1);
    Real k1_1_25p = getStrikeFromDelta(Option::Put, -deltas[1], dt, spot->value(), domDisc1, forDisc1, vol1, t1);
    Real k1_1_atm = getAtmStrike(dt, at, spot->value(), domDisc1, forDisc1, vol1, t1);
    Real k1_1_25c = getStrikeFromDelta(Option::Call, deltas[1], dt, spot->value(), domDisc1, forDisc1, vol1, t1);
    Real k1_1_10c = getStrikeFromDelta(Option::Call, deltas[0], dt, spot->value(), domDisc1, forDisc1, vol1, t1);

    BOOST_CHECK_SMALL(vol1->blackVol(dates[0], k1_1_10p) - (atmQuotes[0] + bfQuotes[0][0] - 0.5 * rrQuotes[0][0]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[0], k1_1_25p) - (atmQuotes[0] + bfQuotes[0][1] - 0.5 * rrQuotes[0][1]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[0], k1_1_atm) - atmQuotes[0], tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[0], k1_1_25c) - (atmQuotes[0] + bfQuotes[0][1] + 0.5 * rrQuotes[0][1]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[0], k1_1_10c) - (atmQuotes[0] + bfQuotes[0][0] + 0.5 * rrQuotes[0][0]),
                      tol1);

    Real t2 = vol1->timeFromReference(dates[1]);
    Real domDisc2 = domesticTS->discount(dates[1] + spotDays) / domesticTS->discount(refDate + spotDays);
    Real forDisc2 = foreignTS->discount(dates[1] + spotDays) / foreignTS->discount(refDate + spotDays);
    Real k1_2_10p = getStrikeFromDelta(Option::Put, -deltas[0], ltdt, spot->value(), domDisc2, forDisc2, vol1, t2);
    Real k1_2_25p = getStrikeFromDelta(Option::Put, -deltas[1], ltdt, spot->value(), domDisc2, forDisc2, vol1, t2);
    Real k1_2_atm = getAtmStrike(ltdt, ltat, spot->value(), domDisc2, forDisc2, vol1, t2);
    Real k1_2_25c = getStrikeFromDelta(Option::Call, deltas[1], ltdt, spot->value(), domDisc2, forDisc2, vol1, t2);
    Real k1_2_10c = getStrikeFromDelta(Option::Call, deltas[0], ltdt, spot->value(), domDisc2, forDisc2, vol1, t2);

    BOOST_CHECK_SMALL(vol1->blackVol(dates[1], k1_2_10p) - (atmQuotes[1] + bfQuotes[1][0] - 0.5 * rrQuotes[1][0]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[1], k1_2_25p) - (atmQuotes[1] + bfQuotes[1][1] - 0.5 * rrQuotes[1][1]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[1], k1_2_atm) - atmQuotes[1], tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[1], k1_2_25c) - (atmQuotes[1] + bfQuotes[1][1] + 0.5 * rrQuotes[1][1]),
                      tol1);
    BOOST_CHECK_SMALL(vol1->blackVol(dates[1], k1_2_10c) - (atmQuotes[1] + bfQuotes[1][0] + 0.5 * rrQuotes[1][0]),
                      tol1);
}

BOOST_AUTO_TEST_CASE(testBrokerBF) {

    BOOST_TEST_MESSAGE("Testing bf/rr vol surface with broker bf quotes...");

    Real tol1 = 1E-5;
    Real tol2 = 1E-5;

    auto vol2 = QuantLib::ext::make_shared<BlackVolatilitySurfaceBFRR>(
        refDate, dates, deltas, bfQuotes, rrQuotes, atmQuotes, dc, cal, spot, spotDays, cal, domesticTS, foreignTS, dt,
        at, switchTenor, ltdt, ltat, rrInFavorOf, true, smileInterpolation);

    Real t1 = vol2->timeFromReference(dates[0]);
    Real domDisc1 = domesticTS->discount(dates[0] + spotDays) / domesticTS->discount(refDate + spotDays);
    Real forDisc1 = foreignTS->discount(dates[0] + spotDays) / foreignTS->discount(refDate + spotDays);
    Real t2 = vol2->timeFromReference(dates[1]);
    Real domDisc2 = domesticTS->discount(dates[1] + spotDays) / domesticTS->discount(refDate + spotDays);
    Real forDisc2 = foreignTS->discount(dates[1] + spotDays) / foreignTS->discount(refDate + spotDays);

    // checks for expiry 1

    Real bfvol2_1_10_broker = atmQuotes[0] + bfQuotes[0][0];
    Real bfvol2_1_25_broker = atmQuotes[0] + bfQuotes[0][1];

    BlackDeltaCalculator bdc_1_10_p(Option::Put, dt, spot->value(), domDisc1, forDisc1,
                                    bfvol2_1_10_broker * std::sqrt(t1));
    Real k2_1_10p_broker = bdc_1_10_p.strikeFromDelta(-deltas[0]);

    BlackDeltaCalculator bdc_1_10_c(Option::Call, dt, spot->value(), domDisc1, forDisc1,
                                    bfvol2_1_10_broker * std::sqrt(t1));
    Real k2_1_10c_broker = bdc_1_10_c.strikeFromDelta(deltas[0]);

    BlackDeltaCalculator bdc_1_25_p(Option::Put, dt, spot->value(), domDisc1, forDisc1,
                                    bfvol2_1_25_broker * std::sqrt(t1));
    Real k2_1_25p_broker = bdc_1_25_p.strikeFromDelta(-deltas[1]);

    BlackDeltaCalculator bdc_1_25_c(Option::Call, dt, spot->value(), domDisc1, forDisc1,
                                    bfvol2_1_25_broker * std::sqrt(t1));
    Real k2_1_25c_broker = bdc_1_25_c.strikeFromDelta(deltas[1]);

    Real bfPrice_1_10_broker = blackFormula(Option::Put, k2_1_10p_broker, spot->value() / domDisc1 * forDisc1,
                                            bfvol2_1_10_broker * std::sqrt(t1)) +
                               blackFormula(Option::Call, k2_1_10c_broker, spot->value() / domDisc1 * forDisc1,
                                            bfvol2_1_10_broker * std::sqrt(t1));

    Real bfPrice_1_25_broker = blackFormula(Option::Put, k2_1_25p_broker, spot->value() / domDisc1 * forDisc1,
                                            bfvol2_1_25_broker * std::sqrt(t1)) +
                               blackFormula(Option::Call, k2_1_25c_broker, spot->value() / domDisc1 * forDisc1,
                                            bfvol2_1_25_broker * std::sqrt(t1));

    Real k2_1_10p = getStrikeFromDelta(Option::Put, -deltas[0], dt, spot->value(), domDisc1, forDisc1, vol2, t1);
    Real k2_1_25p = getStrikeFromDelta(Option::Put, -deltas[1], dt, spot->value(), domDisc1, forDisc1, vol2, t1);
    Real k2_1_atm = getAtmStrike(dt, at, spot->value(), domDisc1, forDisc1, vol2, t1);
    Real k2_1_25c = getStrikeFromDelta(Option::Call, deltas[1], dt, spot->value(), domDisc1, forDisc1, vol2, t1);
    Real k2_1_10c = getStrikeFromDelta(Option::Call, deltas[0], dt, spot->value(), domDisc1, forDisc1, vol2, t1);

    Real bfPrice_1_10_smile = blackFormula(Option::Put, k2_1_10p_broker, spot->value() / domDisc1 * forDisc1,
                                           std::sqrt(vol2->blackVariance(dates[0], k2_1_10p_broker))) +
                              blackFormula(Option::Call, k2_1_10c_broker, spot->value() / domDisc1 * forDisc1,
                                           std::sqrt(vol2->blackVariance(dates[0], k2_1_10c_broker)));

    Real bfPrice_1_25_smile = blackFormula(Option::Put, k2_1_25p_broker, spot->value() / domDisc1 * forDisc1,
                                           std::sqrt(vol2->blackVariance(dates[0], k2_1_25p_broker))) +
                              blackFormula(Option::Call, k2_1_25c_broker, spot->value() / domDisc1 * forDisc1,
                                           std::sqrt(vol2->blackVariance(dates[0], k2_1_25c_broker)));

    // check broker bf premium = smile bf premium

    BOOST_CHECK_SMALL(bfPrice_1_10_smile - bfPrice_1_10_broker, tol2);
    BOOST_CHECK_SMALL(bfPrice_1_25_smile - bfPrice_1_25_broker, tol2);

    // check rr and atm quotes are reproduced on smile

    BOOST_CHECK_SMALL(vol2->blackVol(dates[0], k2_1_10c) - vol2->blackVol(dates[0], k2_1_10p) - rrQuotes[0][0], tol1);
    BOOST_CHECK_SMALL(vol2->blackVol(dates[0], k2_1_25c) - vol2->blackVol(dates[0], k2_1_25p) - rrQuotes[0][1], tol1);
    BOOST_CHECK_SMALL(vol2->blackVol(dates[0], k2_1_atm) - atmQuotes[0], tol1);

    // checks for expiry 2

    Real bfvol2_2_10_broker = atmQuotes[1] + bfQuotes[1][0];
    Real bfvol2_2_25_broker = atmQuotes[1] + bfQuotes[1][1];

    BlackDeltaCalculator bdc_2_10_p(Option::Put, ltdt, spot->value(), domDisc2, forDisc2,
                                    bfvol2_2_10_broker * std::sqrt(t2));
    Real k2_2_10p_broker = bdc_2_10_p.strikeFromDelta(-deltas[0]);

    BlackDeltaCalculator bdc_2_10_c(Option::Call, ltdt, spot->value(), domDisc2, forDisc2,
                                    bfvol2_2_10_broker * std::sqrt(t2));
    Real k2_2_10c_broker = bdc_2_10_c.strikeFromDelta(deltas[0]);

    BlackDeltaCalculator bdc_2_25_p(Option::Put, ltdt, spot->value(), domDisc2, forDisc2,
                                    bfvol2_2_25_broker * std::sqrt(t2));
    Real k2_2_25p_broker = bdc_2_25_p.strikeFromDelta(-deltas[1]);

    BlackDeltaCalculator bdc_2_25_c(Option::Call, ltdt, spot->value(), domDisc2, forDisc2,
                                    bfvol2_2_25_broker * std::sqrt(t2));
    Real k2_2_25c_broker = bdc_2_25_c.strikeFromDelta(deltas[1]);

    Real bfPrice_2_10_broker = blackFormula(Option::Put, k2_2_10p_broker, spot->value() / domDisc2 * forDisc2,
                                            bfvol2_2_10_broker * std::sqrt(t2)) +
                               blackFormula(Option::Call, k2_2_10c_broker, spot->value() / domDisc2 * forDisc2,
                                            bfvol2_2_10_broker * std::sqrt(t2));

    Real bfPrice_2_25_broker = blackFormula(Option::Put, k2_2_25p_broker, spot->value() / domDisc2 * forDisc2,
                                            bfvol2_2_25_broker * std::sqrt(t2)) +
                               blackFormula(Option::Call, k2_2_25c_broker, spot->value() / domDisc2 * forDisc2,
                                            bfvol2_2_25_broker * std::sqrt(t2));

    Real k2_2_10p = getStrikeFromDelta(Option::Put, -deltas[0], ltdt, spot->value(), domDisc2, forDisc2, vol2, t2);
    Real k2_2_25p = getStrikeFromDelta(Option::Put, -deltas[1], ltdt, spot->value(), domDisc2, forDisc2, vol2, t2);
    Real k2_2_atm = getAtmStrike(ltdt, ltat, spot->value(), domDisc2, forDisc2, vol2, t2);
    Real k2_2_25c = getStrikeFromDelta(Option::Call, deltas[1], ltdt, spot->value(), domDisc2, forDisc2, vol2, t2);
    Real k2_2_10c = getStrikeFromDelta(Option::Call, deltas[0], ltdt, spot->value(), domDisc2, forDisc2, vol2, t2);

    Real bfPrice_2_10_smile = blackFormula(Option::Put, k2_2_10p_broker, spot->value() / domDisc2 * forDisc2,
                                           std::sqrt(vol2->blackVariance(dates[1], k2_2_10p_broker))) +
                              blackFormula(Option::Call, k2_2_10c_broker, spot->value() / domDisc2 * forDisc2,
                                           std::sqrt(vol2->blackVariance(dates[1], k2_2_10c_broker)));

    Real bfPrice_2_25_smile = blackFormula(Option::Put, k2_2_25p_broker, spot->value() / domDisc2 * forDisc2,
                                           std::sqrt(vol2->blackVariance(dates[1], k2_2_25p_broker))) +
                              blackFormula(Option::Call, k2_2_25c_broker, spot->value() / domDisc2 * forDisc2,
                                           std::sqrt(vol2->blackVariance(dates[1], k2_2_25c_broker)));

    // check broker bf premium = smile bf premium

    BOOST_CHECK_SMALL(bfPrice_2_10_smile - bfPrice_2_10_broker, tol2);
    BOOST_CHECK_SMALL(bfPrice_2_25_smile - bfPrice_2_25_broker, tol2);

    // check rr and atm quotes are reproduced on smile

    BOOST_CHECK_SMALL(vol2->blackVol(dates[1], k2_2_10c) - vol2->blackVol(dates[1], k2_2_10p) - rrQuotes[1][0], tol1);
    BOOST_CHECK_SMALL(vol2->blackVol(dates[1], k2_2_25c) - vol2->blackVol(dates[1], k2_2_25p) - rrQuotes[1][1], tol1);
    BOOST_CHECK_SMALL(vol2->blackVol(dates[1], k2_2_atm) - atmQuotes[1], tol1);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
