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

void MarketDataCsvLoaderImpl::retrieveMarketData(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                 const map<Date, set<string>>& quotes, const Date& requestDate) {
    std::set<Wildcard> wildcards;
    std::set<std::string> names;

    if (!inputs_->entireMarket()) {
        if (auto q = quotes.find(requestDate); q != quotes.end()) {
            for (auto const& tmp : q->second) {
                Wildcard wc(tmp);
                if (wc.hasWildcard())
                    wildcards.insert(wc);
                else
                    names.insert(tmp);
            }
        }
    }

    for (const auto& md : csvLoader_->loadQuotes(requestDate)) {
        if (!inputs_->entireMarket()) {
            if (names.find(md->name()) != names.end() ||
                std::any_of(wildcards.begin(), wildcards.end(),
                            [&md](const Wildcard& w) { return w.matches(md->name()); }))
                loader->add(md);
        } else {
            loader->add(md);
        }
    }
}

void MarketDataCsvLoaderImpl::retrieveFixings(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        ore::analytics::FixingMap fixings,
        map<pair<string, Date>, set<Date>> lastAvailableFixingLookupMap) {
    LOG("MarketDataCsvLoader::retrieveFixings called: all fixings ? " << (inputs_->allFixings() ? "Y" : "N"));

    for (const auto& f : csvLoader_->loadFixings()) {
        if (!inputs_->allFixings()) {
            if (auto n = fixings.find(f.name); n != fixings.end()) {
                if (n->second.data().find(f.date) != n->second.data().end())
                    loader->addFixing(f.date, f.name, f.fixing);
            }
        } else {
            loader->addFixing(f.date, f.name, f.fixing);
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
            WLOG("MarketDataCsvLoader::retrieveFixings(::load Could not find fixing for id "
                 << fp.first.first << " on date " << fp.first.second << ". ");
        }
    }
}

} // namespace analytics
} // namespace ore
