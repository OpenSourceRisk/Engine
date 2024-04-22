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

/*! \file ored/portfolio/builders/equitytouchoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/pricingengines/vanilla/analyticdigitalamericanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for EQ Touch Options
/*! Pricing engines are cached by asset name / currency

    \ingroup portfolio
 */
class EquityTouchOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&, const string&> {
public:
    EquityTouchOptionEngineBuilder()
        : CachingEngineBuilder("BlackScholesMerton", "AnalyticDigitalAmericanEngine", {"EquityTouchOption"}) {}
    EquityTouchOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"EquityTouchOption"}) {}

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy, const string& type) override {
        return assetName + ccy.code() + type;
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const string& type) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityDividendCurve(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing)));

        if (type == "One-Touch") {
            engine_ = "AnalyticDigitalAmericanEngine";
            return QuantLib::ext::make_shared<QuantLib::AnalyticDigitalAmericanEngine>(gbsp);
        } else if (type == "No-Touch") {
            engine_ = "AnalyticDigitalAmericanKOEngine";
            return QuantLib::ext::make_shared<QuantLib::AnalyticDigitalAmericanKOEngine>(gbsp);
        } else {
            QL_FAIL("Unknwon EQ touch option type: " << type);
        }
    }
};

} // namespace data
} // namespace oreplus
