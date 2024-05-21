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

#include <ored/portfolio/failedtrade.hpp>
#include <qle/instruments/nullinstrument.hpp>

using ore::data::EngineFactory;
using ore::data::Envelope;
using ore::data::VanillaInstrument;
using ore::data::Trade;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using QuantExt::NullInstrument;
using QuantLib::Date;

namespace ore {
namespace data {

FailedTrade::FailedTrade()
    : Trade("Failed") {}

FailedTrade::FailedTrade(const Envelope& env)
    : Trade("Failed", env) {}

void FailedTrade::build(const QuantLib::ext::shared_ptr<EngineFactory>&) {
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(QuantLib::ext::make_shared<NullInstrument>());
    notional_ = 0.0;
    notionalCurrency_ = npvCurrency_ = "USD";
    maturity_ = Date::maxDate();
    setSensitivityTemplate(std::string());
}

void FailedTrade::setUnderlyingTradeType(const std::string& underlyingTradeType) {
    underlyingTradeType_ = underlyingTradeType;
}

const std::string& FailedTrade::underlyingTradeType() const {
    return underlyingTradeType_;
}

void FailedTrade::fromXML(XMLNode* node) {
    Trade::fromXML(node);
}

XMLNode* FailedTrade::toXML(XMLDocument& doc) const {
    return Trade::toXML(doc);
}

}
}
