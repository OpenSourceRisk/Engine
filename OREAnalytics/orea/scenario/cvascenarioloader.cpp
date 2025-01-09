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

#include <orea/scenario/cvascenarioloader.hpp>

using namespace std;
using namespace QuantLib;
using namespace ore::data;

namespace ore {
namespace analytics {

CvaScenarioLoader::CvaScenarioLoader(const Date& loaderDate, const QuantLib::ext::shared_ptr<Loader>& inLoader) :
        ClonedLoader(loaderDate, inLoader) {
        
    // generate a base scenario
    QuantLib::ext::shared_ptr<CvaScenario> baseScenario = QuantLib::ext::make_shared<CvaScenario>();
    for (auto q : inLoader->loadQuotes(loaderDate)) {
        baseScenario->add(q->name(), q->quote()->value());
    }
    baseScenario_ = baseScenario;
};

void CvaScenarioLoader::setBaseScenario(const QuantLib::ext::shared_ptr<ore::analytics::CvaScenario>& baseScenario) {
    baseScenario_ = baseScenario;
}

void CvaScenarioLoader::reset() {
    if (alteredKeys_.size() > 0) {
        for (auto k : alteredKeys_) {
            Real value = baseScenario_->get(k);

            //update the market datum
            updateMarketDatum(k, value);

            // remove from altered keys
	    // this causes a seg fault, so skip this and clear the altered keys after the loop
            // alteredKeys_.erase(k);
        }
	alteredKeys_.clear();
    }
}

void CvaScenarioLoader::applyScenario(const QuantLib::ext::shared_ptr<ore::analytics::CvaScenario>& scenario) {
    
    DLOG("CvaScenarioLoader::applyScenario called");

    // first reset any values back to base scenario
    reset();

    // loop over all keys and update the value in the loader
    set<string> keys = scenario->keys();
    DLOG("Loop over " << keys.size() << " keys");
    for (auto k : keys) {
        DLOG("Processing key " << k);
	Real value = scenario->get(k);

        //update the market datum
        updateMarketDatum(k, value);

        // add to alteredKeys to track changes
        alteredKeys_.insert(k);
    }
}

void CvaScenarioLoader::updateMarketDatum(std::string key, QuantLib::Real value) {
    auto datum = get(key, loaderDate_);
    QuantLib::ext::shared_ptr<SimpleQuote> sq = QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(datum->quote().currentLink());
    if (sq)
        sq->setValue(value);
}
} // namespace data
} // namespace ore
