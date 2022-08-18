/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ored/marketdata/clonedloader.hpp>

using namespace std;
using namespace ore::data;
using namespace QuantLib;

namespace ore {
namespace data {

ClonedLoader::ClonedLoader(const Date& loaderDate, const boost::shared_ptr<Loader>& inLoader)
    : loaderDate_(loaderDate) {
    for (const auto& md : inLoader->loadQuotes(loaderDate)) {
        data_[loaderDate].insert(md->clone());
    }
    fixings_ = inLoader->loadFixings();
    dividends_ = inLoader->loadDividends();
}

} // namespace data
} // namespace ore
