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

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup
*/

#include <boost/timer/timer.hpp>
#include <ored/marketdata/fixings.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/index.hpp>
#include <qle/indexes/equityindex.hpp>

using boost::timer::cpu_timer;
using boost::timer::default_places;
using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void applyFixings(const vector<Fixing>& fixings, const boost::shared_ptr<data::Conventions>& conventions) {
    Size count = 0;
    map<string, boost::shared_ptr<Index>> cache;
    cpu_timer timer;
    boost::shared_ptr<Index> index;
    for (auto& f : fixings) {
        try {
            auto it = cache.find(f.name);
            if (it == cache.end()) {
                index = parseIndex(f.name, conventions);
                cache[f.name] = index;
            } else {
                index = it->second;
            }
            index->addFixing(f.date, f.fixing, true);
            TLOG("Added fixing for " << f.name << " (" << io::iso_date(f.date) << ") value:" << f.fixing);
            count++;
        } catch (const std::exception& e) {
            WLOG("Error during adding fixing for " << f.name << ": " << e.what());
        }
    }
    timer.stop();
    LOG("Added " << count << " of " << fixings.size() << " fixings in " << timer.format(default_places, "%w")
                 << " seconds");
}

void applyDividends(const vector<Fixing>& dividends) {
    Size count = 0;
    map<string, boost::shared_ptr<EquityIndex>> cache;
    cpu_timer timer;
    boost::shared_ptr<EquityIndex> index;
    for (auto& f : dividends) {
        try {
            auto it = cache.find(f.name);
            if (it == cache.end()) {
                index = boost::make_shared<EquityIndex>(f.name, NullCalendar(), Currency());
                cache[f.name] = index;
            } else {
                index = it->second;
            }
            index->addDividend(f.date, f.fixing, true);
            TLOG("Added dividend for " << f.name << " (" << io::iso_date(f.date) << ") value:" << f.fixing);
            count++;
        } catch (const std::exception& e) {
            WLOG("Error during adding dividend for " << f.name << ": " << e.what());
        }
    }
    timer.stop();
    DLOG("Added " << count << " of " << dividends.size() << " dividends in " << timer.format(default_places, "%w")
                  << " seconds");
}

} // namespace data
} // namespace ore
