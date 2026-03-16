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

struct CommonData {
    Date today;
    std::vector<double> detachmentPoints;
    std::vector<Period> tenors;
    Date startDate;

    CommonData()
        : today(22, Sep, 2022), detachmentPoints({0.03, 0.07, 0.15, 1.0}), tenors({3 * Years, 5 * Years}),
          startDate(20, Sep, 2021) {}
};

void initializeSettings(CommonData& cd) {
    Settings::instance().evaluationDate() = cd.today;
}

Handle<QuantExt::BaseCorrelationTermStructure> buildBilinearFlatBaseCorrelationCurve(CommonData& cd) {    
    // Correlation matrix detachmentPoints x tenors
    std::vector<std::vector<double>> correlations{
        {0.409223169, 0.405249307}, {0.507498351, 0.486937064}, {0.614741119, 0.623673691}, {1.0, 1.0}};

    std::vector<std::vector<Handle<Quote>>> quotes;
    for (const auto& row : correlations) {
        quotes.push_back(std::vector<Handle<Quote>>());
        for (const auto& correl : row) {
            quotes.back().push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(correl)));
        }
    }

    return Handle<QuantExt::BaseCorrelationTermStructure>(QuantLib::ext::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<QuantExt::BilinearFlat>>(
        0, WeekendsOnly(), ModifiedFollowing, cd.tenors, cd.detachmentPoints, quotes, Actual365Fixed(), cd.startDate,
        DateGeneration::CDS2015));
}

BOOST_AUTO_TEST_CASE(testBaseCorrelationCurve) {
    CommonData cd;

    initializeSettings(cd);

    auto curve = buildBilinearFlatBaseCorrelationCurve(cd);
   

    BOOST_CHECK_EQUAL(curve->dates()[0], Date(20, Dec, 2024));
    BOOST_CHECK_EQUAL(curve->dates()[1], Date(20, Dec, 2026));

    // Check quoted points
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);
    // Check detachment point interpolation
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2026), 0.05), (0.405249307 + 0.486937064) / 2.0, 1e-10);
    // Check if exceptions are thrown for extrapolation without enable extrapolation
    curve->disableExtrapolation();
    BOOST_CHECK_THROW(curve->correlation(Date(20, Dec, 2026), 0.01), std::exception);
    BOOST_CHECK_THROW(curve->correlation(Date(20, Dec, 2021), 0.03), std::exception);
    curve->enableExtrapolation();
    // Check if extrapolation is flat 
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2026), 0.01), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2028), 0.05), (0.405249307 + 0.486937064) / 2.0, 1e-10);
    BOOST_CHECK_CLOSE(curve->correlation(Date(20, Dec, 2022), 0.05), (0.409223169 + 0.507498351) / 2.0, 1e-10);

    
}


BOOST_AUTO_TEST_CASE(testSpreadedCorrelationCurve) {
    CommonData cd;

    initializeSettings(cd);

    auto curve = buildBilinearFlatBaseCorrelationCurve(cd);

    std::vector<std::vector<Handle<Quote>>> shifts;

    for (Size i = 0; i < cd.detachmentPoints.size(); ++i) {
        shifts.push_back(std::vector<Handle<Quote>>());
        for (Size j = 0; j < cd.tenors.size(); ++j) {
            shifts[i].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)));
        }
    }
    
    QuantExt::SpreadedBaseCorrelationCurve shiftedCurve(curve, cd.tenors, cd.detachmentPoints, shifts, cd.startDate, DateGeneration::CDS2015);

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*shifts[0][1])->setValue(0.01);

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.03), 0.415249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    BOOST_CHECK_CLOSE(shiftedCurve.correlation(Date(20, Dec, 2026), 0.05), (0.415249307 + 0.486937064) / 2.0, 1e-10);
}

BOOST_AUTO_TEST_CASE(testParallelShift) {
    CommonData cd;

    initializeSettings(cd);

    auto curve = buildBilinearFlatBaseCorrelationCurve(cd);
    // Simple parallel shift, we need at least a 2 x 2 matrix for the interpolation
    // but the actual tenor and detachments points doesn't matter
    auto parallelShift = QuantLib::ext::make_shared<SimpleQuote>(0.0);

    std::vector<std::vector<Handle<Quote>>> quotes{{Handle<Quote>(parallelShift)}};

    std::vector<Period> terms{1 * Days};
    std::vector<double> detachmentPoints{1.0};

    Size nt = terms.size();
    Size nd = detachmentPoints.size();

    if (nt == 1) {
        terms.push_back(terms[0] + 1 * Days); // arbitrary, but larger than the first term
        for (Size i = 0; i < nd; ++i)
            quotes[i].push_back(quotes[i][0]);
    }

    if (nd == 1) {
        quotes.push_back(std::vector<Handle<Quote>>(terms.size()));
        for (Size j = 0; j < terms.size(); ++j)
            quotes[1][j] = quotes[0][j];

        if (detachmentPoints[0] < 1.0 && !QuantLib::close_enough(detachmentPoints[0], 1.0)) {
            detachmentPoints.push_back(1.0);
        } else {
            detachmentPoints.insert(detachmentPoints.begin(), 0.01);
        }
    }


    QuantExt::SpreadedBaseCorrelationCurve shiftedCurveParallel(curve, terms, detachmentPoints, quotes);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.03), 0.405249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.07), 0.486937064, 1e-10);

    parallelShift->setValue(0.01);

    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.03), 0.415249307, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.07), 0.496937064, 1e-10);
    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 1.0), 1.0, 1e-10);

    BOOST_CHECK_CLOSE(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.05), (0.415249307 + 0.496937064) / 2.0,
                      1e-10);

    BOOST_CHECK_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.01), std::exception);
    BOOST_CHECK_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2028), 0.03), std::exception);

    shiftedCurveParallel.enableExtrapolation();
    BOOST_CHECK_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.01), std::exception);
    BOOST_CHECK_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2028), 0.03), std::exception);

    // No to enable extrapolation on both curves before extrapolate
    shiftedCurveParallel.enableExtrapolation();
    curve->enableExtrapolation();
    BOOST_CHECK_NO_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2026), 0.01));
    BOOST_CHECK_NO_THROW(shiftedCurveParallel.correlation(Date(20, Dec, 2028), 0.03));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()