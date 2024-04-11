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

/*! \file ored/portfolio/builders/fxdoublebarrieroption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/barrier/fdblackscholesbarrierengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/pricingengines/analyticdoublebarrierengine.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for European FX Double Barrier Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxDoubleBarrierOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const Currency&, const Currency&, const Date&> {

protected:
    FxDoubleBarrierOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"FxDoubleBarrierOption"}) {}

    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy, const Date& paymentDate) override {
        return forCcy.code() + domCcy.code() + ore::data::to_string(paymentDate);
    }

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>
    getBlackScholesProcess(const Currency& forCcy, const Currency& domCcy, const std::vector<Time>& timePoints = {}) {
        const string pair = forCcy.code() + domCcy.code();
        Handle<BlackVolTermStructure> vol = market_->fxVol(pair, configuration(ore::data::MarketContext::pricing));
        if (!timePoints.empty()) {
            vol = Handle<BlackVolTermStructure>(
                QuantLib::ext::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
            vol->enableExtrapolation();
        }
        return QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing)),
            market_->discountCurve(forCcy.code(),
                                   configuration(ore::data::MarketContext::pricing)), // dividend yield ~ foreign yield
            market_->discountCurve(domCcy.code(), configuration(ore::data::MarketContext::pricing)), vol);
    }
};

//! Analytical Engine Builder for FX Double Barrier Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxDoubleBarrierOptionAnalyticEngineBuilder : public FxDoubleBarrierOptionEngineBuilder {
public:
    FxDoubleBarrierOptionAnalyticEngineBuilder()
        : FxDoubleBarrierOptionEngineBuilder("GarmanKohlhagen", "AnalyticDoubleBarrierEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy, const Date& paymentDate) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy);
        return QuantLib::ext::make_shared<QuantExt::AnalyticDoubleBarrierEngine>(gbsp, paymentDate);
    }
};

} // namespace data
} // namespace oreplus
