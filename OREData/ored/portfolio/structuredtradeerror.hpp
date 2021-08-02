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
class StructuredTradeErrorMessage : public StructuredErrorMessage {
public:
    StructuredTradeErrorMessage(const boost::shared_ptr<Trade>& trade, const std::string& exceptionType,
                                const std::string& exceptionWhat = "")
        : tradeId_(trade->id()), tradeType_(trade->tradeType()), exceptionType_(exceptionType),
          exceptionWhat_(exceptionWhat) {}

    StructuredTradeErrorMessage(const std::string& tradeId, const std::string& tradeType,
                                const std::string& exceptionType, const std::string exceptionWhat = "")
        : tradeId_(tradeId), tradeType_(tradeType), exceptionType_(exceptionType), exceptionWhat_(exceptionWhat) {}

    const std::string& tradeId() const { return tradeId_; }
    const std::string& tradeType() const { return tradeType_; }
    const std::string& exceptionType() const { return exceptionType_; }
    const std::string& exceptionWhat() const { return exceptionWhat_; }

protected:
    std::string json() const override {
        return "{ \"errorType\":\"Trade\", \"tradeId\":\"" + tradeId_ + "\"," + " \"tradeType\":\"" + tradeType_ +
               "\"," + " \"exceptionType\":\"" + exceptionType_ + "\"," + " \"exceptionMessage\":\"" +
               jsonify(exceptionWhat_) + "\"}";
    }

private:
    std::string tradeId_, tradeType_, exceptionType_, exceptionWhat_;
};

} // namespace data
} // namespace ore
