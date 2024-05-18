/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file portfolio/equityswap.hpp
    \brief Equity Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable Equity Swap contract
/*!
\ingroup tradedata
*/
class EquitySwap : public Swap {
public:
    //! Default constructor
    EquitySwap() : Swap("EquitySwap") {}

    //! Constructor with vector of LegData
    EquitySwap(const Envelope& env, const vector<LegData>& legData);

    //! Constructor with two legs
    EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);

    void checkEquitySwap(const vector<LegData>& legData);

    //! Trade interface
    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

private:
    Size equityLegIndex_, irLegIndex_;
};
} // namespace data
} // namespace ore
