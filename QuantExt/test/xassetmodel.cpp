/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "xassetmodel.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

namespace {

// check for expected result up to round off errors
void check(std::string s, const Real x, const Real y, const Real e,
           const Size n = 42) {
    if (!close_enough(y, e, n)) {
        BOOST_ERROR("failed to verify " << s << "(" << x << ") = " << e
                                        << " up to round off errors, it is "
                                        << y << " instead (difference is "
                                        << (y - e) << ", n is " << n << ")");
    }
};

// check for expected result up to truncation errors with absolute tolerance
void check2(std::string s, const Real x, const Real y, const Real e,
            const Real tol) {
    if (std::abs(y - e) > tol) {
        BOOST_ERROR("failed to verify " << s << "(" << x << ") = " << e
                                        << ", it is " << y
                                        << " instead (difference is " << (y - e)
                                        << ", abs tol=" << tol << ")");
    }
};

} // anonymous namespace

void XAssetModelTest::testParametrizationBaseClasses() {

    BOOST_TEST_MESSAGE(
        "Testing XAssetModel parametrizations (base classes)...");

    // base class

    Parametrization p1((EURCurrency()));
    if (p1.parameterSize(42) != 0) {
        BOOST_ERROR("empty parametrization should have zero parameter "
                    "size, it is "
                    << p1.parameterSize(42) << " instead");
    }
    if (p1.parameterTimes(42) != Array()) {
        BOOST_ERROR("empty parametrization should have empty times "
                    "array, is has size "
                    << p1.parameterTimes(42).size() << " though");
    }

    // piecewise constant helpers

    Array noTimes, three(1, 3.0), zero(1, 0.0);
    PiecewiseConstantHelper1 helper11(noTimes, three);
    check("helper11.y", 0.0, helper11.y(0.0), 3.0);
    check("helper11.y", 1.0, helper11.y(1.0), 3.0);
    check("helper11.y", 3.0, helper11.y(3.0), 3.0);
    check("helper11.int_y_sqr", 0.0, helper11.int_y_sqr(0.0), 0.0);
    check("helper11.int_y_sqr", 1.0, helper11.int_y_sqr(1.0), 9.0);
    check("helper11.int_y_sqr", 3.0, helper11.int_y_sqr(3.0), 27.0);

    PiecewiseConstantHelper2 helper21(noTimes, three);
    check("helper21.y", 0.0, helper21.y(0.0), 3.0);
    check("helper21.y", 1.0, helper21.y(1.0), 3.0);
    check("helper21.y", 3.0, helper21.y(3.0), 3.0);
    check("helper21.exp_m_int_y", 0.0, helper21.exp_m_int_y(0.0), 1.0);
    check("helper21.exp_m_int_y", 1.0, helper21.exp_m_int_y(1.0),
          std::exp(-3.0));
    check("helper21.exp_m_int_y", 3.0, helper21.exp_m_int_y(3.0),
          std::exp(-9.0));
    check("helper21.int_exp_m_int_y", 0.0, helper21.int_exp_m_int_y(0.0), 0.0);
    check("helper21.int_exp_m_int_y", 1.0, helper21.int_exp_m_int_y(1.0),
          (1.0 - std::exp(-3.0)) / 3.0);
    check("helper21.int_exp_m_int_y", 3.0, helper21.int_exp_m_int_y(3.0),
          (1.0 - std::exp(-9.0)) / 3.0);

    PiecewiseConstantHelper2 helper22(noTimes, zero);
    check("helper22.y", 0.0, helper22.y(0.0), 0.0);
    check("helper22.y", 1.0, helper22.y(1.0), 0.0);
    check("helper22.y", 3.0, helper22.y(3.0), 0.0);
    check("helper22.exp_m_int_y", 0.0, helper22.exp_m_int_y(0.0), 1.0);
    check("helper22.exp_m_int_y", 1.0, helper22.exp_m_int_y(1.0), 1.0);
    check("helper22.exp_m_int_y", 3.0, helper22.exp_m_int_y(3.0), 1.0);
    check("helper22.int_exp_m_int_y", 0.0, helper22.int_exp_m_int_y(0.0), 0.0);
    check("helper22.int_exp_m_int_y", 1.0, helper22.int_exp_m_int_y(1.0), 1.0);
    check("helper22.int_exp_m_int_y", 3.0, helper22.int_exp_m_int_y(3.0), 3.0);

    Array times(3), values(4);
    times[0] = 1.0;
    times[1] = 2.0;
    times[2] = 3.0;
    values[0] = 1.0;
    values[1] = 2.0;
    values[2] = 0.0;
    values[3] = 3.0;
    PiecewiseConstantHelper1 helper12(times, values);
    check("helper12.y", 0.0, helper12.y(0.0), 1.0);
    check("helper12.y", 0.5, helper12.y(0.5), 1.0);
    check("helper12.y", 1.0, helper12.y(1.0), 2.0);
    check("helper12.y", 2.2, helper12.y(2.2), 0.0);
    check("helper12.y", 3.0 - 1.0E-8, helper12.y(3.0 - 1.0E-8), 0.0);
    check("helper12.y", 3.0, helper12.y(3.0), 3.0);
    check("helper12.y", 5.0, helper12.y(5.0), 3.0);
    check("helper12.int_y_sqr", 0.0, helper12.int_y_sqr(0.0), 0.0);
    check("helper12.int_y_sqr", 0.5, helper12.int_y_sqr(0.5), 0.5);
    check("helper12.int_y_sqr", 1.0, helper12.int_y_sqr(1.0), 1.0);
    check("helper12.int_y_sqr", 1.2, helper12.int_y_sqr(1.2), 1.0 + 4.0 * 0.2);
    check("helper12.int_y_sqr", 2.0, helper12.int_y_sqr(2.0), 1.0 + 4.0);
    check("helper12.int_y_sqr", 2.1, helper12.int_y_sqr(2.1), 1.0 + 4.0);
    check("helper12.int_y_sqr", 2.5, helper12.int_y_sqr(2.5), 1.0 + 4.0);
    check("helper12.int_y_sqr", 2.9, helper12.int_y_sqr(2.9), 1.0 + 4.0);
    check("helper12.int_y_sqr", 3.0, helper12.int_y_sqr(3.0), 1.0 + 4.0);
    check("helper12.int_y_sqr", 5.0, helper12.int_y_sqr(5.0),
          1.0 + 4.0 + 9.0 * 2.0);

    PiecewiseConstantHelper2 helper23(times, values);
    check("helper23.y", 0.0, helper23.y(0.0), 1.0);
    check("helper23.y", 0.5, helper23.y(0.5), 1.0);
    check("helper23.y", 1.0, helper23.y(1.0), 2.0);
    check("helper23.y", 2.2, helper23.y(2.2), 0.0);
    check("helper23.y", 3.0 - 1.0E-8, helper23.y(3.0 - 1.0E-8), 0.0);
    check("helper23.y", 3.0, helper23.y(3.0), 3.0);
    check("helper23.y", 5.0, helper23.y(5.0), 3.0);
    check("helper23.exp_m_int_y", 0.0, helper23.exp_m_int_y(0.0), 1.0);
    check("helper23.exp_m_int_y", 0.5, helper23.exp_m_int_y(0.5),
          std::exp(-0.5));
    check("helper23.exp_m_int_y", 1.0, helper23.exp_m_int_y(1.0),
          std::exp(-1.0));
    check("helper23.exp_m_int_y", 1.5, helper23.exp_m_int_y(1.5),
          std::exp(-2.0));
    check("helper23.exp_m_int_y", 2.0, helper23.exp_m_int_y(2.0),
          std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.1, helper23.exp_m_int_y(2.1),
          std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.5, helper23.exp_m_int_y(2.5),
          std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.9, helper23.exp_m_int_y(2.9),
          std::exp(-3.0));
    check("helper23.exp_m_int_y", 3.0, helper23.exp_m_int_y(3.0),
          std::exp(-3.0));
    check("helper23.exp_m_int_y", 5.0, helper23.exp_m_int_y(5.0),
          std::exp(-3.0 - 6.0));

    check("helper23.int_exp_m_int_y", 0.0, helper23.int_exp_m_int_y(0.0), 0.0);
    Real h = 1.0E-5, x0 = 0.0, sum = 0.0;
    while (x0 < 5.0) {
        sum += h * helper23.exp_m_int_y(x0 + h / 2.0);
        x0 += h;
        check2("helper23.exp_m_int_y2", x0, helper23.int_exp_m_int_y(x0), sum,
               1.0E-10);
    }

    // check update after times / value change

    times[0] = 0.5;
    values[0] = 0.5;
    values[1] = 1.0;
    helper12.update();
    helper23.update();
    check("helper12.y", 0.5, helper12.y(0.5), 1.0);
    check("helper12.int_y_sqr", 1.0, helper12.int_y_sqr(1.0),
          0.5 * 0.5 * 0.5 + 0.5);
    check("helper23.y", 0.5, helper23.y(0.5), 1.0);
    check("helper23.exp_m_int_y", 1.0, helper23.exp_m_int_y(1.0),
          std::exp(-0.75));
}

void XAssetModelTest::testParametrizations() {

    BOOST_TEST_MESSAGE("Testing XAssetModel parametrizations...");

    // test generic inspectors of irlgm1f parametrization

    class IrLgm1fTmpParametrization : public IrLgm1fParametrization {
      public:
        IrLgm1fTmpParametrization(const Currency &currency)
            : IrLgm1fParametrization(currency) {}
        Handle<YieldTermStructure> termStructure() const {
            return Handle<YieldTermStructure>();
        }
        // do not use this parametrization at home
        Real zeta(const Time t) const { return sin(t); }
        Real H(const Time t) const { return t * t * t; }
    } irlgm1f_1((EURCurrency()));

    // check numerical scheme (in particular near zero)

    Real h = 1.0E-8, h2 = 1.0E-4;

    check("irlgm1f_1.alpha", 0.0, irlgm1f_1.alpha(0.0),
          std::sqrt((irlgm1f_1.zeta(h) - irlgm1f_1.zeta(0.0)) / h));
    check("irlgm1f_1.alpha", 0.3E-8, irlgm1f_1.alpha(0.3E-8),
          std::sqrt((irlgm1f_1.zeta(h) - irlgm1f_1.zeta(0.0)) / h));
    check("irlgm1f_1.alpha", 1.0, irlgm1f_1.alpha(1.0),
          std::sqrt(
              (irlgm1f_1.zeta(1.0 + h / 2.0) - irlgm1f_1.zeta(1.0 - h / 2.0)) /
              h));

    check("irlgm1f_1.Hprime", 0.0, irlgm1f_1.Hprime(0.0),
          (irlgm1f_1.H(h) - irlgm1f_1.H(0.0)) / h);
    check("irlgm1f_1.Hprime", 0.3E-8, irlgm1f_1.Hprime(0.3E-8),
          (irlgm1f_1.H(h) - irlgm1f_1.H(0.0)) / h);
    check("irlgm1f_1.Hprime", 1.0, irlgm1f_1.Hprime(1.0),
          (irlgm1f_1.H(1.0 + h / 2.0) - irlgm1f_1.H(1.0 - h / 2.0)) / h);

    check("irlgm1f_1.Hprime2", 0.0, irlgm1f_1.Hprime2(0.0),
          (irlgm1f_1.H(2.0 * h2) - 2.0 * irlgm1f_1.H(h2) + irlgm1f_1.H(0.0)) /
              (h2 * h2));
    check("irlgm1f_1.Hprime2", 0.3E-4, irlgm1f_1.Hprime2(0.3E-4),
          (irlgm1f_1.H(2.0 * h2) - 2.0 * irlgm1f_1.H(h2) + irlgm1f_1.H(0.0)) /
              (h2 * h2));
    check("irlgm1f_1.Hprime2", 1.0, irlgm1f_1.Hprime2(1.0),
          (irlgm1f_1.H(1.0 + h2) - 2.0 * irlgm1f_1.H(1.0) +
           irlgm1f_1.H(1.0 - h2)) /
              (h2 * h2));

    check("irlgm1f_1.hullWhiteSigma", 1.5, irlgm1f_1.hullWhiteSigma(1.5),
          irlgm1f_1.Hprime(1.5) * irlgm1f_1.alpha(1.5));
    check("irlgm1f_1.kappa", 1.5, irlgm1f_1.kappa(1.5),
          -irlgm1f_1.Hprime2(1.5) / irlgm1f_1.Hprime(1.5));

    // check irlgm1f parametrization (piecewise constant and constant)
    // for consistency with sqrt(zeta') = alpha, -H'' / H' = kappa
    // as well, check the Hull White adaptor by checking
    // sqrt(zeta') H' = sigma, -H'' / H' = kappa
    // in all cases we compute the derivatives with a numerical scheme
    // here to ensure that we get out what we put in

    Handle<YieldTermStructure> flatYts(boost::make_shared<FlatForward>(
        0, NullCalendar(), 0.02, Actual365Fixed()));

    IrLgm1fConstantParametrization irlgm1f_2(EURCurrency(), flatYts, 0.01,
                                             0.01);
    IrLgm1fConstantParametrization irlgm1f_3(EURCurrency(), flatYts, 0.01,
                                             0.00);
}

test_suite *XAssetModelTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests");
    suite->add(
        QUANTEXT_TEST_CASE(&XAssetModelTest::testParametrizationBaseClasses));
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testParametrizations));
    return suite;
}
