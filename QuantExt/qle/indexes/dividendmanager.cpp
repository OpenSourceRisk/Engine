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
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

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

const set<Dividend>& DividendManager::getHistory(const string& name) { return data_[to_upper_copy(name)]; }

void DividendManager::setHistory(const string& name, const std::set<Dividend>& history) {
    notifier(name)->notifyObservers();
    data_[to_upper_copy(name)] = history;
}

void DividendManager::addDividend(const std::string& name, const Dividend& dividend, const bool forceOverwrite) {
    auto& divs = data_[name];
    if (!forceOverwrite) {
        bool duplicateFixing = false;
        for (const auto& d : divs) {
            if (d == dividend)
                duplicateFixing = true;
        }
        QL_REQUIRE(!duplicateFixing, "At least one duplicated fixing provided: ("
                                         << dividend.name << ", " << dividend.exDate << ", " << dividend.rate << ")");
    }
    divs.insert(dividend);
    notifier(name)->notifyObservers();
}

QuantLib::ext::shared_ptr<Observable> DividendManager::notifier(const string& name) {
    auto n = notifiers_.find(name);
    if (n != notifiers_.end())
        return n->second;
    auto o = ext::make_shared<Observable>();
    notifiers_[name] = o;
    return o;
}

void DividendManager::clearHistory(const std::string& name) {
    notifier(name)->notifyObservers();
    data_.erase(name);
}

void DividendManager::clearHistories() {
    for (auto const& d : data_)
        notifier(d.first)->notifyObservers();
    data_.clear();
}

template <class Archive> void Dividend::serialize(Archive& ar, const unsigned int version) {
    ar& exDate;
    ar& name;
    ar& rate;
    ar& payDate;
    ar& announcementDate;
}

template void Dividend::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void Dividend::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
  
} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::Dividend);
