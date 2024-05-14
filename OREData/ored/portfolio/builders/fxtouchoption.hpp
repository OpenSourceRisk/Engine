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

/*! \file ored/portfolio/builders/fxtouchoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/pricingengines/analyticdigitalamericanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for FX Touch Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxTouchOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const Currency&, const Currency&, const string&,
                                                    const Date&, const bool> {
public:
    FxTouchOptionEngineBuilder()
        : CachingEngineBuilder("GarmanKohlhagen", "AnalyticDigitalAmericanEngine", {"FxTouchOption"}) {}
    FxTouchOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"FxTouchOption"}) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const string& type, const Date& payDate,
                           const bool flipResults) override {
        return forCcy.code() + domCcy.code() + type + ore::data::to_string(payDate) + (flipResults ? "_1" : "_0");
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                        const string& type, const Date& payDate,
                                                        const bool flipResults) override {
        string pair = forCcy.code() + domCcy.code();
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(),
                                   configuration(ore::data::MarketContext::pricing)), // dividend yield ~ foreign yield
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->fxVol(pair, configuration(ore::data::MarketContext::pricing)));

        if (type == "One-Touch") {
            return QuantLib::ext::make_shared<QuantExt::AnalyticDigitalAmericanEngine>(gbsp, payDate, flipResults);
        } else if (type == "No-Touch") {
            return QuantLib::ext::make_shared<QuantExt::AnalyticDigitalAmericanKOEngine>(gbsp, payDate, flipResults);
        } else {
            QL_FAIL("Unknown FX touch option type: " << type);
        }
    }
};

} // namespace data
} // namespace oreplus
