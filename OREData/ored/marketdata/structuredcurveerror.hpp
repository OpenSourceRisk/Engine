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

/*! \file ored/marketdata/structuredcurveerror.hpp
    \brief Error for market data or curve
    \ingroup marketdata
*/

#pragma once

#include <boost/algorithm/string.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

//! Utility class for Structured Curve errors, contains the curve ID
class StructuredCurveErrorMessage : public StructuredMessage {
public:
    StructuredCurveErrorMessage(const std::string& curveId, const std::string& exceptionType,
                                const std::string& exceptionWhat)
        : StructuredMessage(
              Category::Error, Group::Curve, exceptionWhat,
              std::map<std::string, std::string>({{"exceptionType", exceptionType}, {"curveId", curveId}})) {}
};

class StructuredCurveWarningMessage : public StructuredMessage {
public:
    StructuredCurveWarningMessage(const std::string& curveId, const std::string& exceptionType,
                                  const std::string& exceptionWhat)
        : StructuredMessage(
              Category::Warning, Group::Curve, exceptionWhat,
              std::map<std::string, std::string>({{"exceptionType", exceptionType}, {"curveId", curveId}})) {}
};

} // namespace data
} // namespace ore
