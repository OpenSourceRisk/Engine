/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <orepcapital/ored/marketdata/clonedloader.hpp>

using namespace std;
using namespace ore::data;
using namespace QuantLib;

namespace oreplus {
namespace data {

ClonedLoader::ClonedLoader(const Date& loaderDate, const boost::shared_ptr<Loader>& inLoader) :
                           loaderDate_(loaderDate) {
    // loop over all quotes in the loader for given date and store a copy
    for (auto md : inLoader->loadQuotes(loaderDate)) {
        auto tmp = md->clone();
        data_[md->name()] = tmp;
        datums_.push_back(tmp);
    }
}

const std::vector<boost::shared_ptr<MarketDatum>>& ClonedLoader::loadQuotes(const Date& d) const {
    QL_REQUIRE(d == loaderDate_, "Date " << d << " does not match loader date " << loaderDate_);
    return datums_;
}

const boost::shared_ptr<MarketDatum>& ClonedLoader::get(const string& name, const Date& d) const {
    QL_REQUIRE(d == loaderDate_, "Date " << d << " does not match loader date " << loaderDate_);
    auto it = data_.find(name);
    QL_REQUIRE(it != data_.end(), "No datum for " << name << " on date " << d);
    return it->second;
}

} // namespace data
} // namespace oreplus
