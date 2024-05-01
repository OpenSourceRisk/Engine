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

/*! \file portfolio/builders/creditdefaultswapoption.hpp
\brief Builder that returns an engine to price a credit default swap option
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/creditdefaultswap.hpp>

#include <qle/pricingengines/blackcdsoptionengine.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Credit Default Swap Options
/*! Pricing engines are cached by creditCurveId and cds term.
    \ingroup portfolio
*/

class CreditDefaultSwapOptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&> {

protected:
    CreditDefaultSwapOptionEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CreditDefaultSwapOption"}) {}

    virtual string keyImpl(const Currency&, const string& creditCurveId,
                           const string& term) override {
        return term.empty() ? creditCurveId : creditCurveId + "-" + term;
    }
};

//! Black CDS option engine builder for CDS options.
/*! This class creates a BlackCdsOptionEngine
    \ingroup portfolio
*/

class BlackCdsOptionEngineBuilder : public CreditDefaultSwapOptionEngineBuilder {
public:
    BlackCdsOptionEngineBuilder()
        : CreditDefaultSwapOptionEngineBuilder("Black", "BlackCdsOptionEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const string& term) override {

        string curveId = term.empty() ? creditCurveId : creditCurveId + "-" + term;
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<QuantExt::CreditVolCurve> vol = market_->cdsVol(curveId, configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts =
            market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing))->curve();
        Handle<Quote> recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantExt::BlackCdsOptionEngine>(dpts, recovery->value(), yts, vol);
    }
};

} // namespace data
} // namespace ore
