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

/*! \file portfolio/builders/fxoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/builders/fxbarrieroption.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/utilities.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/barrier/analyticbinarybarrierengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;
  
//! Engine Builder for European FX Digital Barrier Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxDigitalBarrierOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const Currency&, const Currency&, const Date&> {
public:
    FxDigitalBarrierOptionEngineBuilder()
        : CachingEngineBuilder("GarmanKohlhagen", "FdBlackScholesBarrierEngine", {"FxDigitalBarrierOption"}) {}
    FxDigitalBarrierOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"FxDigitalBarrierOption"}) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const Date& expiryDate) override {
        return forCcy.code() + domCcy.code() + ore::data::to_string(expiryDate);
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy,
                                                        const Date& expiryDate) override {
        // We follow the way FdBlackScholesBarrierEngine determines maturity for time grid generation
        Handle<YieldTermStructure> riskFreeRate =
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing));
        Time expiry = riskFreeRate->dayCounter().yearFraction(riskFreeRate->referenceDate(),
                                                              std::max(riskFreeRate->referenceDate(), expiryDate));

        FiniteDifferenceParams fdp = fdSchemeParams(*this, expiry);
        const string pair = forCcy.code() + domCcy.code();
        Handle<BlackVolTermStructure> vol = market_->fxVol(pair, configuration(ore::data::MarketContext::pricing));
        if (fdp.monotoneVar) {
            vol = Handle<BlackVolTermStructure>(
                QuantLib::ext::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, fdp.timePoints));
            vol->enableExtrapolation();
        }
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(), configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)), vol);
        return QuantLib::ext::make_shared<FdBlackScholesBarrierEngine>(
            gbsp, fdp.tGrid, fdp.xGrid, fdp.dampingSteps, fdp.scheme);
    }
};

} // namespace data
} // namespace oreplus
