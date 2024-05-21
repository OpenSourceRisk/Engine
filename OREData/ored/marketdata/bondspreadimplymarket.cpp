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

#include <ored/marketdata/bondspreadimplymarket.hpp>

namespace ore {
namespace data {

BondSpreadImplyMarket::BondSpreadImplyMarket(const QuantLib::ext::shared_ptr<Market>& market, const bool handlePseudoCurrencies)
    : WrappedMarket(market, handlePseudoCurrencies) {}

QuantLib::ext::shared_ptr<SimpleQuote> BondSpreadImplyMarket::spreadQuote(const string& securityID) const {
    if (auto s = spreadQuote_.find(securityID); s != spreadQuote_.end())
        return s->second;
    auto q = QuantLib::ext::make_shared<SimpleQuote>(0.0);
    spreadQuote_[securityID] = q;
    return q;
}

Handle<Quote> BondSpreadImplyMarket::securitySpread(const string& securityID, const string& configuration) const {
    return Handle<Quote>(spreadQuote(securityID));
}

} // namespace data
} // namespace ore
