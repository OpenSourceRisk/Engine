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

/*! \file ored/marketdata/bondspreadimplymarket.hpp
    \brief market that can be used to imply bond spreads
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/wrappedmarket.hpp>

#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

class BondSpreadImplyMarket : public WrappedMarket {
public:
    explicit BondSpreadImplyMarket(const QuantLib::ext::shared_ptr<Market>& market, const bool handlePseudoCurrencies = true);

    // an internally constructed spread quote returned by securitySpread()
    QuantLib::ext::shared_ptr<SimpleQuote> spreadQuote(const string& securityID) const;

    Handle<Quote> securitySpread(const string& securityID,
                                 const string& configuration = Market::defaultConfiguration) const override;

protected:
    mutable std::map<std::string, QuantLib::ext::shared_ptr<SimpleQuote>> spreadQuote_;
};

} // namespace data
} // namespace ore
