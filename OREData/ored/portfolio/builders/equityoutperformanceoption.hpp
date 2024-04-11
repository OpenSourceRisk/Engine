/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/equityoutperformanceoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <qle/pricingengines/analyticoutperformanceoptionengine.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;
using namespace ore::data;

//! Engine Builder for EQ Outperformance Option
/*! Pricing engines are cached by asset names / currency

    \ingroup portfolio
 */
class EquityOutperformanceOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const string&, const Currency&> {
public:
    EquityOutperformanceOptionEngineBuilder()
        : CachingEngineBuilder("BlackScholesMerton", "AnalyticOutperformanceOptionEngine", {"EquityOutperformanceOption"}) {}
    EquityOutperformanceOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"EquityOutperformanceOption"}) {}

protected:
    virtual string keyImpl(const string& assetName1, const string& assetName2, const Currency& ccy) override {
        return assetName1 + assetName2 + ccy.code();
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName1, const string& assetName2, const Currency& ccy) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp1 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->equitySpot(assetName1, configuration(ore::data::MarketContext::pricing)),
            market_->equityDividendCurve(assetName1, configuration(ore::data::MarketContext::pricing)),
            market_->equityForecastCurve(assetName1, configuration(ore::data::MarketContext::pricing)),
            market_->equityVol(assetName1, configuration(ore::data::MarketContext::pricing)));

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp2 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            market_->equitySpot(assetName2, configuration(ore::data::MarketContext::pricing)),
            market_->equityDividendCurve(assetName2, configuration(ore::data::MarketContext::pricing)),
            market_->equityForecastCurve(assetName2, configuration(ore::data::MarketContext::pricing)),
            market_->equityVol(assetName2, configuration(ore::data::MarketContext::pricing)));

        int integrationPoints = ore::data::parseInteger(engineParameter("IntegrationPoints"));

        QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
            QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
        try {
            corrCurve = market_->correlationCurve("EQ-" + assetName1, "EQ-" + assetName2,
                                                  configuration(MarketContext::pricing));
        } catch (...) {
            WLOG("no correlation curve for EQ-" << assetName1 << ", EQ-" << assetName2
                                                << " found, fall back to zero correlation.");
        }

        engine_ = "AnalyticOutperformanceOptionEngine";
        return QuantLib::ext::make_shared<QuantExt::AnalyticOutperformanceOptionEngine>(gbsp1, gbsp2, corrCurve, integrationPoints);
    }
};

} // namespace data
} // namespace ore
