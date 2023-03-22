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

// #include "stdafx.h"

#include <ored/utilities/filteredbufferedlogger.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <string.h>

using std::string;

namespace ore {
namespace data {

void FilteredBufferedLogger::log(unsigned lvl, const std::string& log) {
    // Only takes ALRERTs (filters out Warning "Failed to build Curve"
    if (lvl != ORE_ALERT &&
        lvl != ORE_CRITICAL &&
        lvl != ORE_ERROR &&
        lvl != ORE_WARNING)
        return;

    // Search for StructuredErrorMessage and take everything to the right of that
    // Do not log duplicates of the same message
    size_t n = log.find(ore::data::StructuredMessage::name);
    if (n != string::npos) {
        // Strip it out
        string log_message = log.substr(n + strlen(ore::data::StructuredMessage::name));
        // check if we have already logged the mssage
        std::size_t message_hash = std::hash<std::string>{}(log_message);
        if (message_hash_history_.insert(message_hash).second) {
            // log it
            BufferLogger::log(lvl, log_message);
        }
    }
}

}
}
