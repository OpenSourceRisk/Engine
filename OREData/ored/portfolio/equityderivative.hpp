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

/*! \file ored/portfolio/equityderivative.hpp
    \brief EQ base trade classes
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp> // For EquityUnderlying type

namespace ore {
namespace data {

//! Base class for all Equity Derivaties
class EquityDerivative : virtual public ore::data::Trade {
protected:
    EquityDerivative(const std::string& tradeType) : ore::data::Trade(tradeType) {}

    EquityDerivative(const std::string& tradeType, ore::data::Envelope& env) : ore::data::Trade(tradeType, env) {}
};

//! Base class for all single asset Equity Derivaties
class EquitySingleAssetDerivative : public EquityDerivative {
protected:
    EquitySingleAssetDerivative(const std::string& tradeType) : ore::data::Trade(tradeType), EquityDerivative(tradeType) {}

    EquitySingleAssetDerivative(const std::string& tradeType, ore::data::Envelope& env, const EquityUnderlying& equityUnderlying)
        : ore::data::Trade(tradeType, env), EquityDerivative(tradeType, env), equityUnderlying_(equityUnderlying) {}

    // Protected members
    EquityUnderlying equityUnderlying_;

public:
    const string& equityName() const { return equityUnderlying_.name(); }
};

} // namespace data
} // namespace oreplus
