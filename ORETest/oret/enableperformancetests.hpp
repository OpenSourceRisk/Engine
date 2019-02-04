/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file oret/enableperformancetests.hpp
    \brief Check whether to enable performance tests
*/

#pragma once

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <cstring>

namespace ore {
namespace test {

/*! Check whether to enable performance tests */

struct enablePerformanceTests {
    boost::test_tools::assertion_result operator()(boost::unit_test::test_unit_id) {
        const auto& master = boost::unit_test::framework::master_test_suite();
        int argc = master.argc;
        char** argv = master.argv;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--enable_performance_tests") == 0)
                return true;
        }
        return false;
    }
};

} // namespace test
} // namespace ore
