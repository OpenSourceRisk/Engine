/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
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

void FailedTrade::build(const boost::shared_ptr<EngineFactory>&) {
    instrument_ = boost::make_shared<VanillaInstrument>(boost::make_shared<NullInstrument>());
    notional_ = 0.0;
    notionalCurrency_ = npvCurrency_ = "USD";
    maturity_ = Date::maxDate();
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

XMLNode* FailedTrade::toXML(XMLDocument& doc) {
    return Trade::toXML(doc);
}

}
}
