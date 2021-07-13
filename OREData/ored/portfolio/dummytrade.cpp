/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ored/portfolio/dummytrade.hpp>
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

DummyTrade::DummyTrade()
    : Trade("Dummy") {}

DummyTrade::DummyTrade(const Envelope& env)
    : Trade("Dummy", env) {}

void DummyTrade::build(const boost::shared_ptr<EngineFactory>&) {
    instrument_ = boost::make_shared<VanillaInstrument>(boost::make_shared<NullInstrument>());
    notional_ = 0.0;
    notionalCurrency_ = npvCurrency_ = "USD";
    maturity_ = Date::maxDate();
}

void DummyTrade::fromXML(XMLNode* node) {
    Trade::fromXML(node);
}

XMLNode* DummyTrade::toXML(XMLDocument& doc) {
    return Trade::toXML(doc);
}

}
}
