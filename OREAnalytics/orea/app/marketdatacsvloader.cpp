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

#include <orea/app/marketdatacsvloader.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

using namespace ore::data;
using QuantExt::OptionPriceSurface;

namespace ore {
namespace analytics {

void MarketDataCsvLoaderImpl::loadCorporateActionData(QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                  const map<string, string>& equities) {
    for (const auto& div : csvLoader_->loadDividends()) {
        for (const auto& it : equities) {
            if (div.name == it.second)
                loader->addDividend(div);
        }
    }
}

void MarketDataCsvLoaderImpl::retrieveMarketData(
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
    const map<Date, set<string>>& quotes,
    const Date& requestDate) {        
    // load csvLoader quotes and add to loader if valid
    // doesn't currently handle Lagged or Mpor Date
    if (inputs_->entireMarket()) {
        for (const auto& md : csvLoader_->loadQuotes(inputs_->asof()))
            loader->add(inputs_->asof(), md->name(), md->quote()->value());
    } else {
        for (const auto& qd : quotes) {
            auto d = qd.first;
            for (const auto& q : qd.second) {
                Wildcard wc(q);
                if (!wc.hasWildcard()) {
                    // if we don't have it, it's probably optional, just leave it out for now
                    if (csvLoader_->has(q, d)) {
                        QuantLib::ext::shared_ptr<MarketDatum> datum = csvLoader_->get(q, d);
                        loader->add(d, datum->name(), datum->quote()->value());
                    } else {
                        LOG("Requested quote " << q << " for date " << d
                                               << " not in csv file. This is not necessarily an error.");
                    }

                } else {
                    // a bit messy, we loop over all quotes each time
                    for (auto md : csvLoader_->loadQuotes(d)) {
                        if (wc.matches(md->name()))
                            loader->add(d, md->name(), md->quote()->value());
                    }
                }
            }
        }
    }
}

void MarketDataCsvLoaderImpl::retrieveFixings(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        ore::analytics::FixingMap fixings,
        map<pair<string, Date>, set<Date>> lastAvailableFixingLookupMap) {

    LOG("MarketDataCsvLoader::retrieveFixings called: all fixings ? " << (inputs_->allFixings() ? "Y" : "N"));
    
    if (inputs_->allFixings()) {
        for (const auto& f : csvLoader_->loadFixings()) {
            loader->addFixing(f.date, f.name, f.fixing);
            //DLOG("add fixing for " << f.name << " fixing date " << io::iso_date(f.date));
        }
    } else {
        // Filter by required fixing data
        
        // Loop over the relevant fixings and add only them to the InMemoryLoader
        for (const auto& [name, fixingDates] : fixings) {
            // LOG("fixings are required for index " << kv.first << " and " << kv.second.size() << " dates"); 
            // map<string, set<Date>>
            for (const auto& [date, mandatory] : fixingDates) {
                // lazy loop over all of them...
                for (const auto& fix : csvLoader_->loadFixings()) {
                    if (fix.name == name && fix.date == date) {
                        // add it to the inMemory
                        loader->addFixing(fix.date, fix.name, fix.fixing);
                        //DLOG("add fixing for " << fix.name << " as of " << io::iso_date(fix.date));
                    }
                }
            }
        }
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
            WLOG("MarketDataCsvLoader::retrieveFixings(::load Could not find fixing for id " << fp.first.first << " on date "
                                                                       << fp.first.second << ". ");
        }
    }
}

} // namespace analytics
} // namespace ore
