/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/marketdatainmemoryloader.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

using namespace ore::data;
using QuantExt::OptionPriceSurface;

namespace ore {
namespace analytics {

void MarketDataInMemoryLoaderImpl::loadCorporateActionData(QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                           const map<string, string>& equities) {
    // TODO
}

void MarketDataInMemoryLoaderImpl::retrieveMarketData(
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
    const map<Date, set<string>>& quotes,
    const Date& relabelDate) {        

    if (inputs_->entireMarket()) {
        loadDataFromBuffers(*loader, marketData_, std::vector<std::string>(), inputs_->implyTodaysFixings());
    } else {
        QL_FAIL("MarketDataInMemoryLoaderImpl::retrieveMarketData() requires inputs_->entireMarket()");
    }
}

void MarketDataInMemoryLoaderImpl::retrieveFixings(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        map<string, RequiredFixings::FixingDates> fixings,
        map<pair<string, Date>, set<Date>> lastAvailableFixingLookupMap) {
    
    if (inputs_->allFixings()) {
        loadDataFromBuffers(*loader, std::vector<std::string>(), fixingData_, inputs_->implyTodaysFixings());
    } else {
        QL_FAIL("MarketDataInMemoryLoaderImpl::retrieveFixings() requires inputs_->allFixings()");
    }

    for (const auto& fp : lastAvailableFixingLookupMap) {
        if (loader->getFixing(fp.first.first, fp.first.second).empty()) {
            set<Date>::reverse_iterator rit;
            for (rit = fp.second.rbegin(); rit != fp.second.rend(); rit++) {
                auto f = loader->getFixing(fp.first.first, *rit);
                if (!f.empty()) {
                    loader->addFixing(fp.first.second, fp.first.first, f.fixing);
                    break;
                }
            }
            WLOG("MarketDataInMemoryLoader::retrieveFixings(::load Could not find fixing for id " << fp.first.first << " on date "
                                                                       << fp.first.second << ". ");
        }
    }
}

} // namespace analytics
} // namespace ore
