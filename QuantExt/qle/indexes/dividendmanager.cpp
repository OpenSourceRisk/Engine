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
#include <ql/time/calendars/nullcalendar.hpp>
#include <boost/algorithm/string/case_conv.hpp>

using namespace::std;
using namespace::QuantLib;
using boost::algorithm::to_upper_copy;

namespace QuantExt {

void applyDividends(const set<Dividend>& dividends) {
    map<string, QuantLib::ext::shared_ptr<EquityIndex2>> cache;
    QuantLib::ext::shared_ptr<EquityIndex2> index;
    std::string lastIndexName;
    for (auto& d: dividends) {
        try {
            if (lastIndexName != d.name) {
                index = QuantLib::ext::make_shared<EquityIndex2>(d.name, NullCalendar(), Currency());
                lastIndexName = d.name;
            }
            index->addDividend(d, true);
        } catch (...) {}
    }
}

bool operator<(const Dividend& d1, const Dividend& d2) {
    if (d1.name != d2.name)
        return d1.name < d2.name;
    return d1.exDate < d2.exDate;
}

bool operator==(const Dividend& d1, const Dividend& d2) { 
    return d1.exDate == d2.exDate && d1.name == d2.name; 
}

std::ostream& operator<<(std::ostream& out, Dividend dividend) {
    return out << dividend.name << "," << dividend.exDate;
}

bool DividendManager::hasHistory(const string& name) const { return data_.find(to_upper_copy(name)) != data_.end(); }

const set<Dividend>& DividendManager::getHistory(const string& name) {
    return data_[to_upper_copy(name)].value();
}

void DividendManager::setHistory(const string& name, const std::set<Dividend>& history) {
    data_[to_upper_copy(name)] = history;
}

QuantLib::ext::shared_ptr<Observable> DividendManager::notifier(const string& name) { return data_[to_upper_copy(name)]; }

void DividendManager::clearHistory(const std::string& name) { data_.erase(name); }

void DividendManager::clearHistories() { data_.clear(); }
  
}
