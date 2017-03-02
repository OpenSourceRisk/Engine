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

#include <ored/portfolio/tradefactory.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/bond.hpp>

using namespace std;

namespace ore {
namespace data {

TradeFactory::TradeFactory() {
    addBuilder("Swap", boost::make_shared<TradeBuilder<Swap>>());
    addBuilder("Swaption", boost::make_shared<TradeBuilder<Swaption>>());
    addBuilder("FxForward", boost::make_shared<TradeBuilder<FxForward>>());
    addBuilder("FxSwap", boost::make_shared<TradeBuilder<FxSwap>>());
    addBuilder("FxOption", boost::make_shared<TradeBuilder<FxOption>>());
    addBuilder("CapFloor", boost::make_shared<TradeBuilder<CapFloor>>());
    addBuilder("Bond", boost::make_shared<TradeBuilder<Bond>>());
}

void TradeFactory::addBuilder(const string& className, const boost::shared_ptr<AbstractTradeBuilder>& b) {
    builders_[className] = b;
}

boost::shared_ptr<Trade> TradeFactory::build(const string& className) const {
    auto it = builders_.find(className);
    if (it == builders_.end())
        return boost::shared_ptr<Trade>();
    else
        return it->second->build();
}
}
}
