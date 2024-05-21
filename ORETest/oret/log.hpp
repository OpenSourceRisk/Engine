/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file oret/log.hpp
    \brief boost test logger
*/

#pragma once

#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>
#include <ored/utilities/log.hpp>

using ore::data::Logger;

namespace ore {
namespace test {

//! BoostTest Logger
/*!
  This logger writes each log message out to the BOOST_TEST_MESSAGE.
  To view log messages run ore unit tests with the flag "--log_level=test_suite"
  \ingroup utilities
  \see Log
 */
class BoostTestLogger : public Logger {
public:
    //! Constructor
    BoostTestLogger() : Logger("BoostTestLogger") {}
    //! The log callback
    virtual void log(unsigned, const std::string& msg) override { BOOST_TEST_MESSAGE(msg); }
};

//! Gets passed the command line arguments from a unit test suite
//! and sets up ORE logging if it is requested
/*!
    Specifying --ore_log_mask on its own turns on logging with a default
    log mask of 255
    Optionally, you can specify the log mask using --ore_log_mask=<mask>
    \ingroup utilities
*/
void setupTestLogging(int argc, char** argv) {

    for (int i = 1; i < argc; ++i) {

        // --ore_log_mask indicates we want ORE logging
        if (boost::starts_with(argv[i], "--ore_log_mask")) {

            // Check if mask is provided also (default is 255)
            unsigned int mask = 255;
            std::vector<std::string> strs;
            boost::split(strs, argv[i], boost::is_any_of("="));
            if (strs.size() > 1) {
                mask = boost::lexical_cast<unsigned int>(strs[1]);
            }

            // Set up logging
            QuantLib::ext::shared_ptr<ore::test::BoostTestLogger> logger = QuantLib::ext::make_shared<ore::test::BoostTestLogger>();
            ore::data::Log::instance().removeAllLoggers();
            ore::data::Log::instance().registerLogger(logger);
            ore::data::Log::instance().switchOn();
            ore::data::Log::instance().setMask(mask);
        }
    }
}

} // namespace test
} // namespace ore
