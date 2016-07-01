/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/index.hpp
  \brief ibor index tests
*/

#ifndef quantext_test_index_hpp
#define quantext_test_index_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! Ibor index tests
/*!
  \ingroup tests
*/
class IndexTest {
  public:
    static void testIborIndex();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
