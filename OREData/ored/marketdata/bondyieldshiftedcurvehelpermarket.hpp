/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2023 Oleg Kulkov
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

/*! \file ored/marketdata/bondyieldshiftedcurvehelpermarket.hpp
    \brief Wrapper class for building bond yield shifted yield curve
    \ingroup curves
*/

#pragma once

#include <ored/marketdata/marketimpl.hpp>

namespace ore {
    
namespace data {

/*! This market takes a map from Ibor index names to estimation curves and provides the corresponding
    Ibor indices via the market interface.

    In addition (dummy) yield curves, security spreads, default curves and recovery rates are provided, all with
    zero rates.

    This way we can build a Bond against this market and use the underlying QuantLib::Bond instrument to set
    up a BondHelper from which a fitted bond curve can be bootstrapped.
*/

class BondYieldShiftedCurveHelperMarket : public MarketImpl {
public:
    explicit BondYieldShiftedCurveHelperMarket(const std::map<std::string, Handle<YieldTermStructure>>& iborIndexCurves = {},
                                                const bool handlePseudoCurrencies = true);

    Handle<YieldTermStructure> yieldCurve(const string& name,
                                          const string& configuration = Market::defaultConfiguration) const override;

    Handle<Quote> securitySpread(const string& securityID,
                                 const string& configuration = Market::defaultConfiguration) const override;
    
    Handle<QuantExt::CreditCurve> defaultCurve(const string&,
                                               const string& configuration = Market::defaultConfiguration) const override;

    Handle<Quote> recoveryRate(const string&,
                               const string& configuration = Market::defaultConfiguration) const override;

};

} // namespace data

} // namespace ore
