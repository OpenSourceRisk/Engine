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

/*! \file ored/portfolio/builders/equitybarrieroption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/barrier/analyticbarrierengine.hpp>
#include <ql/pricingengines/barrier/fdblackscholesbarrierengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for Equity Barrier Options
/*! Pricing engines are cached by asset name / currency

    \ingroup portfolio
 */
class EquityBarrierOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&, const Date&> {

protected:
    EquityBarrierOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"EquityBarrierOption"}) {}

    virtual string keyImpl(const string& assetName, const Currency& ccy, const Date& expiryDate) override {
        return assetName + "/" + ccy.code() + "/" + ore::data::to_string(expiryDate);
    }

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> getBlackScholesProcess(const string& assetName, const Currency& ccy,
                                                                             const std::vector<Time>& timePoints = {}) {

        Handle<BlackVolTermStructure> vol = this->market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing));
            if (!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(
                    QuantLib::ext::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
                vol->enableExtrapolation();
            }
            return QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                this->market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
                this->market_->equityDividendCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                this->market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                vol);
    }
};

class EquityBarrierOptionAnalyticEngineBuilder
    : public EquityBarrierOptionEngineBuilder {
public:
    EquityBarrierOptionAnalyticEngineBuilder()
        : EquityBarrierOptionEngineBuilder("BlackScholesMerton", "AnalyticBarrierEngine") {}

protected:

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const Date& expiryDate) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy);
        return QuantLib::ext::make_shared<QuantLib::AnalyticBarrierEngine>(gbsp);
    }

};

class EquityBarrierOptionFDEngineBuilder
    : public EquityBarrierOptionEngineBuilder {
public:
    EquityBarrierOptionFDEngineBuilder()
        : EquityBarrierOptionEngineBuilder("BlackScholesMerton", "FdBlackScholesBarrierEngine") {}

protected:

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const Date& expiryDate) override {
        // We follow the way FdBlackScholesBarrierEngine determines maturity for time grid generation
        Handle<YieldTermStructure> riskFreeRate =
            market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing));
        Time expiry = riskFreeRate->dayCounter().yearFraction(riskFreeRate->referenceDate(),
                                                              std::max(riskFreeRate->referenceDate(), expiryDate));

        FdmSchemeDesc scheme = ore::data::parseFdmSchemeDesc(engineParameter("Scheme"));
        Size tGrid = std::max<Size>(1, (Size)(ore::data::parseInteger(engineParameter("TimeGridPerYear")) * expiry));
        Size xGrid = ore::data::parseInteger(engineParameter("XGrid"));
        Size dampingSteps = ore::data::parseInteger(engineParameter("DampingSteps"));
        bool monotoneVar = ore::data::parseBool(engineParameter("EnforceMonotoneVariance", {}, false, "true"));

        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp;

        if(monotoneVar) {
            // Replicate the construction of time grid in FiniteDifferenceModel::rollbackImpl
            // This time grid is required to build a BlackMonotoneVarVolTermStructure which
            // ensures monotonic variance along the time grid
            std::vector<Time> timePoints(tGrid + 1);
            Array timePointsArray(tGrid, expiry, -expiry / tGrid);
            timePoints[0] = 0.0;
            for(Size i = 0; i < tGrid; i++)
                timePoints[timePoints.size() - i - 1] = timePointsArray[i];
            timePoints.insert(std::upper_bound(timePoints.begin(), timePoints.end(), 0.99 / 365), 0.99 / 365);
            gbsp = getBlackScholesProcess(assetName, ccy, timePoints);
        } else {
            gbsp = getBlackScholesProcess(assetName, ccy);
        }
        return QuantLib::ext::make_shared<FdBlackScholesBarrierEngine>(gbsp, tGrid, xGrid,
                                                               dampingSteps, scheme);
    }

};

} // namespace data
} // namespace ore
