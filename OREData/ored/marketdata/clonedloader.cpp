/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
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

#include <ored/marketdata/clonedloader.hpp>

using namespace std;
using namespace ore::data;
using namespace QuantLib;

namespace ore {
namespace data {

ClonedLoader::ClonedLoader(const Date& loaderDate, const QuantLib::ext::shared_ptr<Loader>& inLoader)
    : loaderDate_(loaderDate) {
    for (const auto& md : inLoader->loadQuotes(loaderDate)) {
        data_[loaderDate].insert(md->clone());
    }
    fixings_ = inLoader->loadFixings();
    dividends_ = inLoader->loadDividends();
}

} // namespace data
} // namespace ore
