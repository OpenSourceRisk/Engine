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

/*! \file ored/portfolio/structuredtradeerror.hpp
    \brief Structured Trade Error class
    \ingroup portfolio
*/

#pragma once

#include <boost/algorithm/string.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

//! Utility class for Structured Trade errors, contains the Trade ID and Type
class StructuredTradeErrorMessage : public StructuredMessage {
public:
    StructuredTradeErrorMessage(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, const std::string& exceptionType,
                                const std::string& exceptionWhat)
        : StructuredMessage(
              Category::Error, Group::Trade, exceptionWhat,
              std::map<std::string, std::string>(
                  {{"exceptionType", exceptionType}, {"tradeId", trade->id()}, {"tradeType", trade->tradeType()}})) {}

    StructuredTradeErrorMessage(const std::string& tradeId, const std::string& tradeType,
                                const std::string& exceptionType, const std::string& exceptionWhat)
        : StructuredMessage(Category::Error, Group::Trade, exceptionWhat,
                            std::map<std::string, std::string>(
                                {{"exceptionType", exceptionType}, {"tradeId", tradeId}, {"tradeType", tradeType}})) {}
};

} // namespace data
} // namespace ore
