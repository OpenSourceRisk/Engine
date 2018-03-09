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

#include "crossassetmodelparametrizations.hpp"
#include "utilities.hpp"

#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/models/all.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

namespace {

// check for expected result up to round off errors
void check(const std::string& s, const Real x, const Real y, const Real e, const Size n = 42) {
    if (!close_enough(y, e, n)) {
        BOOST_ERROR("failed to verify " << s << "(" << x << ") = " << e << " up to round off errors, it is " << y
                                        << " instead (difference is " << (y - e) << ", n is " << n << ")");
    }
}

// check for expected result up to truncation errors with absolute tolerance
void check2(const std::string& s, const Real x, const Real y, const Real e, const Real tol) {
    if (std::abs(y - e) > tol) {
        BOOST_ERROR("failed to verify " << s << "(" << x << ") = " << e << ", it is " << y << " instead (difference is "
                                        << (y - e) << ", abs tol=" << tol << ")");
    }
}

} // anonymous namespace

namespace testsuite {

void CrossAssetModelParametrizationsTest::testParametrizationBaseClasses() {

    BOOST_TEST_MESSAGE("Testing CrossAssetModel parametrizations (base classes)...");

    // base class

    Parametrization p1((EURCurrency()));

    if (p1.parameterTimes(42) != Array()) {
        BOOST_ERROR("empty parametrization should have empty times "
                    "array, it has size "
                    << p1.parameterTimes(42).size() << " though");
    }

    if (p1.parameterValues(42) != Array()) {
        BOOST_ERROR("empty parametrization should have empty values "
                    "array, it has size "
                    << p1.parameterValues(42).size() << " though");
    }

    if (p1.parameter(42)->params().size() != 0) {
        BOOST_ERROR("empty parametrization should have empty parameter "
                    "array, it has size "
                    << p1.parameter(42)->params().size() << " though");
    }

    // piecewise constant helpers

    // the helpers expect raw values in the sense of parameter transformation
    // which we generate here hard coded (kind of white box testing, since
    // the helper classes are never used directly in client code)
    Array noTimes;
    PiecewiseConstantHelper1 helper11(noTimes);
    helper11.p()->setParam(0, std::sqrt(3.0));
    check("helper11.y", 0.0, helper11.y(0.0), 3.0);
    check("helper11.y", 1.0, helper11.y(1.0), 3.0);
    check("helper11.y", 3.0, helper11.y(3.0), 3.0);
    check("helper11.int_y_sqr", 0.0, helper11.int_y_sqr(0.0), 0.0);
    check("helper11.int_y_sqr", 1.0, helper11.int_y_sqr(1.0), 9.0);
    check("helper11.int_y_sqr", 3.0, helper11.int_y_sqr(3.0), 27.0);

    PiecewiseConstantHelper2 helper21(noTimes);
    helper21.p()->setParam(0, 3.0);
    check("helper21.y", 0.0, helper21.y(0.0), 3.0);
    check("helper21.y", 1.0, helper21.y(1.0), 3.0);
    check("helper21.y", 3.0, helper21.y(3.0), 3.0);
    check("helper21.exp_m_int_y", 0.0, helper21.exp_m_int_y(0.0), 1.0);
    check("helper21.exp_m_int_y", 1.0, helper21.exp_m_int_y(1.0), std::exp(-3.0));
    check("helper21.exp_m_int_y", 3.0, helper21.exp_m_int_y(3.0), std::exp(-9.0));
    check("helper21.int_exp_m_int_y", 0.0, helper21.int_exp_m_int_y(0.0), 0.0);
    check("helper21.int_exp_m_int_y", 1.0, helper21.int_exp_m_int_y(1.0), (1.0 - std::exp(-3.0)) / 3.0);
    check("helper21.int_exp_m_int_y", 3.0, helper21.int_exp_m_int_y(3.0), (1.0 - std::exp(-9.0)) / 3.0);

    // the helper type 3 is close to type 2, so we only do the easiest
    // tests here, in the irlgm1f Hull White adaptor tests below the
    // other tests will be implicit though
    PiecewiseConstantHelper3 helper31(noTimes, noTimes);
    helper31.p1()->setParam(0, std::sqrt(3.0));
    helper31.p2()->setParam(0, 2.0);
    // helper 3 requires an update()
    helper31.update();
    check("helper31.y1", 0.0, helper31.y1(0.0), 3.0);
    check("helper31.y1", 1.0, helper31.y1(1.0), 3.0);
    check("helper31.y1", 3.0, helper31.y1(3.0), 3.0);
    check("helper31.y2", 0.0, helper31.y2(0.0), 2.0);
    check("helper31.y2", 1.0, helper31.y2(1.0), 2.0);
    check("helper31.y2", 3.0, helper31.y2(3.0), 2.0);
    check("helper31.int_y1_sqr_int_exp_2_int_y2", 0.0, helper31.int_y1_sqr_exp_2_int_y2(0.0), 0.0);
    check("helper31.int_y1_sqr_int_exp_2_int_y2", 1.0, helper31.int_y1_sqr_exp_2_int_y2(1.0),
          9.0 / 4.0 * (std::exp(2.0 * 2.0 * 1.0) - 1.0));
    check("helper31.int_y1_sqr_int_exp_2_int_y2", 3.0, helper31.int_y1_sqr_exp_2_int_y2(3.0),
          9.0 / 4.0 * (std::exp(2.0 * 2.0 * 3.0) - 1.0));

    // test union set of times
    Array times1(2);
    Array times2(3);
    times1[0] = 0.1;
    times1[1] = 0.5;
    times2[0] = 0.2;
    times2[1] = 0.5;
    times2[2] = 1.0;
    PiecewiseConstantHelper3 helper32(times1, times2);
    helper32.p1()->setParam(0, 0.0);
    helper32.p1()->setParam(1, 0.0);
    helper32.p1()->setParam(2, 0.0);
    helper32.p2()->setParam(0, 0.0);
    helper32.p2()->setParam(1, 0.0);
    helper32.p2()->setParam(2, 0.0);
    helper32.p2()->setParam(3, 0.0);
    helper32.update();
    Array exTu(4);
    exTu[0] = 0.1;
    exTu[1] = 0.2;
    exTu[2] = 0.5;
    exTu[3] = 1.0;
    if (helper32.tUnion() != exTu)
        BOOST_ERROR("helper32 expected tUnion array " << exTu << ", but is " << helper32.tUnion());

    PiecewiseConstantHelper2 helper22(noTimes);
    helper22.p()->setParam(0, 0.0);
    check("helper22.y", 0.0, helper22.y(0.0), 0.0);
    check("helper22.y", 1.0, helper22.y(1.0), 0.0);
    check("helper22.y", 3.0, helper22.y(3.0), 0.0);
    check("helper22.exp_m_int_y", 0.0, helper22.exp_m_int_y(0.0), 1.0);
    check("helper22.exp_m_int_y", 1.0, helper22.exp_m_int_y(1.0), 1.0);
    check("helper22.exp_m_int_y", 3.0, helper22.exp_m_int_y(3.0), 1.0);
    check("helper22.int_exp_m_int_y", 0.0, helper22.int_exp_m_int_y(0.0), 0.0);
    check("helper22.int_exp_m_int_y", 1.0, helper22.int_exp_m_int_y(1.0), 1.0);
    check("helper22.int_exp_m_int_y", 3.0, helper22.int_exp_m_int_y(3.0), 3.0);

    Array times(3), values(4), sqrt_values(4);
    times[0] = 1.0;
    times[1] = 2.0;
    times[2] = 3.0;
    values[0] = 1.0;
    values[1] = 2.0;
    values[2] = 0.0;
    values[3] = 3.0;
    sqrt_values[0] = std::sqrt(1.0);
    sqrt_values[1] = std::sqrt(2.0);
    sqrt_values[2] = std::sqrt(0.0);
    sqrt_values[3] = std::sqrt(3.0);
    PiecewiseConstantHelper1 helper12(times);
    helper12.p()->setParam(0, sqrt_values[0]);
    helper12.p()->setParam(1, sqrt_values[1]);
    helper12.p()->setParam(2, sqrt_values[2]);
    helper12.p()->setParam(3, sqrt_values[3]);
    helper12.update();
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
    check("helper12.int_y_sqr", 5.0, helper12.int_y_sqr(5.0), 1.0 + 4.0 + 9.0 * 2.0);

    PiecewiseConstantHelper2 helper23(times);
    helper23.p()->setParam(0, values[0]);
    helper23.p()->setParam(1, values[1]);
    helper23.p()->setParam(2, values[2]);
    helper23.p()->setParam(3, values[3]);
    helper23.update();
    check("helper23.y", 0.0, helper23.y(0.0), 1.0);
    check("helper23.y", 0.5, helper23.y(0.5), 1.0);
    check("helper23.y", 1.0, helper23.y(1.0), 2.0);
    check("helper23.y", 2.2, helper23.y(2.2), 0.0);
    check("helper23.y", 3.0 - 1.0E-8, helper23.y(3.0 - 1.0E-8), 0.0);
    check("helper23.y", 3.0, helper23.y(3.0), 3.0);
    check("helper23.y", 5.0, helper23.y(5.0), 3.0);
    check("helper23.exp_m_int_y", 0.0, helper23.exp_m_int_y(0.0), 1.0);
    check("helper23.exp_m_int_y", 0.5, helper23.exp_m_int_y(0.5), std::exp(-0.5));
    check("helper23.exp_m_int_y", 1.0, helper23.exp_m_int_y(1.0), std::exp(-1.0));
    check("helper23.exp_m_int_y", 1.5, helper23.exp_m_int_y(1.5), std::exp(-2.0));
    check("helper23.exp_m_int_y", 2.0, helper23.exp_m_int_y(2.0), std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.1, helper23.exp_m_int_y(2.1), std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.5, helper23.exp_m_int_y(2.5), std::exp(-3.0));
    check("helper23.exp_m_int_y", 2.9, helper23.exp_m_int_y(2.9), std::exp(-3.0));
    check("helper23.exp_m_int_y", 3.0, helper23.exp_m_int_y(3.0), std::exp(-3.0));
    check("helper23.exp_m_int_y", 5.0, helper23.exp_m_int_y(5.0), std::exp(-3.0 - 6.0));

    check("helper23.int_exp_m_int_y", 0.0, helper23.int_exp_m_int_y(0.0), 0.0);
    Real h = 1.0E-5, x0 = 0.0, sum = 0.0;
    while (x0 < 5.0) {
        sum += h * helper23.exp_m_int_y(x0 + h / 2.0);
        x0 += h;
        check2("helper23.int_exp_m_int_y2", x0, helper23.int_exp_m_int_y(x0), sum, 1.0E-10);
    }

    // check update after value change

    helper12.p()->setParam(0, std::sqrt(0.5));
    helper12.p()->setParam(1, std::sqrt(1.0));
    helper23.p()->setParam(0, 0.5);
    helper23.p()->setParam(1, 1.0);
    helper12.update();
    helper23.update();
    check("update helper12.y", 1.0, helper12.y(1.0), 1.0);
    check("update helper12.int_y_sqr", 2.0, helper12.int_y_sqr(2.0), 0.5 * 0.5 + 1.0 * 1.0);
    check("update helper23.y", 1.0, helper23.y(1.0), 1.0);
    check("update helper23.exp_m_int_y", 2.0, helper23.exp_m_int_y(2.0), std::exp(-0.5 - 1.0));

    // check dates based constructor

    Handle<YieldTermStructure> yts(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()));
    std::vector<Date> dates;
    dates.push_back(yts->referenceDate() + 100);
    dates.push_back(yts->referenceDate() + 200);
    dates.push_back(yts->referenceDate() + 250);
    dates.push_back(yts->referenceDate() + 2385);
    PiecewiseConstantHelper1 helper1x(dates, yts);

    check("time from date helper1x", 0.0, helper1x.t()[0], yts->timeFromReference(dates[0]));
    check("time from date helper1x", 0.0, helper1x.t()[1], yts->timeFromReference(dates[1]));
    check("time from date helper1x", 0.0, helper1x.t()[2], yts->timeFromReference(dates[2]));
    check("time from date helper1x", 0.0, helper1x.t()[3], yts->timeFromReference(dates[3]));
}

void CrossAssetModelParametrizationsTest::testIrLgm1fParametrizations() {

    BOOST_TEST_MESSAGE("Testing CrossAssetModel parametrizations (irlgm1f)...");

    // test generic inspectors of irlgm1f parametrization

    class IrLgm1fTmpParametrization : public IrLgm1fParametrization {
    public:
        IrLgm1fTmpParametrization(const Currency& currency, const Handle<YieldTermStructure>& termStructure)
            : IrLgm1fParametrization(currency, termStructure) {}
        Handle<YieldTermStructure> termStructure() const { return Handle<YieldTermStructure>(); }
        // do not use this parametrization at home
        Real zeta(const Time t) const { return sin(t); }
        Real H(const Time t) const { return t * t * t; }
    } irlgm1f_1((EURCurrency()), Handle<YieldTermStructure>());

    // check numerical differentiation scheme (in particular near zero)
    // of the irlgm1f parametrization

    Real h = 1.0E-6, h2 = 1.0E-4;

    check("irlgm1f_1.alpha", 0.0, irlgm1f_1.alpha(0.0), std::sqrt((irlgm1f_1.zeta(h) - irlgm1f_1.zeta(0.0)) / h));
    check("irlgm1f_1.alpha", 0.3E-8, irlgm1f_1.alpha(0.3E-8), std::sqrt((irlgm1f_1.zeta(h) - irlgm1f_1.zeta(0.0)) / h));
    check("irlgm1f_1.alpha", 1.0, irlgm1f_1.alpha(1.0),
          std::sqrt((irlgm1f_1.zeta(1.0 + h / 2.0) - irlgm1f_1.zeta(1.0 - h / 2.0)) / h));

    check("irlgm1f_1.Hprime", 0.0, irlgm1f_1.Hprime(0.0), (irlgm1f_1.H(h) - irlgm1f_1.H(0.0)) / h);
    check("irlgm1f_1.Hprime", 0.3E-8, irlgm1f_1.Hprime(0.3E-8), (irlgm1f_1.H(h) - irlgm1f_1.H(0.0)) / h);
    check("irlgm1f_1.Hprime", 1.0, irlgm1f_1.Hprime(1.0),
          (irlgm1f_1.H(1.0 + h / 2.0) - irlgm1f_1.H(1.0 - h / 2.0)) / h);

    check("irlgm1f_1.Hprime2", 0.0, irlgm1f_1.Hprime2(0.0),
          (irlgm1f_1.H(2.0 * h2) - 2.0 * irlgm1f_1.H(h2) + irlgm1f_1.H(0.0)) / (h2 * h2));
    check("irlgm1f_1.Hprime2", 0.3E-4, irlgm1f_1.Hprime2(0.3E-4),
          (irlgm1f_1.H(2.0 * h2) - 2.0 * irlgm1f_1.H(h2) + irlgm1f_1.H(0.0)) / (h2 * h2));
    check("irlgm1f_1.Hprime2", 1.0, irlgm1f_1.Hprime2(1.0),
          (irlgm1f_1.H(1.0 + h2) - 2.0 * irlgm1f_1.H(1.0) + irlgm1f_1.H(1.0 - h2)) / (h2 * h2));

    check("irlgm1f_1.hullWhiteSigma", 1.5, irlgm1f_1.hullWhiteSigma(1.5), irlgm1f_1.Hprime(1.5) * irlgm1f_1.alpha(1.5));
    check("irlgm1f_1.kappa", 1.5, irlgm1f_1.kappa(1.5), -irlgm1f_1.Hprime2(1.5) / irlgm1f_1.Hprime(1.5));

    // check the irlgm1f parametrizations

    Handle<YieldTermStructure> flatYts(boost::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    IrLgm1fConstantParametrization irlgm1f_2(EURCurrency(), flatYts, 0.01, 0.01);
    IrLgm1fConstantParametrization irlgm1f_3(EURCurrency(), flatYts, 0.01, 0.00);

    Array alphaTimes(99), kappaTimes(99), alpha(100), kappa(100), sigma(100);
    for (Size i = 0; i < 100; ++i) {
        if (i < 99) {
            alphaTimes[i] = static_cast<Real>(i + 1);
            kappaTimes[i] = alphaTimes[i];
        }
        // 0.0000 to 0.099
        alpha[i] = sigma[i] = static_cast<Real>(i) * 0.0010;
        // -0.05 to 0.049
        kappa[i] = (static_cast<Real>(i) - 50.0) * 0.001;
    }

    IrLgm1fPiecewiseConstantParametrization irlgm1f_4(EURCurrency(), flatYts, alphaTimes, alpha, kappaTimes, kappa);

    // alpha and kappa times are identical
    IrLgm1fPiecewiseConstantHullWhiteAdaptor irlgm1f_5(EURCurrency(), flatYts, alphaTimes, sigma, alphaTimes, kappa);

    Real t = 0.0, step = 1.0E-3;
    while (t < 100.0) {

        // check irlgm1f parametrization (piecewise constant and constant)
        // for consistency with sqrt(zeta') = alpha and -H'' / H' = kappa

        // as well, check the Hull White adaptor by checking
        // sqrt(zeta') H' = sigma, -H'' / H' = kappa

        Real zetaPrime2, zetaPrime3, zetaPrime4, zetaPrime5;
        Real Hprime2, Hprime3, Hprime4, Hprime5;
        Real Hprimeprime2, Hprimeprime3, Hprimeprime4, Hprimeprime5;
        if (t < h / 2.0) {
            zetaPrime2 = (irlgm1f_2.zeta(t + h) - irlgm1f_2.zeta(t)) / h;
            zetaPrime3 = (irlgm1f_3.zeta(t + h) - irlgm1f_3.zeta(t)) / h;
            zetaPrime4 = (irlgm1f_4.zeta(t + h) - irlgm1f_4.zeta(t)) / h;
            zetaPrime5 = (irlgm1f_5.zeta(t + h) - irlgm1f_5.zeta(t)) / h;
            Hprime2 = (irlgm1f_2.H(t + h) - irlgm1f_2.H(t)) / h;
            Hprime3 = (irlgm1f_3.H(t + h) - irlgm1f_3.H(t)) / h;
            Hprime4 = (irlgm1f_4.H(t + h) - irlgm1f_4.H(t)) / h;
            Hprime5 = (irlgm1f_5.H(t + h) - irlgm1f_5.H(t)) / h;
        } else {
            zetaPrime2 = (irlgm1f_2.zeta(t + h / 2.0) - irlgm1f_2.zeta(t - h / 2.0)) / h;
            zetaPrime3 = (irlgm1f_3.zeta(t + h / 2.0) - irlgm1f_3.zeta(t - h / 2.0)) / h;
            zetaPrime4 = (irlgm1f_4.zeta(t + h / 2.0) - irlgm1f_4.zeta(t - h / 2.0)) / h;
            zetaPrime5 = (irlgm1f_5.zeta(t + h / 2.0) - irlgm1f_5.zeta(t - h / 2.0)) / h;
            Hprime2 = (irlgm1f_2.H(t + h / 2.0) - irlgm1f_2.H(t - h / 2.0)) / h;
            Hprime3 = (irlgm1f_3.H(t + h / 2.0) - irlgm1f_3.H(t - h / 2.0)) / h;
            Hprime4 = (irlgm1f_4.H(t + h / 2.0) - irlgm1f_4.H(t - h / 2.0)) / h;
            Hprime5 = (irlgm1f_5.H(t + h / 2.0) - irlgm1f_5.H(t - h / 2.0)) / h;
        }
        if (t < h2) {
            Hprimeprime2 = (irlgm1f_2.H(2.0 * h2) - 2.0 * irlgm1f_2.H(h2) + irlgm1f_2.H(0.0)) / (h2 * h2);
            Hprimeprime3 = (irlgm1f_3.H(2.0 * h2) - 2.0 * irlgm1f_3.H(h2) + irlgm1f_3.H(0.0)) / (h2 * h2);
            Hprimeprime4 = (irlgm1f_4.H(2.0 * h2) - 2.0 * irlgm1f_4.H(h2) + irlgm1f_4.H(0.0)) / (h2 * h2);
            Hprimeprime5 = (irlgm1f_5.H(2.0 * h2) - 2.0 * irlgm1f_5.H(h2) + irlgm1f_5.H(0.0)) / (h2 * h2);
        } else {
            Hprimeprime2 = (irlgm1f_2.H(t + h2) - 2.0 * irlgm1f_2.H(t) + irlgm1f_2.H(t - h2)) / (h2 * h2);
            Hprimeprime3 = (irlgm1f_3.H(t + h2) - 2.0 * irlgm1f_3.H(t) + irlgm1f_3.H(t - h2)) / (h2 * h2);
            Hprimeprime4 = (irlgm1f_4.H(t + h2) - 2.0 * irlgm1f_4.H(t) + irlgm1f_4.H(t - h2)) / (h2 * h2);
            Hprimeprime5 = (irlgm1f_5.H(t + h2) - 2.0 * irlgm1f_5.H(t) + irlgm1f_5.H(t - h2)) / (h2 * h2);
        }
        check2("sqrt(d/dt irlgm1f_2.zeta)", t, sqrt(zetaPrime2), 0.01, 1.0E-7);
        check2("sqrt(d/dt irlgm1f_3.zeta)", t, sqrt(zetaPrime3), 0.01, 1.0E-7);
        if (std::fabs(t - static_cast<int>(t + 0.5)) > h) {
            // we can not expect this test to work when the numerical
            // differentiation is going over a grid point where
            // alpha (or sigma) jumps
            check2("sqrt(d/dt irlgm1f_4.zeta)", t, sqrt(zetaPrime4), QL_PIECEWISE_FUNCTION(alphaTimes, alpha, t),
                   1.0E-7);
            check2("sqrt(d/dt irlgm1f_5.zeta)*H'", t, sqrt(zetaPrime5) * Hprime5,
                   QL_PIECEWISE_FUNCTION(alphaTimes, sigma, t), 1.0E-6);
        }
        check2("irlgm1f_2.(-H''/H')", t, -Hprimeprime2 / Hprime2, 0.01, 2.0E-5);
        check2("irlgm1f_3.(-H''/H')", t, -Hprimeprime3 / Hprime3, 0.00, 2.0E-5);
        if (std::fabs(t - static_cast<int>(t + 0.5)) > h2) {
            // same as above, we avoid to test the grid points
            check2("irlgm1f_4.(-H''/H')", t, -Hprimeprime4 / Hprime4, QL_PIECEWISE_FUNCTION(kappaTimes, kappa, t),
                   5.0E-5);
            check2("irlgm1f_5.(-H''/H')", t, -Hprimeprime5 / Hprime5, QL_PIECEWISE_FUNCTION(alphaTimes, kappa, t),
                   5.0E-5);
        }

        // check the remaining inspectors

        check("irlgm1f_2.alpha", t, irlgm1f_2.alpha(t), 0.01);
        check("irlgm1f_3.alpha", t, irlgm1f_3.alpha(t), 0.01);
        check("irlgm1f_4.alpha", t, irlgm1f_4.alpha(t), QL_PIECEWISE_FUNCTION(alphaTimes, alpha, t));
        check("irlgm1f_5.hullWhiteSigma", t, irlgm1f_5.hullWhiteSigma(t), QL_PIECEWISE_FUNCTION(alphaTimes, sigma, t));

        check("irlgm1f_2.kappa", t, irlgm1f_2.kappa(t), 0.01);
        check("irlgm1f_3.kappa", t, irlgm1f_3.kappa(t), 0.00);
        check("irlgm1f_4.kappa", t, irlgm1f_4.kappa(t), QL_PIECEWISE_FUNCTION(kappaTimes, kappa, t));
        check("irlgm1f_5.kappa", t, irlgm1f_5.kappa(t), QL_PIECEWISE_FUNCTION(alphaTimes, kappa, t));

        check2("irlgm1f_2.Hprime", t, irlgm1f_2.Hprime(t), Hprime2, 1.0E-6);
        check2("irlgm1f_3.Hprime", t, irlgm1f_3.Hprime(t), Hprime3, 1.0E-6);
        if (std::fabs(t - static_cast<int>(t + 0.5)) > h) {
            // same as above, we avoid to test the grid points
            check2("irlgm1f_4.Hprime", t, irlgm1f_4.Hprime(t), Hprime4, 1.0E-6);
            check2("irlgm1f_5.Hprime", t, irlgm1f_5.Hprime(t), Hprime5, 1.0E-6);
            check2("irlgm1f_5.alpha", t, irlgm1f_5.alpha(t),
                   QL_PIECEWISE_FUNCTION(alphaTimes, sigma, t) / irlgm1f_5.Hprime(t), 1.0E-6);
        }

        check2("irlgm1f_2.Hprime2", t, irlgm1f_2.Hprime2(t), Hprimeprime2, 2.0E-5);
        check2("irlgm1f_3.Hprime2", t, irlgm1f_3.Hprime2(t), Hprimeprime3, 2.0E-5);
        if (std::fabs(t - static_cast<int>(t + 0.5)) > h) {
            // same as above, we avoid to test the grid points
            check2("irlgm1f_4.Hprime2", t, irlgm1f_4.Hprime2(t), Hprimeprime4, 2.0E-3);
            check2("irlgm1f_5.Hprime2", t, irlgm1f_5.Hprime2(t), Hprimeprime5, 2.0E-3);
        }

        check2("irlgm1f_2.hullWhiteSigma", t, irlgm1f_2.hullWhiteSigma(t), 0.01 * Hprime2, 1.0E-7);
        check2("irlgm1f_3.hullWhiteSigma", t, irlgm1f_3.hullWhiteSigma(t), 0.01 * Hprime3, 1.0E-7);
        check2("irlgm1f_4.hullWhiteSigma", t, irlgm1f_4.hullWhiteSigma(t),
               QL_PIECEWISE_FUNCTION(alphaTimes, alpha, t) * Hprime4, 1.0E-7);
        // irlgm1f_5.alpha check is above if you should have wondered ...

        t += step;
    }
}

void CrossAssetModelParametrizationsTest::testFxBsParametrizations() {

    BOOST_TEST_MESSAGE("Testing CrossAssetModel parametrizations (fxbs)...");

    FxBsConstantParametrization fxbs_0(USDCurrency(), Handle<Quote>(boost::make_shared<SimpleQuote>(1.10)), 0.10);

    check("fxbs_0.variance", 0.0, fxbs_0.variance(0.0), 0.0);
    check("fxbs_0.variance", 1.0, fxbs_0.variance(1.0), 0.01 * 1.0);
    check("fxbs_0.variance", 2.0, fxbs_0.variance(2.0), 0.01 * 2.0);
    check("fxbs_0.variance", 3.0, fxbs_0.variance(3.0), 0.01 * 3.0);
    check("fxbs_0.stdDeviation", 0.0, fxbs_0.stdDeviation(0.0), 0.0);
    check("fxbs_0.stdDeviation", 1.0, fxbs_0.stdDeviation(1.0), std::sqrt(0.01 * 1.0));
    check("fxbs_0.stdDeviation", 2.0, fxbs_0.stdDeviation(2.0), std::sqrt(0.01 * 2.0));
    check("fxbs_0.stdDeviation", 3.0, fxbs_0.stdDeviation(3.0), std::sqrt(0.01 * 3.0));
    check("fxbs_0.sigma", 0.0, fxbs_0.sigma(0.0), 0.10);
    check("fxbs_0.sigma", 1.0, fxbs_0.sigma(1.0), 0.10);
    check("fxbs_0.sigma", 2.0, fxbs_0.sigma(2.0), 0.10);
    check("fxbs_0.sigma", 3.0, fxbs_0.sigma(3.0), 0.10);

    Array times(3), sigma(4);
    times[0] = 1.0;
    times[1] = 2.0;
    times[2] = 3.0;
    sigma[0] = 0.10;
    sigma[1] = 0.20;
    sigma[2] = 0.0;
    sigma[3] = 0.15;

    FxBsPiecewiseConstantParametrization fxbs_1(USDCurrency(), Handle<Quote>(boost::make_shared<SimpleQuote>(1.10)),
                                                times, sigma);

    check("fxbs_1.variance", 0.0, fxbs_1.variance(0.0), 0.0);
    check("fxbs_1.variance", 0.5, fxbs_1.variance(0.5), 0.10 * 0.10 * 0.5);
    check("fxbs_1.variance", 1.0, fxbs_1.variance(1.0), 0.10 * 0.10);
    check("fxbs_1.variance", 1.5, fxbs_1.variance(1.5), 0.10 * 0.10 + 0.20 * 0.20 * 0.5);
    check("fxbs_1.variance", 2.0, fxbs_1.variance(2.0), 0.10 * 0.10 + 0.20 * 0.20);
    check("fxbs_1.variance", 2.2, fxbs_1.variance(2.2), 0.10 * 0.10 + 0.20 * 0.20);
    check("fxbs_1.variance", 3.0, fxbs_1.variance(3.0), 0.10 * 0.10 + 0.20 * 0.20);
    check("fxbs_1.variance", 5.0, fxbs_1.variance(5.0), 0.10 * 0.10 + 0.20 * 0.20 + 2 * 0.15 * 0.15);

    check("fxbs_1.stdDeviation", 0.0, fxbs_1.stdDeviation(0.0), std::sqrt(0.0));
    check("fxbs_1.stdDeviation", 0.5, fxbs_1.stdDeviation(0.5), std::sqrt(0.10 * 0.10 * 0.5));
    check("fxbs_1.stdDeviation", 1.0, fxbs_1.stdDeviation(1.0), std::sqrt(0.10 * 0.10));
    check("fxbs_1.stdDeviation", 1.5, fxbs_1.stdDeviation(1.5), std::sqrt(0.10 * 0.10 + 0.20 * 0.20 * 0.5));
    check("fxbs_1.stdDeviation", 2.0, fxbs_1.stdDeviation(2.0), std::sqrt(0.10 * 0.10 + 0.20 * 0.20));
    check("fxbs_1.stdDeviation", 2.2, fxbs_1.stdDeviation(2.2), std::sqrt(0.10 * 0.10 + 0.20 * 0.20));
    check("fxbs_1.stdDeviation", 3.0, fxbs_1.stdDeviation(3.0), std::sqrt(0.10 * 0.10 + 0.20 * 0.20));
    check("fxbs_1.stdDeviation", 5.0, fxbs_1.stdDeviation(5.0), std::sqrt(0.10 * 0.10 + 0.20 * 0.20 + 2 * 0.15 * 0.15));

    check("fxb2_1.sigma", 0.0, fxbs_1.sigma(0.0), 0.10);
    check("fxb2_1.sigma", 0.5, fxbs_1.sigma(0.5), 0.10);
    check("fxb2_1.sigma", 1.0, fxbs_1.sigma(1.0), 0.20);
    check("fxb2_1.sigma", 2.0, fxbs_1.sigma(2.0), 0.00);
    check("fxb2_1.sigma", 3.0, fxbs_1.sigma(3.0), 0.15);
    check("fxb2_1.sigma", 5.0, fxbs_1.sigma(5.0), 0.15);
}

test_suite* CrossAssetModelParametrizationsTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CrossAsset model parametrizations tests");
    suite->add(BOOST_TEST_CASE(&CrossAssetModelParametrizationsTest::testParametrizationBaseClasses));
    suite->add(BOOST_TEST_CASE(&CrossAssetModelParametrizationsTest::testIrLgm1fParametrizations));
    suite->add(BOOST_TEST_CASE(&CrossAssetModelParametrizationsTest::testFxBsParametrizations));
    return suite;
}
} // namespace testsuite
