/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file test/indices.hpp
    \brief Index name parsing test
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test index parsing
/*!
  \ingroup tests
*/
class IndexTest {
public:
    //! Test ibor index parsing, correctness of the resulting QuantLib index' name
    static void testIborIndexParsing();
    //! Test that parsing of non-existing currency-name-tenor combinations fails
    static void testIborIndexParsingFails();
    //! Test G5 swap index parsing, correctness of the resulting QuantLib index' name
    static void testSwapIndexParsing();
    static boost::unit_test_framework::test_suite* suite();
};
}
