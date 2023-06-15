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

#include "toplevelfixture.hpp"

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

#include <qle/math/matrixfunctions.hpp>
#include <qle/models/transitionmatrix.hpp>

#include <ql/math/comparison.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(TransitionMatrix)

namespace {
template <class I> Real normEucl(I begin, I end) {
    Real sum = 0.0;
    for (I it = begin; it != end; ++it)
        sum += (*it) * (*it);
    return std::sqrt(sum);
}

template <class I> Real normMax(I begin, I end) {
    Real m = 0.0;
    for (I it = begin; it != end; ++it)
        m = std::max(std::abs(*it), m);
    return m;
}

template <class I> Real normMad(I begin, I end) {
    Real sum = 0.0;
    for (I it = begin; it != end; ++it)
        sum += std::abs(*it);
    return sum / static_cast<Real>(end - begin);
}
} // namespace

BOOST_AUTO_TEST_CASE(testGenerator) {

    if (!QuantExt::supports_Expm() || !QuantExt::supports_Logm()) {
        BOOST_CHECK(true);
        return;
    }

    BOOST_TEST_MESSAGE("Testing transition matrix generator computation...");

    // cf. Alexander Kreinin and Marina Sidelnikova, "Regularization Algorithms for Transition Matrices"
    // table 1 (Moody's average rating transition matrix of all corporates, 1980-1999)

    // clang-format off
        Real transDataRaw[] = {
        //  Aaa     Aa      A       Baa     Ba      B       C       Default
            0.8588, 0.0976, 0.0048, 0.0000, 0.0003, 0.0000, 0.0000, 0.0000, // Aaa
            0.0092, 0.8487, 0.0964, 0.0036, 0.0015, 0.0002, 0.0000, 0.0004, //  Aa
            0.0008, 0.0224, 0.8624, 0.0609, 0.0077, 0.0021, 0.0000, 0.0002, //   A
            0.0008, 0.0037, 0.0602, 0.7916, 0.0648, 0.0130, 0.0011, 0.0019, // Baa
            0.0003, 0.0008, 0.0046, 0.0402, 0.7676, 0.0788, 0.0047, 0.0140, //  Ba
            0.0001, 0.0004, 0.0016, 0.0053, 0.0586, 0.7607, 0.0274, 0.0660, //   B
            0.0000, 0.0000, 0.0000, 0.0100, 0.0279, 0.0538, 0.5674, 0.2535, //   C
            0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 1.0000  // Default
        };
    // clang-format on
    std::vector<Real> transData(transDataRaw, transDataRaw + 8 * 8);

    Matrix trans(8, 8, 0.0);
    for (Size i = 0; i < 8; ++i) {
        for (Size j = 0; j < 8; ++j) {
            trans[i][j] = transData[i * 8 + j];
        }
    }
    BOOST_TEST_MESSAGE("Original Transition Matrix =\n" << trans);

    // sanitise matrix

    Matrix transSan(trans);
    sanitiseTransitionMatrix(transSan);
    BOOST_TEST_MESSAGE("Sanitised Transition Matrix =\n" << transSan);
    for (Size i = 0; i < transSan.rows(); ++i) {
        Real sum = 0.0;
        for (Size j = 0; j < transSan.columns(); ++j) {
            if (i != j)
                BOOST_CHECK_CLOSE(trans[i][j], transSan[i][j], 1E-8);
            sum += transSan[i][j];
        }
        BOOST_CHECK_CLOSE(sum, 1.0, 1E-8);
    }
    BOOST_CHECK_NO_THROW(checkTransitionMatrix(transSan));

    // compute generator

    Matrix ltr = QuantExt::Logm(transSan);
    BOOST_TEST_MESSAGE("Log Transition Matrix=\n" << ltr);

    Matrix gen = generator(transSan);
    BOOST_TEST_MESSAGE("Generator =\n" << gen);
    BOOST_CHECK_NO_THROW(checkGeneratorMatrix(gen));

    // compute approxmiate 1y transition matrix

    Matrix approx1y = QuantExt::Expm(gen);
    BOOST_TEST_MESSAGE("Approximate Transition Matrix =\n" << approx1y);
    checkTransitionMatrix(approx1y);

    // check results from table 5

    Real rowDist[] = {6.769E-4, 0.032E-4, 1.021E-4, 0.0, 0.0, 0.0, 6.475E-4, 0.0};
    Matrix diff1 = gen - ltr;

    for (Size i = 0; i < 7; ++i) {
        Real dist = normEucl(diff1.row_begin(i), diff1.row_end(i));
        BOOST_TEST_MESSAGE("row " << i << " reference result " << rowDist[i] << " actual result " << dist);
        if (QuantLib::close_enough(rowDist[i], 0.0)) {
            BOOST_CHECK(dist < 100 * QL_EPSILON);
        } else
            // 2% rel. diff. to value in paper
            BOOST_CHECK_CLOSE(dist, rowDist[i], 2.0);
    }

    // check results from table 7

    Matrix diff2 = approx1y - transSan;
    // 1% rel. diff. to value in paper
    BOOST_CHECK_CLOSE(normMax(diff2.begin(), diff2.end()), 4.599E-4, 1.0);
    BOOST_CHECK_CLOSE(normMad(diff2.begin(), diff2.end()), 0.382E-4, 1.0);

} // testGenerator

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
