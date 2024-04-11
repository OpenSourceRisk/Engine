/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/fxforward.hpp
    \brief Engine builder for FX Forwards
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for FX Forwards
/*! Pricing engines are cached by currency1 and currency2
    \ingroup builders
*/
class FxForwardEngineBuilderBase
    : public CachingPricingEngineBuilder<string, const Currency&, const Currency&> {
public:
    FxForwardEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"FxForward"}) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy) override {
        return forCcy.code() + domCcy.code();
    }
};

//! Engine Builder for FX Forwards
/*! Pricing engines are cached by currency pair
    \ingroup builders
*/
class FxForwardEngineBuilder : public FxForwardEngineBuilderBase {
public:
    FxForwardEngineBuilder() : FxForwardEngineBuilderBase("DiscountedCashflows", "DiscountingFxForwardEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override {
        string pair = keyImpl(forCcy, domCcy);
        string tmp = engineParameter("includeSettlementDateFlows", {}, false, "");
        bool includeSettlementDateFlows = tmp == "" ? false : parseBool(tmp);
        return QuantLib::ext::make_shared<QuantExt::DiscountingFxForwardEngine>(
            domCcy, market_->discountCurve(domCcy.code(), configuration(MarketContext::pricing)), forCcy,
            market_->discountCurve(forCcy.code(), configuration(MarketContext::pricing)),
            market_->fxRate(pair, configuration(MarketContext::pricing)),
            includeSettlementDateFlows);
    }
};

//! FX forward engine builder for external cam, with additional simulation dates (AMC)
class CamAmcFxForwardEngineBuilder : public FxForwardEngineBuilderBase {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcFxForwardEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                const std::vector<Date>& simulationDates)
        : FxForwardEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
