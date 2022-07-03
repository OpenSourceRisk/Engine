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

/*! \file test/xmlmanipulation.hpp
    \brief XMl manipulation test
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test XML manipulation
/*!
  \ingroup tests
*/
class XMLManipulationTest {
public:
    //! Test getting nodes, node values, node attributes
    static void testXMLDataGetters();
    //! Test getting vectors of node values
    static void testXMLVectorDataGetters();
    //! Test setting XML data
    static void testXMLDataSetters();
    //! Test writing setting XML data
    static void testXMLAttributes();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
