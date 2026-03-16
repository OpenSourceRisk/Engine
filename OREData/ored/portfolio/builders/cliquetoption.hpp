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

/*! \file portfolio/builders/cliquetoption.hpp
    \brief Engine builder for cliquet options
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cliquetoption.hpp>
#include <ored/portfolio/scriptedtrade.hpp>

namespace ore {
namespace data {

//! Engine builder for Cliquet Options
/*! Pricing engines are cached by currency
    \ingroup builders
*/
class CliquetOptionEngineBuilder
    : public ore::data::CachingOptionEngineBuilder<std::string, const std::string&, const QuantLib::Currency&,
                                                   const ore::data::AssetClass&> {
public:
    CliquetOptionEngineBuilder(const std::string& model, const std::string& engine,
                               const std::set<std::string>& tradeTypes, const ore::data::AssetClass& assetClass)
        : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass) {}

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engine(const std::string& assetName, const QuantLib::Currency& ccy) {
        return ore::data::CachingOptionEngineBuilder<std::string, const std::string&, const QuantLib::Currency&,
                                                     const ore::data::AssetClass&>::engine(assetName, ccy, assetClass_);
    }

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engine(const QuantLib::Currency& ccy1, const QuantLib::Currency& ccy2) {
        return ore::data::CachingOptionEngineBuilder<std::string, const std::string&, const QuantLib::Currency&,
                                                     const ore::data::AssetClass&>::engine(ccy1.code(), ccy2,
                                                                                           assetClass_);
    }

protected:
    virtual std::string keyImpl(const std::string& assetName, const QuantLib::Currency& ccy,
                                const ore::data::AssetClass& assetClass) override {
        return assetName + "/" + ccy.code();
    }
};

//! Engine Builder for Equity Cliquet Options
/*! Pricing engines are cached by asset/currency
    \ingroup builders
 */
class EquityCliquetOptionEngineBuilder : public CliquetOptionEngineBuilder {
public:
    EquityCliquetOptionEngineBuilder(const std::string& model, const std::string& engine)
        : CliquetOptionEngineBuilder(model, engine, {"EquityCliquetOption"}, ore::data::AssetClass::EQ) {}
};

class EquityCliquetOptionMcScriptEngineBuilder : public EquityCliquetOptionEngineBuilder {
public:
    EquityCliquetOptionMcScriptEngineBuilder() : EquityCliquetOptionEngineBuilder("BlackScholes", "MCScript") {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::string& assetName, const QuantLib::Currency& ccy,
                                                const ore::data::AssetClass& assetClass) override;
};

} // namespace data
} // namespace ore
