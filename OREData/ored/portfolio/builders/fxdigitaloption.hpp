/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/fxdigitaloption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/pricingengines/analyticeuropeanengine.hpp>
#include <qle/pricingengines/analyticcashsettledeuropeanengine.hpp>
#include <qle/pricingengines/fxdigitalcallspreadengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
namespace ore {
namespace data {
using std::string;

//! Engine Builder base class for European FX Digital Options
/*! FX digital options are always cash settled and priced with a cash-settled
    European engine. Concrete builders select the actual pricing engine.
    Pricing engines are cached by currency pair.

    \ingroup portfolio
 */
class FxDigitalOptionEngineBuilderBase
    : public ore::data::CachingPricingEngineBuilder<string, const Currency&, const Currency&, const bool> {
public:
    FxDigitalOptionEngineBuilderBase(const string& model, const string& engine, 
                                     const std::set<std::string>& productTypes = {"FxDigitalOption"}) 
        : CachingEngineBuilder(model, engine, productTypes) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const bool flipResults) override {
        return forCcy.code() + domCcy.code() + (flipResults ? "_1" : "_0");
    }

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process(const Currency& forCcy, const Currency& domCcy) {
        string pair = forCcy.code() + domCcy.code();
        return QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(), configuration(ore::data::MarketContext::pricing)), // dividend yield ~ foreign yield
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->fxVol(pair, configuration(ore::data::MarketContext::pricing)));
    }
};

//! Engine Builder for European cash-settled FX Digital Options
/*! Builds an AnalyticCashSettledEuropeanEngine.

    \ingroup portfolio
 */
class FxDigitalOptionEngineBuilder : public FxDigitalOptionEngineBuilderBase {
public:
    FxDigitalOptionEngineBuilder() : FxDigitalOptionEngineBuilderBase("GarmanKohlhagen", "AnalyticEuropeanEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                                const bool flipResults) override {
        string pair = forCcy.code() + domCcy.code();

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(),
                                   configuration(ore::data::MarketContext::pricing)), // dividend yield ~ foreign yield
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->fxVol(pair, configuration(ore::data::MarketContext::pricing)));
        return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngine>(gbsp, flipResults);
    }
};

//! Engine Builder for European cash-settled FX Digital Options using call-spread replication
/*! Builds an FxDigitalCallSpreadEngine.

    \ingroup portfolio
 */
class FxDigitalOptionCallSpreadEngineBuilder : public FxDigitalOptionEngineBuilderBase {
public:
    FxDigitalOptionCallSpreadEngineBuilder() : FxDigitalOptionEngineBuilderBase("GarmanKohlhagen", "CallSpreadEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                                const bool flipResults) override {
        Real eps = parseReal(engineParameter("CallSpreadEps", {}, false, "1.0e-4"));
        return QuantLib::ext::make_shared<QuantExt::FxDigitalCallSpreadEngine>(process(forCcy, domCcy), flipResults, eps);
    }
};

//! Engine Builder for European cash-settled FX Digital Options with CS product type
/*! Builds an AnalyticCashSettledEuropeanEngine for FxDigitalOptionEuropeanCS product type.

    \ingroup portfolio
 */
class FxDigitalCSOptionEngineBuilder : public FxDigitalOptionEngineBuilderBase {
public:
    FxDigitalCSOptionEngineBuilder()
        : FxDigitalOptionEngineBuilderBase("GarmanKohlhagen", "AnalyticCashSettledEuropeanEngine", {"FxDigitalOptionEuropeanCS"}) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const bool flipResults) override {
        return forCcy.code() + domCcy.code();
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                                const bool flipResults) override {
        string pair = forCcy.code() + domCcy.code();

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(),
                                   configuration(ore::data::MarketContext::pricing)), // dividend yield ~ foreign yield
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->fxVol(pair, configuration(ore::data::MarketContext::pricing)));

        return QuantLib::ext::make_shared<QuantExt::AnalyticCashSettledEuropeanEngine>(gbsp, flipResults);
    }
};

} // namespace data
} // namespace oreplus
