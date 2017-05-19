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

#pragma once

#include <orea/scenario/aggregationscenariodata.hpp>

#include <boost/test/unit_test.hpp>

namespace testsuite {
//! Test storing and retrieving 'additional scenario data'
/*!
  This object is used in the core valuation engine during
  scenario market updates to store index fixings and FX rates
  along paths (per date and sample).
  This subset of scenario data is needed in the postprocessor
  to compound and convert collateral amounts.
 */
class AggregationScenarioDataTest {
public:
    //! Test storing and retrieving a few artificial data points
    static void testInMemoryAggregationScenarioData();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
