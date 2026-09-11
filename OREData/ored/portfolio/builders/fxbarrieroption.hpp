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

/*! \file ored/portfolio/builders/fxoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/utilities.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/barrier/fdblackscholesbarrierengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/pricingengines/analyticbarrierengine.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for European FX Barrier Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxBarrierOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const Currency&, const Currency&, const Date&,
                                                    const Date&> {

protected:
    FxBarrierOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"FxBarrierOption"}) {}

    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const Date& expiryDate,
                           const Date& paymentDate) override {
        return forCcy.code() + "/" + domCcy.code() + "/" + ore::data::to_string(expiryDate) + "/" +
               ore::data::to_string(paymentDate);
    }

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>
    getBlackScholesProcess(const Currency& forCcy, const Currency& domCcy, const std::vector<Time>& timePoints = {}) {
        const string& ccyPairCode = forCcy.code() + domCcy.code();
        Handle<BlackVolTermStructure> vol =
            market_->fxVol(ccyPairCode, configuration(ore::data::MarketContext::pricing));
        if (!timePoints.empty()) {
            vol = Handle<BlackVolTermStructure>(
                QuantLib::ext::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
            vol->enableExtrapolation();
        }
        return QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(ccyPairCode, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)), vol);
    }
};

class FxBarrierOptionAnalyticEngineBuilder : public FxBarrierOptionEngineBuilder {
public:
    FxBarrierOptionAnalyticEngineBuilder() : FxBarrierOptionEngineBuilder("GarmanKohlhagen", "AnalyticBarrierEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                        const Date& expiryDate, const Date& paymentDate) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy);
        return QuantLib::ext::make_shared<QuantExt::AnalyticBarrierEngine>(gbsp, paymentDate);
    }
};

class FxBarrierOptionFDEngineBuilder : public FxBarrierOptionEngineBuilder {
public:
    FxBarrierOptionFDEngineBuilder() : FxBarrierOptionEngineBuilder("GarmanKohlhagen", "FdBlackScholesBarrierEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                        const Date& expiryDate, const Date& paymentDate) override {
        // We follow the way FdBlackScholesBarrierEngine determines maturity for time grid generation
        Handle<YieldTermStructure> riskFreeRate =
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing));
        Time expiry = riskFreeRate->dayCounter().yearFraction(riskFreeRate->referenceDate(),
                                                              std::max(riskFreeRate->referenceDate(), expiryDate));

        FiniteDifferenceParams fdp = fdSchemeParams(*this, expiry, false);
        auto gbsp = getBlackScholesProcess(forCcy, domCcy, fdp.timePoints);
        return QuantLib::ext::make_shared<FdBlackScholesBarrierEngine>(
            gbsp, fdp.tGrid, fdp.xGrid, fdp.dampingSteps, fdp.scheme);
    }
};

class FxBarrierOptionScriptedEngineBuilder : public DelegatingEngineBuilder {
public:
    FxBarrierOptionScriptedEngineBuilder()
        : DelegatingEngineBuilder("ScriptedTrade", "ScriptedTrade", {"FxBarrierOption", "FxDoubleBarrierOption"}) {}
    QuantLib::ext::shared_ptr<ore::data::Trade>
    build(const Trade* trade, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;
    std::string effectiveTradeType() const override { return "ScriptedTrade"; }
};

} // namespace data
} // namespace oreplus
