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

#include "aggregationscenariodata.hpp"

using namespace openriskengine::analytics;
using namespace boost::unit_test_framework;

namespace testsuite {

void AggregationScenarioDataTest::testInMemoryAggregationScenarioData() {
    InMemoryAggregationScenarioData data(3, 5);

    // write data
    BOOST_CHECK_THROW(data.set(3, 0, 0.0, AggregationScenarioDataType::Generic, "blabla"), std::exception);
    BOOST_CHECK_THROW(data.set(0, 5, 0.0, AggregationScenarioDataType::Generic, "blabla"), std::exception);

    for (Size i = 0; i < 3; ++i) {
        for (Size j = 0; j < 5; ++j) {
            data.set(i, j, 0.0001 * i + 0.01 * j, AggregationScenarioDataType::IndexFixing, "OIS_EUR");
            data.set(i, j, 0.1 + 0.0001 * i + 0.01 * j, AggregationScenarioDataType::IndexFixing, "OIS_USD");
            data.set(i, j, 0.2 + 0.0001 * i + 0.01 * j, AggregationScenarioDataType::IndexFixing, "OIS_GBP");
            data.set(i, j, i + 0.1 * j, AggregationScenarioDataType::FXSpot, "EURUSD");
            data.set(i, j, 2.0 + i + 0.1 * j, AggregationScenarioDataType::FXSpot, "EURGBP");
        }
    }

    // read data
    BOOST_CHECK_THROW(data.get(3, 0, AggregationScenarioDataType::Generic, "blabla"), std::exception);
    BOOST_CHECK_THROW(data.get(0, 5, AggregationScenarioDataType::Generic, "blabla"), std::exception);

    Real tol = 1.0E-12;

    for (Size i = 0; i < 3; ++i) {
        for (Size j = 0; j < 5; ++j) {
            BOOST_CHECK_CLOSE(data.get(i, j, AggregationScenarioDataType::IndexFixing, "OIS_EUR"), 0.0001 * i + 0.01 * j,
                              tol);
            BOOST_CHECK_CLOSE(data.get(i, j, AggregationScenarioDataType::IndexFixing, "OIS_USD"),
                              0.1 + 0.0001 * i + 0.01 * j, tol);
            BOOST_CHECK_CLOSE(data.get(i, j, AggregationScenarioDataType::IndexFixing, "OIS_GBP"),
                              0.2 + 0.0001 * i + 0.01 * j, tol);
            BOOST_CHECK_CLOSE(data.get(i, j, AggregationScenarioDataType::FXSpot, "EURUSD"), i + 0.1 * j, tol);
            BOOST_CHECK_CLOSE(data.get(i, j, AggregationScenarioDataType::FXSpot, "EURGBP"), 2.0 + i + 0.1 * j, tol);
        }
    }
}

test_suite* AggregationScenarioDataTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Additional scenario data tests");

    suite->add(BOOST_TEST_CASE(&AggregationScenarioDataTest::testInMemoryAggregationScenarioData));

    return suite;
}
}
