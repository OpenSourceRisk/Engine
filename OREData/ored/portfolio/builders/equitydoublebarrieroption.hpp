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

/*! \file ored/portfolio/builders/equitydoublebarrieroption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines//barrier/analyticdoublebarrierengine.hpp>
#include <ql/pricingengines/barrier/fdblackscholesbarrierengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for Equity Double Barrier Options
/*! Pricing engines are cached by asset name / currency

    \ingroup portfolio
 */
class EquityDoubleBarrierOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&, const Date&> {

protected:
    EquityDoubleBarrierOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"EquityDoubleBarrierOption"}) {}

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

class EquityDoubleBarrierOptionAnalyticEngineBuilder
    : public EquityDoubleBarrierOptionEngineBuilder {
public:
    EquityDoubleBarrierOptionAnalyticEngineBuilder()
        : EquityDoubleBarrierOptionEngineBuilder("BlackScholesMerton", "AnalyticDoubleBarrierEngine") {}

protected:

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const Date& expiryDate) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy);
        return QuantLib::ext::make_shared<QuantLib::AnalyticDoubleBarrierEngine>(gbsp);
    }

};

} // namespace data
} // namespace ore
