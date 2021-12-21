/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <orea/app/structuredanalyticswarning.hpp>

using std::string;

namespace ore {
namespace analytics {

StructuredAnalyticsWarningMessage::StructuredAnalyticsWarningMessage(const string& analyticType,
                                                                     const string& warningType,
                                                                     const string& warningWhat)
    : analyticType_(analyticType), warningType_(warningType), warningWhat_(warningWhat) {}

const string& StructuredAnalyticsWarningMessage::analyticType() const { return analyticType_; }

const string& StructuredAnalyticsWarningMessage::warningType() const { return warningType_; }

const string& StructuredAnalyticsWarningMessage::warningWhat() const { return warningWhat_; }

string StructuredAnalyticsWarningMessage::json() const {
    return "{ \"errorType\":\"Analytics Warning\", \"analyticType\":\"" + analyticType_ + "\"," +
           " \"warningType\":\"" + warningType_ + "\"," + " \"warningMessage\":\"" + jsonify(warningWhat_) + "\"}";
}

} // namespace analytics
} // namespace ore
