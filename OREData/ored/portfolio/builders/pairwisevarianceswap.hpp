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

/*! \file ored/portfolio/builders/pairwisevarianceswap.hpp
    \brief pairwise variance swap engine builder
    \ingroup builders
*/

#pragma once

#include <ored/utilities/parsers.hpp>
#include <qle/pricingengines/pairwisevarianceswapengine.hpp>

#include <string>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/version.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for Pairwise Variance Swaps
/*! Pricing engines are cached by equity/currency

    \ingroup builders
 */
class PairwiseVarSwapEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<string, const string&, const string&, const Currency&, const Date&,
                                                    const AssetClass&> {
public:
    PairwiseVarSwapEngineBuilder()
        : CachingEngineBuilder("BlackScholes", "PairwiseVarianceSwapEngine",
                               {"EquityPairwiseVarianceSwap", "FxPairwiseVarianceSwap"}) {}

protected:
    virtual string keyImpl(const string& underlyingName1, const string& underlyingName2, const Currency& ccy,
                           const Date& accrEndDate, const AssetClass& assetClassUnderlyings) override {
        return underlyingName1 + "/" + underlyingName2 + "/" + ccy.code();
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& underlyingName1, const string& underlyingName2,
                                                        const Currency& ccy, const Date& accrEndDate,
                                                        const AssetClass& assetClassUnderlyings) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp1, gbsp2;
        QuantLib::ext::shared_ptr<Index> index1, index2;
        if (assetClassUnderlyings == AssetClass::EQ) {
            gbsp1 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(underlyingName1, configuration(ore::data::MarketContext::pricing)),
                market_->equityDividendCurve(underlyingName1, configuration(ore::data::MarketContext::pricing)),
                market_->equityForecastCurve(underlyingName1, configuration(ore::data::MarketContext::pricing)),
                market_->equityVol(underlyingName1, configuration(ore::data::MarketContext::pricing)));
            index1 = market_->equityCurve(underlyingName1).currentLink();
            gbsp2 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(underlyingName2, configuration(ore::data::MarketContext::pricing)),
                market_->equityDividendCurve(underlyingName2, configuration(ore::data::MarketContext::pricing)),
                market_->equityForecastCurve(underlyingName2, configuration(ore::data::MarketContext::pricing)),
                market_->equityVol(underlyingName2, configuration(ore::data::MarketContext::pricing)));
            index2 = market_->equityCurve(underlyingName2).currentLink();
        } else if (assetClassUnderlyings == AssetClass::FX) {
            const auto fxIndex1 = market_->fxIndex("FX-" + underlyingName1);
            const auto fxIndex2 = market_->fxIndex("FX-" + underlyingName2);
            const string& ccyPairCode1 = fxIndex1->sourceCurrency().code() + fxIndex1->targetCurrency().code();
            const string& ccyPairCode2 = fxIndex2->sourceCurrency().code() + fxIndex2->targetCurrency().code();
            gbsp1 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->fxSpot(ccyPairCode1, configuration(ore::data::MarketContext::pricing)),
                fxIndex1->targetCurve(), fxIndex1->sourceCurve(),
                market_->fxVol(ccyPairCode1, configuration(ore::data::MarketContext::pricing)));
            gbsp2 = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->fxSpot(ccyPairCode2, configuration(ore::data::MarketContext::pricing)),
                fxIndex2->targetCurve(), fxIndex2->sourceCurve(),
                market_->fxVol(ccyPairCode2, configuration(ore::data::MarketContext::pricing)));
            index1 = fxIndex1.currentLink();
            index2 = fxIndex2.currentLink();
            
        } else {
            QL_FAIL("Asset class of " + underlyingName1 + " and " + underlyingName2 + " not recognized.");
        }

        QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
            QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
        Handle<Quote> correlation(
            QuantLib::ext::make_shared<QuantExt::CorrelationValue>(corrCurve, corrCurve->timeFromReference(accrEndDate)));

        try {
            corrCurve =
                market_->correlationCurve(index1->name(), index2->name(), configuration(MarketContext::pricing));
        } catch (...) {
            WLOG("no correlation curve for " << index1->name() << ", " << index2->name()
                                             << " found, fall back to zero correlation");
        }

        return QuantLib::ext::make_shared<QuantExt::PairwiseVarianceSwapEngine>(
            index1, index2, gbsp1, gbsp2,
            market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing)), correlation);
    }
};

} // namespace data
} // namespace ore
