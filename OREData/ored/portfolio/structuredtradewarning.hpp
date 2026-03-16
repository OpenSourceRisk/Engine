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

/*! \file ored/portfolio/structuredtradewarning.hpp
    \brief Classes for structured trade warnings
    \ingroup portfolio
*/

#pragma once

#include <boost/algorithm/string.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

//! Utility classes for Structured warnings, contains the Trade ID and Type
class StructuredTradeWarningMessage : public StructuredMessage {
public:
    StructuredTradeWarningMessage(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, const std::string& warningType,
                                  const std::string& warningWhat)
        : StructuredMessage(
              Category::Warning, Group::Trade, warningWhat,
              std::map<std::string, std::string>(
                  {{"warningType", warningType}, {"tradeId", trade->id()}, {"tradeType", trade->tradeType()}})) {}

    StructuredTradeWarningMessage(const std::string& tradeId, const std::string& tradeType,
                                  const std::string& warningType, const std::string& warningWhat)
        : StructuredMessage(Category::Warning, Group::Trade, warningWhat,
                            std::map<std::string, std::string>(
                                {{"warningType", warningType}, {"tradeId", tradeId}, {"tradeType", tradeType}})) {}
};

} // namespace data
} // namespace ore
