/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/cliquetoption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/commoditydigitaloption.hpp>
#include <ored/portfolio/compositetrade.hpp>
#include <ored/portfolio/commodityapo.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commodityoptionstrip.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/commodityswaption.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equitybarrieroption.hpp>
#include <ored/portfolio/equitydoublebarrieroption.hpp>
#include <ored/portfolio/equitydigitaloption.hpp>
#include <ored/portfolio/equitydoubletouchoption.hpp>
#include <ored/portfolio/equityeuropeanbarrieroption.hpp>
#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/equitytouchoption.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxaverageforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxdoublebarrieroption.hpp>
#include <ored/portfolio/fxdoubletouchoption.hpp>
#include <ored/portfolio/fxdigitalbarrieroption.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/crosscurrencyswap.hpp>
#include <ored/portfolio/inflationswap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/varianceswap.hpp>
#include <ored/portfolio/failedtrade.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/utilities/log.hpp>

using namespace std;

namespace ore {
namespace data {

TradeFactory::TradeFactory(std::map<string, boost::shared_ptr<AbstractTradeBuilder>> extraBuilders) {
    addBuilder("Swap", boost::make_shared<TradeBuilder<Swap>>());
    addBuilder("CrossCurrencySwap", boost::make_shared<TradeBuilder<CrossCurrencySwap>>());
    addBuilder("InflationSwap", boost::make_shared<TradeBuilder<InflationSwap>>());
    addBuilder("Swaption", boost::make_shared<TradeBuilder<Swaption>>());
    addBuilder("FxAverageForward", boost::make_shared<TradeBuilder<FxAverageForward>>());
    addBuilder("FxForward", boost::make_shared<TradeBuilder<FxForward>>());
    addBuilder("ForwardRateAgreement", boost::make_shared<TradeBuilder<ForwardRateAgreement>>());
    addBuilder("FxSwap", boost::make_shared<TradeBuilder<FxSwap>>());
    addBuilder("FxOption", boost::make_shared<TradeBuilder<FxOption>>());
    addBuilder("FxAsianOption", boost::make_shared<TradeBuilder<FxAsianOption>>());
    addBuilder("FxBarrierOption", boost::make_shared<TradeBuilder<FxBarrierOption>>());
    addBuilder("FxDoubleBarrierOption", boost::make_shared<TradeBuilder<FxDoubleBarrierOption>>());
    addBuilder("FxKIKOBarrierOption", boost::make_shared<TradeBuilder<FxKIKOBarrierOption>>());
    addBuilder("FxDigitalBarrierOption", boost::make_shared<TradeBuilder<FxDigitalBarrierOption>>());
    addBuilder("FxTouchOption", boost::make_shared<TradeBuilder<FxTouchOption>>());
    addBuilder("FxDoubleTouchOption", boost::make_shared<TradeBuilder<FxDoubleTouchOption>>());
    addBuilder("FxEuropeanBarrierOption", boost::make_shared<TradeBuilder<FxEuropeanBarrierOption>>());
    addBuilder("FxDigitalOption", boost::make_shared<TradeBuilder<FxDigitalOption>>());
    addBuilder("FxVarianceSwap", boost::make_shared<TradeBuilder<FxVarSwap>>());
    addBuilder("CapFloor", boost::make_shared<TradeBuilder<CapFloor>>());
    addBuilder("EquityOption", boost::make_shared<TradeBuilder<EquityOption>>());
    addBuilder("EquityBarrierOption", boost::make_shared<TradeBuilder<EquityBarrierOption>>());
    addBuilder("EquityDoubleBarrierOption", boost::make_shared<TradeBuilder<EquityDoubleBarrierOption>>());
    addBuilder("EquityAsianOption", boost::make_shared<TradeBuilder<EquityAsianOption>>());
    addBuilder("EquityCliquetOption", boost::make_shared<TradeBuilder<EquityCliquetOption>>());
    addBuilder("EquityEuropeanBarrierOption", boost::make_shared<TradeBuilder<EquityEuropeanBarrierOption>>());
    addBuilder("EquityDigitalOption", boost::make_shared<TradeBuilder<EquityDigitalOption>>());
    addBuilder("EquityDoubleTouchOption", boost::make_shared<TradeBuilder<EquityDoubleTouchOption>>());
    addBuilder("EquityTouchOption", boost::make_shared<TradeBuilder<EquityTouchOption>>());
    addBuilder("EquityForward", boost::make_shared<TradeBuilder<EquityForward>>());
    addBuilder("EquitySwap", boost::make_shared<TradeBuilder<EquitySwap>>());
    addBuilder("EquityVarianceSwap", boost::make_shared<TradeBuilder<EqVarSwap>>());
    addBuilder("Bond", boost::make_shared<TradeBuilder<Bond>>());
    addBuilder("ForwardBond", boost::make_shared<TradeBuilder<ForwardBond>>());
    addBuilder("CreditDefaultSwap", boost::make_shared<TradeBuilder<CreditDefaultSwap>>());
    addBuilder("CreditDefaultSwapOption", boost::make_shared<TradeBuilder<CreditDefaultSwapOption>>());
    addBuilder("CommodityForward", boost::make_shared<TradeBuilder<CommodityForward>>());
    addBuilder("CommodityOption", boost::make_shared<TradeBuilder<CommodityOption>>());
//    addBuilder("CommoditySpreadOption", boost::make_shared<TradeBuilder<CommoditySpreadOption>>());
    addBuilder("CommodityDigitalOption", boost::make_shared<TradeBuilder<CommodityDigitalOption>>());
    addBuilder("CommodityAsianOption", boost::make_shared<TradeBuilder<CommodityAsianOption>>());
    addBuilder("CommoditySwap", boost::make_shared<TradeBuilder<CommoditySwap>>());
    addBuilder("CommoditySwaption", boost::make_shared<TradeBuilder<CommoditySwaption>>());
    addBuilder("CommodityAveragePriceOption", boost::make_shared<TradeBuilder<CommodityAveragePriceOption>>());
    addBuilder("CommodityOptionStrip", boost::make_shared<TradeBuilder<CommodityOptionStrip>>());
    addBuilder("CommodityVarianceSwap", boost::make_shared<TradeBuilder<ComVarSwap>>());
    addBuilder("CommoditySpreadOption", boost::make_shared<TradeBuilder<CommoditySpreadOption>>());

    addBuilder("EquityFutureOption", boost::make_shared<TradeBuilder<EquityFutureOption>>());
    addBuilder("Failed", boost::make_shared<TradeBuilder<FailedTrade>>());
    if (extraBuilders.size() > 0)
        addExtraBuilders(extraBuilders);
}

void TradeFactory::addBuilder(const string& className, const boost::shared_ptr<AbstractTradeBuilder>& b) {
    builders_[className] = b;
}

void TradeFactory::addExtraBuilders(std::map<string, boost::shared_ptr<AbstractTradeBuilder>> extraBuilders) {
    if (extraBuilders.size() > 0) {
        LOG("adding " << extraBuilders.size() << " extra trade builders");
        for (auto eb : extraBuilders)
            addBuilder(eb.first, eb.second);
    }
}

boost::shared_ptr<Trade> TradeFactory::build(const string& className) const {
    auto it = builders_.find(className);

    if (it == builders_.end()) {
        return boost::shared_ptr<Trade>();}
    else {
        return it->second->build();}
}

} // namespace data
} // namespace ore
