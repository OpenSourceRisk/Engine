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

/*! \file ored/portfolio/builders/equitydigitaloption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using std::string;

//! Engine Builder for European EQ Digital Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class EquityDigitalOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&> {
public:
    EquityDigitalOptionEngineBuilder()
        : CachingEngineBuilder("BlackScholesMerton", "AnalyticEuropeanEngine", {"EquityDigitalOption"}) {}

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy) override {
        return assetName + "/" + ccy.code();
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy) override {

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityDividendCurve(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing)));

        return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngine>(gbsp);
    }
};

} // namespace data
} // namespace oreplus
