/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#ifndef quantext_test_analyticlgmswaptionengine_hpp
#define quantext_test_analyticlgmswaptionengine_hpp

#include <boost/test/unit_test.hpp>

/* remember to document new and/or updated tests in the Doxygen
   comment block of the corresponding class */

class AnalyticLgmSwaptionEngineTest {
  public:
    static void testMonoCurve();
    static void testDualCurve();
    static void testAgainstOtherEngines();
    static boost::unit_test_framework::test_suite *suite();
};

#endif
