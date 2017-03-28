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

#include <orea/simulation/simmarket.hpp>

namespace ore {
namespace analytics {

void SimMarket::cacheFixings(const map<string, vector<Date>>& fixingMap,
			     const vector<boost::shared_ptr<Index>>& indices) {
    fixingMap_.clear();
    fixingMap_ = fixingMap;
    indices_.clear();
    indices_ = indices;
    fixingCache_.clear();
    for (const boost::shared_ptr<Index>& index : indices_) {
      string qlIndexName = index->name();
      if (fixingMap_.find(qlIndexName) != fixingMap_.end()) {
	fixingCache_[qlIndexName] = IndexManager::instance().getHistory(qlIndexName);
      }
    }
}

void SimMarket::resetFixings() {
    for (auto& kv : fixingCache_)
        IndexManager::instance().setHistory(kv.first, kv.second);
}

void SimMarket::applyFixings(Date start, Date end) {
    // Loop over all indices
    for (const boost::shared_ptr<Index>& index : indices_) {
        string qlIndexName = index->name();
        // Only need to apply fixings if the index is in fixingMap
        if (fixingMap_.find(qlIndexName) != fixingMap_.end()) {

            // Add we have a coupon between start and asof.
            bool needFixings = false;
            vector<Date>& fixingDates = fixingMap_[qlIndexName];
            for (Size i = 0; i < fixingDates.size(); i++) {
                if (fixingDates[i] >= start && fixingDates[i] < end) {
                    needFixings = true;
                    break;
                }
            }

            if (needFixings) {
                Date currentFixingDate = index->fixingCalendar().adjust(end, Following);
                Rate currentFixing = index->fixing(currentFixingDate);
                TimeSeries<Real> history;
                vector<Date>& fixingDates = fixingMap_[qlIndexName];
                for (Size i = 0; i < fixingDates.size(); i++) {
                    if (fixingDates[i] >= start && fixingDates[i] < end)
                        history[fixingDates[i]] = currentFixing;
                    if (fixingDates[i] >= end)
                        break;
                }
                index->addFixings(history, true);
            }
        }
    }
}

}
}
