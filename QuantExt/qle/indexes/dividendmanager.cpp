/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/indexes/dividendmanager.hpp>
#include <qle/indexes/equityindex.hpp>

using namespace::std;
using namespace::QuantLib;

namespace QuantExt {

void applyDividends(const set<Dividend>& dividends) {
    Size count = 0;
    map<string, boost::shared_ptr<EquityIndex>> cache;
    boost::shared_ptr<EquityIndex> index;
    std::string lastIndexName;
    for (auto& f : dividends) {
        try {
            if (lastIndexName != f.name) {
                index = boost::make_shared<EquityIndex>(f.name, NullCalendar(), Currency());
                lastIndexName = f.name;
            }
            index->addDividend(f.date, f.fixing, true);
            count++;
        } catch (const std::exception& e) {
        }
    }
}

bool operator<(const Dividend& f1, const Dividend& f2) {
    if (f1.name != f2.name)
        return f1.name < f2.name;
    return f1.date < f2.date;
}

}
