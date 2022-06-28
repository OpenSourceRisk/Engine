/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/app/structuredanalyticserror.hpp>

using std::string;

namespace ore {
namespace analytics {

StructuredAnalyticsErrorMessage::StructuredAnalyticsErrorMessage(const string& type, const string& what)
    : type_(type), what_(what) {}

const string& StructuredAnalyticsErrorMessage::type() const { return type_; }

const string& StructuredAnalyticsErrorMessage::what() const { return what_; }

string StructuredAnalyticsErrorMessage::json() const {
    return "{ \"errorType\":\"Analytics\", \"exceptionType\":\"" + type_ + "\"," + " \"exceptionMessage\":\"" +
           jsonify(what_) + "\"}";
}

} // namespace analytics
} // namespace ore
