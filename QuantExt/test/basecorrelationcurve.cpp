/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/math/flatextrapolation2d.hpp>
#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <qle/termstructures/credit/spreadedbasecorrelationcurve.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BaseCorrelationTests)

BOOST_AUTO_TEST_CASE(testBaseCorrelationCurve) {

    Date today(20, Sep, 2022);

    Settings::instance().evaluationDate() = today;

    Date startDate(20, Sep, 2021);

    std::vector<double> detachmentPoints{0.03, 0.07, 0.15, 1.0};
    std::vector<Period> tenors{3 * Years, 5 * Years};
    // Correlation matrix detachmentPoints x tenors
    std::vector<std::vector<double>> correlations{
        {0.409223169, 0.405249307}, {0.507498351, 0.486937064}, {0.614741119, 0.623673691}, {1.0, 1.0}};

    std::vector<std::vector<Handle<Quote>>> quotes;
    for (const auto& row : correlations) {
        quotes.push_back(std::vector<Handle<Quote>>());
        for (const auto& correl : row) {
            quotes.back().push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(correl)));
        }
    }

    QuantExt::InterpolatedBaseCorrelationTermStructure<QuantExt::BilinearFlat> baseCorrCurve(
        0, WeekendsOnly(), ModifiedFollowing, tenors, detachmentPoints, quotes, Actual365Fixed(), startDate,
        DateGeneration::CDS2015);

    baseCorrCurve.enableExtrapolation();

    BOOST_CHECK_EQUAL(baseCorrCurve.dates()[0], Date(20, Dec, 2024));
    BOOST_CHECK_EQUAL(baseCorrCurve.dates()[1], Date(20, Dec, 2026));

    // Check quoted points
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);
    // Check detachment point interpolation
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2026), 0.05), (0.405249307 + 0.486937064) / 2.0, 1e-10);
    // Check detachment point extrapolation
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2026), 0.01), 0.405249307, 1e-10);
    // Check time extrapolation
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2028), 0.05), (0.405249307 + 0.486937064) / 2.0, 1e-10);
    BOOST_CHECK_CLOSE(baseCorrCurve.correlation(Date(20, Dec, 2022), 0.05), (0.409223169 + 0.507498351) / 2.0, 1e-10);

    std::vector<std::vector<Handle<Quote>>> shifts;
    for (const auto& row : correlations) {
        shifts.push_back(std::vector<Handle<Quote>>(row.size(), Handle<Quote>(boost::make_shared<SimpleQuote>(0.0))));
    }

    Handle<QuantExt::BaseCorrelationTermStructure> baseCurve(
        boost::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<QuantExt::BilinearFlat>>(baseCorrCurve));

    baseCurve->enableExtrapolation();

    QuantExt::SpreadedBaseCorrelationCurve shiftedCurve(baseCurve, tenors, detachmentPoints, shifts, startDate,
                                                        DateGeneration::CDS2015);
    // shiftedCurve.enableExtrapolation();

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    boost::dynamic_pointer_cast<SimpleQuote>(*shifts[0][1])->setValue(0.01);

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.03), 0.415249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.05), (0.415249307 + 0.486937064) / 2.0, 1e-10);

    // Simple parallel shift, we need at least a 2 x 2 matrix for the interpolation
    // but the actual tenor and detachments points doesn't matter
    auto parallelShift = boost::make_shared<SimpleQuote>(0.0);

    QuantExt::SpreadedBaseCorrelationCurve shiftedCurveParallel(
        baseCurve, {1 * Days, 2 * Days}, {0.01, 0.02},
        {{Handle<Quote>(parallelShift), Handle<Quote>(parallelShift)},
         {Handle<Quote>(parallelShift), Handle<Quote>(parallelShift)}});

    shiftedCurveParallel.enableExtrapolation();

    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    parallelShift->setValue(0.01);

    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.03), 0.415249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.07), 0.496937064, 1e-10);

    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.05), (0.415249307 + 0.496937064) / 2.0,
                      1e-10);

    std::cout << baseCorrCurve.correlation(1, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()