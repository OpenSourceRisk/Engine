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

/*! \file portfolio/builders/indexcreditdefaultswap.hpp
\brief
\ingroup portfolio
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/pricingengines/blackindexcdsoptionengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Index Credit Default Swap Options
/*! Pricing engines are cached by creditCurveId
    \ingroup portfolio
*/

class IndexCreditDefaultSwapOptionEngineBuilder
    : public CachingPricingEngineBuilder<vector<string>, const Currency&, const string&, const vector<string>&> {
protected:
    IndexCreditDefaultSwapOptionEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine) {}

    virtual vector<string> keyImpl(const Currency&, const string& creditCurveId,
                                   const vector<string>& creditCurveIds) override {
        vector<string> res(creditCurveIds);
        res.push_back(creditCurveId);
        return res;
    }
};

//! Midpoint Engine Builder class for IndexCreditDefaultSwaps
/*! This class creates a MidPointCdsEngine
    \ingroup portfolio
*/

class BlackIndexCdsOptionEngineBuilder : public IndexCreditDefaultSwapOptionEngineBuilder {
public:
    BlackIndexCdsOptionEngineBuilder()
        : IndexCreditDefaultSwapOptionEngineBuilder("Black", "BlackIndexCdsOptionEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const vector<string>& creditCurveIds) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<BlackVolTermStructure> vol =
            market_->cdsVol(creditCurveId, configuration(MarketContext::pricing));
        if (engineParameters_.at("Curve") == "Index") {
            Handle<DefaultProbabilityTermStructure> dpts =
                market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing));
            Handle<Quote> recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
            return boost::make_shared<QuantExt::BlackIndexCdsOptionEngine>(dpts, recovery->value(), yts, vol);
        } else if (engineParameters_.at("Curve") == "Underlying") {
            std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
            std::vector<Real> recovery;
            for (auto& c : creditCurveIds) {
                dpts.push_back(market_->defaultCurve(c, configuration(MarketContext::pricing)));
                recovery.push_back(market_->recoveryRate(c, configuration(MarketContext::pricing))->value());
            }
            return boost::make_shared<QuantExt::BlackIndexCdsOptionEngine>(dpts, recovery, yts, vol);
        } else {
            QL_FAIL("BlackIndexCdsEngineBuilder: Curve Parameter value \""
                    << engineParameters_.at("Curve") << "\" not recognised, expected Underlying or Index");
        }
    }
};

} // namespace data
} // namespace ore
