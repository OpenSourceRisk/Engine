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

/*! \file ored/portfolio/builders/equitydoubletouchoption.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/experimental/barrieroption/analyticdoublebarrierbinaryengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Abstract Engine Builder for EQ Double Touch Options
/*! Pricing engines are cached by asset name / currency

    \ingroup portfolio
 */
class EquityDoubleTouchOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&> {
public:
    EquityDoubleTouchOptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"EquityDoubleTouchOption"}) {}

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy) override {
        return assetName + ccy.code();
    }

    boost::shared_ptr<GeneralizedBlackScholesProcess>
    getBlackScholesProcess(const string& assetName, const Currency& ccy, const std::vector<Time>& timePoints = {}) {
        Handle<BlackVolTermStructure> vol =
            market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing));
        if (!timePoints.empty()) {
            vol = Handle<BlackVolTermStructure>(
                boost::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
            vol->enableExtrapolation();
        }
        return boost::make_shared<GeneralizedBlackScholesProcess>(
            market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
            market_->equityDividendCurve(assetName,configuration(ore::data::MarketContext::pricing)),
            market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)), vol);
    }
};

//! Analytical Engine Builder for EQ Double Touch Options
/*! Pricing engines are cached by asset name / currency

    \ingroup portfolio
 */
class EquityDoubleTouchOptionAnalyticEngineBuilder : public EquityDoubleTouchOptionEngineBuilder {
public:
    EquityDoubleTouchOptionAnalyticEngineBuilder()
        : EquityDoubleTouchOptionEngineBuilder("GarmanKohlhagen", "AnalyticDoubleBarrierBinaryEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy);

        engine_ = "AnalyticDoubleBarrierBinaryEngine";
        return boost::make_shared<QuantLib::AnalyticDoubleBarrierBinaryEngine>(gbsp);
    }
};

} // namespace data
} // namespace oreplus
