/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup
*/

#include <ored/marketdata/fixings.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/index.hpp>
#include <boost/timer.hpp>

using namespace std;
using namespace QuantLib;

namespace openriskengine {
namespace data {

void applyFixings(const vector<Fixing>& fixings) {
    Size count = 0;
    map<string, boost::shared_ptr<Index>> cache;
    boost::timer timer;
    boost::shared_ptr<Index> index;
    for (auto& f : fixings) {
        try {
            auto it = cache.find(f.name);
            if (it == cache.end()) {
                index = parseIndex(f.name);
                cache[f.name] = index;
            } else {
                index = it->second;
            }
            index->addFixing(f.date, f.fixing, true);
            DLOG("Added fixing for " << f.name << " (" << io::iso_date(f.date) << ") value:" << f.fixing);
            count++;
        } catch (...) {
        }
    }
    DLOG("Added " << count << " of " << fixings.size() << " fixings in " << timer.elapsed() << " seconds");
}
}
}
