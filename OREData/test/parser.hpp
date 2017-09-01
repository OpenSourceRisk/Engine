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

/*! \file test/parser.hpp
    \brief Parsing test
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test basic parsers
/*!
  \ingroup tests
*/
class ParseTest {
public:
    //! Test parsing of all supported day counter representations
    static void testDayCounterParsing();
    //! Test parsing of all supported frequency representations
    static void testFrequencyParsing();
    //! Test parsing of all supported compounding representations
    static void testCompoundingParsing();
    //! Test parsing of all supported strike representations
    static void testStrikeParsing();
    //! Test date, period, date or period parsing
    static void testDatePeriodParsing();
    //! Test IMM date parsing
    static void testIMMDateParsing();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
