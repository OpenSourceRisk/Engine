/* 
 Copyright (C) 2019 Quaternion Risk Management Ltd

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

#include <ored/utilities/log.hpp>

#include <functional>
#include <set>

namespace ore {
namespace data {

class FilteredBufferedLogger : public ore::data::BufferLogger {
public:
    //! Constructor
    FilteredBufferedLogger() : ore::data::BufferLogger() {}
    //! The log callback
    virtual void log(unsigned, const std::string&) override;

private:
    std::set<std::size_t> message_hash_history_;
};

//! Utility class to build a logger and remove it from the global logger when it goes out of scope
class FilteredBufferedLoggerGuard {
public:
    boost::shared_ptr<FilteredBufferedLogger> logger;
    FilteredBufferedLoggerGuard() {
        logger = boost::make_shared<FilteredBufferedLogger>();
        ore::data::Log::instance().registerLogger(logger);
    }
    ~FilteredBufferedLoggerGuard() {
        try {
            ore::data::Log::instance().removeLogger(ore::data::BufferLogger::name); // need to use this name
        } catch (...) {}
    }
};

}
}

