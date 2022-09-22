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

/*! \file ored/portfolio/fxderivative.hpp
    \brief FX base trade classes
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Base class for all FX Derivaties
class FxDerivative : virtual public ore::data::Trade {
protected:
    FxDerivative(const std::string& tradeType) : ore::data::Trade(tradeType) {}

    FxDerivative(const std::string& tradeType, ore::data::Envelope& env) : ore::data::Trade(tradeType, env) {}
};

//! Base class for all single asset FX Derivaties
class FxSingleAssetDerivative : public FxDerivative {
protected:
    FxSingleAssetDerivative(const std::string& tradeType) : ore::data::Trade(tradeType), FxDerivative(tradeType) {}

    FxSingleAssetDerivative(const std::string& tradeType, ore::data::Envelope& env, const std::string& boughtCurrency,
                            const std::string& soldCurrency)
        : ore::data::Trade(tradeType, env), FxDerivative(tradeType, env), boughtCurrency_(boughtCurrency),
          soldCurrency_(soldCurrency) {}

    // Protected members
    std::string boughtCurrency_;
    std::string soldCurrency_;

    // ... and references for FOR/DOM based trades
    // This is a bit dangerous, if any subclass mixed For-Dom with Bought-Sold it will get messed up!
    // The alternative is to declare 4 strings, all separate and have checks down every time a getter
    // is called below and importantly in the dtor of this class, i.e. check that one and only one pair
    // has been initialised here.
    std::string& foreignCurrency_ = boughtCurrency_;
    std::string& domesticCurrency_ = soldCurrency_;

public:
    const std::string& boughtCurrency() const { return boughtCurrency_; }
    const std::string& soldCurrency() const { return soldCurrency_; }
    const std::string& foreignCurrency() const { return foreignCurrency_; }
    const std::string& domesticCurrency() const { return domesticCurrency_; }
};

} // namespace data
} // namespace oreplus
