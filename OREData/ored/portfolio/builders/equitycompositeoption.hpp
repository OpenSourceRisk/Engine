/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/equitycompositeoption.hpp
    \brief Engine builder for equity composite options
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/quotes/compositequote.hpp>
#include <qle/termstructures/blackvolsurfaceproxy.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

namespace ore {
namespace data {

//! Engine Builder for Composite European Equity Options
/*! Pricing engines are cached by asset/asset currency/strike currency

    \ingroup builders
 */
class EquityEuropeanCompositeEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const Currency&, const Currency&, const Date&> {
public:
    EquityEuropeanCompositeEngineBuilder()
        : CachingEngineBuilder("BlackScholes", "AnalyticEuropeanEngine", {"EquityEuropeanCompositeOption"}) {}

protected:
    virtual string keyImpl(const string& equityName, const Currency& equityCcy, const Currency& strikeCcy,
                           const Date& expiry) override {
        return equityName + "/" + equityCcy.code() + "/" + strikeCcy.code() + "/" + to_string(expiry);
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& equityName, const Currency& equityCcy,
                                                        const Currency& strikeCcy, const Date& expiry) override {

        string config = this->configuration(ore::data::MarketContext::pricing);

        // FOR = Underlyign CCY and DOM = Strike Currency 
        // -> converting underlying ccy into strike ccy by multiply eq spot by fx spot
        string ccyPairCode = equityCcy.code() + strikeCcy.code();

        Handle<Quote> equitySpot = this->market_->equitySpot(equityName, config);
        Handle<Quote> fxSpot = this->market_->fxRate(ccyPairCode, config);
        
        std::function<Real(const Real&, const Real&)> multiply = [](const Real& a, const Real& b) { return a * b; };

        using scaledQuoted = CompositeQuote<std::function<Real(const Real&, const Real&)>>;
        
        Handle<Quote> spot(QuantLib::ext::make_shared<scaledQuoted>(equitySpot, fxSpot, multiply));

        auto dividendCurve = this->market_->equityDividendCurve(equityName, config);

        auto equtiyForcastCurve = this->market_->equityForecastCurve(equityName, config);

        auto equityIndex = this->market_->equityCurve(equityName, config);

        Handle<BlackVolTermStructure> eqVol = this->market_->equityVol(equityName, config);
        Handle<BlackVolTermStructure> fxVol = this->market_->fxVol(ccyPairCode, config);
        
        auto strikeCcyDiscountCurve = this->market_->discountCurve(strikeCcy.code(), config);

        auto fxIndex = market_->fxIndex(equityCcy.code() + strikeCcy.code()).currentLink();

        // Try Catch and 0 correlation fallback
        Handle<QuantExt::CorrelationTermStructure> correlation;
        try {
            correlation = this->market_->correlationCurve(
                "FX-GENERIC-" + equityCcy.code() + "-" + strikeCcy.code(), "EQ-" + equityName, config);
        } catch (...) {
            WLOG("Couldnt find correlation curve "
                 << " FX - GENERIC - " << equityCcy.code() << " -" << strikeCcy.code() << "&EQ - " << equityName << ". will fallback to zero correlation");
            correlation = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, WeekendsOnly(), 0, Actual365Fixed()));
        }
        
        Handle<BlackVolTermStructure> vol(
            QuantLib::ext::make_shared<QuantExt::BlackVolatilitySurfaceProxy>(*eqVol, *equityIndex, *equityIndex, *fxVol, fxIndex, *correlation));
        
        auto blackScholesProcess = QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(
            spot, dividendCurve, strikeCcyDiscountCurve, vol);
        
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(strikeCcy.code(), configuration(MarketContext::pricing));

        return QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(blackScholesProcess, discountCurve);
    }
};

} // namespace data
} // namespace ore
