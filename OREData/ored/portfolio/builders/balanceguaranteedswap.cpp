/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/balanceguaranteedswap.hpp>
#include <qle/pricingengines/numericlgmbgsflexiswapengine.hpp>

#include <ql/quotes/compositequote.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine>
BalanceGuaranteedSwapFlexiSwapLGMGridEngineBuilder::engineImpl(const string& id, const string& id2, const string& ccy,
                                                               const std::vector<Date>& expiries, const Date& maturity,
                                                               const std::vector<Real>& strikes) {
    DLOG("Building LGM Grid BGS Flexi Swap engine for trade " << id);

    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm = model(id, ccy, expiries, maturity, strikes);

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));
    QuantExt::NumericLgmFlexiSwapEngine::Method method;
    if (engineParameter("method") == "SingleSwaptions")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions;
    else if (engineParameter("method") == "SwaptionArray")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::SwaptionArray;
    else if (engineParameter("method") == "Automatic")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::Automatic;
    else {
        QL_FAIL("FlexiSwap engine parameter method (" << engineParameter("method") << ") not recognised");
    }
    Real singleSwaptionThreshold = parseReal(engineParameter("singleSwaptionThreshold"));

    Handle<Quote> minCprMult(QuantLib::ext::make_shared<SimpleQuote>(parseReal(modelParameter("MinCPRMultiplier"))));
    Handle<Quote> maxCprMult(QuantLib::ext::make_shared<SimpleQuote>(parseReal(modelParameter("MaxCPRMultiplier"))));
    Handle<Quote> cpr = market_->cpr(id2, configuration(MarketContext::pricing));
    // use makeCompositeQuote from ql 1.16 onwards...
    Handle<Quote> minCpr(
        QuantLib::ext::make_shared<CompositeQuote<std::multiplies<Real>>>(minCprMult, cpr, std::multiplies<Real>()));
    Handle<Quote> maxCpr(
        QuantLib::ext::make_shared<CompositeQuote<std::multiplies<Real>>>(maxCprMult, cpr, std::multiplies<Real>()));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    Handle<YieldTermStructure> dscCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<QuantExt::NumericLgmBgsFlexiSwapEngine>(lgm, sy, ny, sx, nx, minCpr, maxCpr, dscCurve,
                                                                      method, singleSwaptionThreshold);
}

} // namespace data
} // namespace ore
