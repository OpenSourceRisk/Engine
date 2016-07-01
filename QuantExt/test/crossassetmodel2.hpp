/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#ifndef quantext_test_crossassetmodel2_hpp
#define quantext_test_crossassetmodel2_hpp

#include <boost/test/unit_test.hpp>

/* remember to document new and/or updated tests in the Doxygen
   comment block of the corresponding class */

namespace testsuite {
    
class CrossAssetModelTest2 {
  public:
    static void testLgm31fPositiveCovariance();
    static void testLgm31fMoments();
    static void testLgm31fMartingaleProperty();
    static boost::unit_test_framework::test_suite *suite();
};

}

#endif
