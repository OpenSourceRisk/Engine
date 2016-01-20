/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#ifndef quantext_test_xassetmodel_hpp
#define quantext_test_xassetmodel_hpp

#include <boost/test/unit_test.hpp>

/* remember to document new and/or updated tests in the Doxygen
   comment block of the corresponding class */

class XAssetModelTest {
  public:
    static void testBermudanLgm1fGsr();
    static void testBermudanLgmInvariances();
    static void testLgm1fCalibration();
    static void testCcyLgm3fForeignPayouts();
    static void testLgm5fFxCalibration();
    static void testLgm5fMoments();
    static void testLgmGsrEquivalence();
    static void testLgmMcWithShift();
    static void testCorrelationRecovery();
    static boost::unit_test_framework::test_suite *suite();
};

#endif
