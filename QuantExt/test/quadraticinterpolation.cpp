/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
 #include <boost/test/unit_test.hpp>
 #include <ql/termstructures/yield/discountcurve.hpp>
 #include <qle/math/logquadraticinterpolation.hpp>

 #include <boost/make_shared.hpp>

 using namespace boost::unit_test_framework;
 using namespace QuantExt;
 using namespace QuantLib;

 BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

 BOOST_AUTO_TEST_SUITE(QuadraticInterpolationTest)

 BOOST_AUTO_TEST_CASE(testQuadraticInterpolation) {

     BOOST_TEST_MESSAGE("Testing QuantExt Log-/QuadraticInterpolation");

     std::vector<Time> t = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
     std::vector<Real> l = {
         0.00747391, 0.00755479, 0.0185543, 0.0228824, 0.0237791,
         0.0252718, 0.0338208, 0.0391517, 0.0395441, 0.0495399
     };

     std::vector<Real> expected_lambda = {
         0.103514, // lambda_0
         59.2299, -82.6916, 56.1263, -16.8334, 15.9315,
         -32.2953, 12.8415, 32.2534, -44.2813,
         16.9836 // lambda_N
     };

     std::vector<DiscountFactor> dfs(l.size(), 0.0);
     for (Size i = 0; i < l.size(); ++i) {
         dfs[i] = std::exp(-l[i]);
     }
     LogQuadraticInterpolation q(t.begin(), t.end(), dfs.begin(), 1, 0, -1, 0);

     BOOST_TEST_MESSAGE("Interpolation should be exact at pillars");
     for (Size i = 0; i < l.size(); ++i) {
         BOOST_CHECK_CLOSE(q(t[i]), dfs[i], 0.0001);
     }

     BOOST_TEST_MESSAGE("Test calculated lambdas against cached values");
     std::vector<Real> calculated_lambdas = q.lambdas<
         std::vector<QuantLib::Real>::iterator,
         std::vector<QuantLib::Real>::iterator>();
     for (Size i = 0; i < calculated_lambdas.size(); ++i) {
         BOOST_CHECK_CLOSE(calculated_lambdas[i], expected_lambda[i], 0.01);
     }

     BOOST_TEST_MESSAGE("Test lambdas consistency");
     Real expected_lambda_N = 0;
     for (Size i = 1; i < calculated_lambdas.size() - 1; ++i) {
         expected_lambda_N += -calculated_lambdas[i] * t[i - 1] / t.back();
     }
     BOOST_CHECK_CLOSE(expected_lambda_N, expected_lambda.back(), 0.1);

     BOOST_TEST_MESSAGE("Test interpolated values against cached values");
     std::vector<Real> expected_df = {
         0.992554, 0.992500, 0.992798, 0.993115, 0.993118,
         0.992474, 0.990959, 0.988792, 0.986298, 0.983799,
         0.981617, 0.979994, 0.978880, 0.978154, 0.977693,
         0.977377, 0.977108, 0.976875, 0.976689, 0.976560,
         0.976501, 0.976502, 0.976470, 0.976292, 0.975854,
         0.975045, 0.973794, 0.972198, 0.970399, 0.968535,
         0.966745, 0.965148, 0.963798, 0.962733, 0.961989,
         0.961605, 0.961576, 0.961734, 0.961868, 0.961770,
         0.961228, 0.960090, 0.958433, 0.956389, 0.954090,
         0.951667
     };

     Time j = t.front();
     for (Size i = 0; i < expected_df.size(); ++i) {
         BOOST_CHECK_CLOSE(q(j), expected_df[i], 1e-4);
         j += 0.02;
     }

 }

 BOOST_AUTO_TEST_CASE(testInterpolatedDiscountCurve) {

     BOOST_TEST_MESSAGE("Testing QuantExt "
                        "InterpolatedDiscountCurve<LogQuadratic>");

     SavedSettings backup;
     Settings::instance().evaluationDate() = Date(8, Dec, 2016);
     Date today = Settings::instance().evaluationDate();

     std::vector<Date> dates = {
         Date(8, Dec, 2016), Date(8, Jun, 2017),
         Date(8, Dec, 2017), Date(8, Dec, 2018)
     };
     std::vector<DiscountFactor> dfs = { 1.00, 0.99, 0.95, 0.97 };
     std::vector<Real> params = { 1, 0.01, -1, 0.01 };

     InterpolatedDiscountCurve<LogQuadratic> curve(dates, dfs,
         Actual365Fixed(), LogQuadratic(params[0], params[1],
                                        params[2], params[3]));

     std::vector<Time> times(dates.size());
     for(Size i = 0; i < dates.size(); ++i)
         times[i] = Actual365Fixed().yearFraction(today, dates[i]);
     LogQuadraticInterpolation q(times.begin(), times.end(), dfs.begin(),
                                 params[0], params[1], params[2], params[3]);

     BOOST_TEST_MESSAGE("Interpolation should be exact at pillars");
     for (Size i = 0; i < dfs.size(); ++i) {
         BOOST_CHECK_CLOSE(curve.discount(dates[i]), dfs[i], 0.0000001);
         BOOST_CHECK_CLOSE(curve.discount(times[i]), dfs[i], 0.0000001);
     }

     BOOST_TEST_MESSAGE("Test lambdas consistency between "
                        "InterpolatedDiscountCurve<LogQuadratic> "
                        "and LogQuadraticInterpolation");
     for (Time i = 0; i < times.back(); i += 0.01) {
         BOOST_CHECK_CLOSE(q(i, true), curve.discount(i, true), 0.0001);
     }
 }
 BOOST_AUTO_TEST_SUITE_END()

 BOOST_AUTO_TEST_SUITE_END()
