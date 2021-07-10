/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ored/portfolio/commodityasianoption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/portfolio/equityasianoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityfuturesoption.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxasianoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/utilities/log.hpp>

using namespace std;

namespace ore {
namespace data {

TradeFactory::TradeFactory(std::map<string, boost::shared_ptr<AbstractTradeBuilder>> extraBuilders) {
    addBuilder("Swap", boost::make_shared<TradeBuilder<Swap>>());
    addBuilder("Swaption", boost::make_shared<TradeBuilder<Swaption>>());
    addBuilder("FxForward", boost::make_shared<TradeBuilder<FxForward>>());
    addBuilder("ForwardRateAgreement", boost::make_shared<TradeBuilder<ForwardRateAgreement>>());
    addBuilder("FxSwap", boost::make_shared<TradeBuilder<FxSwap>>());
    addBuilder("FxOption", boost::make_shared<TradeBuilder<FxOption>>());
    addBuilder("FxAsianOption", boost::make_shared<TradeBuilder<FxAsianOption>>());
    addBuilder("CapFloor", boost::make_shared<TradeBuilder<CapFloor>>());
    addBuilder("EquityOption", boost::make_shared<TradeBuilder<EquityOption>>());
    addBuilder("EquityAsianOption", boost::make_shared<TradeBuilder<EquityAsianOption>>());
    addBuilder("EquityForward", boost::make_shared<TradeBuilder<EquityForward>>());
    addBuilder("EquitySwap", boost::make_shared<TradeBuilder<EquitySwap>>());
    addBuilder("Bond", boost::make_shared<TradeBuilder<Bond>>());
    addBuilder("ForwardBond", boost::make_shared<TradeBuilder<ForwardBond>>());
    addBuilder("CreditDefaultSwap", boost::make_shared<TradeBuilder<CreditDefaultSwap>>());
    addBuilder("CreditDefaultSwapOption", boost::make_shared<TradeBuilder<CreditDefaultSwapOption>>());
    addBuilder("CommodityForward", boost::make_shared<TradeBuilder<CommodityForward>>());
    addBuilder("CommodityOption", boost::make_shared<TradeBuilder<CommodityOption>>());
    addBuilder("CommodityAsianOption", boost::make_shared<TradeBuilder<CommodityAsianOption>>());
    addBuilder("EquityFutureOption", boost::make_shared<TradeBuilder<EquityFutureOption>>());
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
