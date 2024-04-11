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
#include <qle/utilities/savedobservablesettings.hpp>

using boost::timer::cpu_timer;
using boost::timer::default_places;
using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void applyFixings(const set<Fixing>& fixings) {

    QuantExt::SavedObservableSettings savedObservableSettings;
    ObservableSettings::instance().disableUpdates(true);

    Size count = 0;
    cpu_timer timer;
    QuantLib::ext::shared_ptr<Index> index;
    std::string lastIndexName;
    for (auto& f : fixings) {
        if(f.name.empty()) {
            WLOG("Skipping fixing with empty name, value " << f.fixing << ", date " << f.date);
        }
        try {
            if (lastIndexName != f.name) {
                index = parseIndex(f.name);
                lastIndexName = f.name;
            }
            index->addFixing(f.date, f.fixing, true);
            TLOG("Added fixing for " << f.name << " (" << io::iso_date(f.date) << ") value:" << f.fixing);
            ++count;
        } catch (const std::exception& e) {
            WLOG("Error during adding fixing for " << f.name << ": " << e.what());
        }
    }
    timer.stop();
    LOG("Added " << count << " of " << fixings.size() << " fixings in " << timer.format(default_places, "%w")
                 << " seconds");
}

bool operator<(const Fixing& f1, const Fixing& f2) {
    if (f1.name != f2.name)
        return f1.name < f2.name;
    return f1.date < f2.date;
}

} // namespace data
} // namespace ore
