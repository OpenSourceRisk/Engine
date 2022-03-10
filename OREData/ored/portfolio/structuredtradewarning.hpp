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
class StructuredTradeWarningMessage : public StructuredErrorMessage {
public:
    StructuredTradeWarningMessage(const boost::shared_ptr<Trade>& trade, const std::string& warningType,
                                  const std::string& warningWhat)
        : tradeId_(trade->id()), tradeType_(trade->tradeType()), warningType_(warningType), warningWhat_(warningWhat) {}

    StructuredTradeWarningMessage(const std::string& tradeId, const std::string& tradeType,
                                  const std::string& warningType, const std::string warningWhat)
        : tradeId_(tradeId), tradeType_(tradeType), warningType_(warningType), warningWhat_(warningWhat) {}

    const std::string& tradeId() const { return tradeId_; }
    const std::string& tradeType() const { return tradeType_; }
    const std::string& warningType() const { return warningType_; }
    const std::string& warningWhat() const { return warningWhat_; }

protected:
    std::string json() const override {
        return "{ \"errorType\":\"Trade Warning\", \"tradeId\":\"" + tradeId_ + "\"," + " \"tradeType\":\"" +
               tradeType_ + "\"," + " \"warningType\":\"" + warningType_ + "\"," + " \"warningMessage\":\"" +
               jsonify(warningWhat_) + "\"}";
    }

private:
    std::string tradeId_, tradeType_, warningType_, warningWhat_;
};

} // namespace data
} // namespace ore
