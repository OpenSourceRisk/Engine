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

#include <ored/model/camcorrelationsfromindexcorrelations.hpp>
#include <ored/model/utilities.hpp>

using namespace QuantLib;

namespace ore::data {

map<CorrelationKey, Handle<Quote>> getCamCorrelationsFromIndexCorrelations(
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& indexCorrelations,
    const std::string& infModelType) {

    // compile cam correlation matrix
    // - we want to use the maximum tenor of an ir index in a correlation pair if several are given (to have
    //   a well defined rule how to derive the LGM IR correlations); to get there we store the correlations
    //   together with the index tenors (or null if not applicatble), so that we can decide if we overwrite
    //   an existing correlation with another candidate or not
    // - correlations are for index pair names and must be constant; if not given for a pair, we assume zero
    //   correlation;
    // - correlations for IR processes are taken from IR index correlations, if several indices exist
    //   for one ccy, the index with the longest tenor T is selected; we do not apply an LGM adjustment for this
    //   value due to time dependent volatility, since this is usually negligible
    // - for inf we do not apply an adjustment either => TODO is that suitable for both DK and JY approximately?
    // - for inf JY we have two driving factors (f1,f2) where f1 drives the real rate process and f2 drives the
    //   inflation index ("fx") process; we assume the correlation between f1 and any other factor (including f2)
    //   to be zero and set the correlation between f2 and any other factor to the correlation read from the
    //   market data for the inflation index, TODO is that assumption reasonable?
    std::map<std::pair<std::string, std::string>,
             std::tuple<Handle<QuantExt::CorrelationTermStructure>, Period, Period>>
        tmpCorrelations;
    for (auto const& c : indexCorrelations) {
        std::pair<std::string, Period> firstEntry = convertIndexToCamCorrelationEntry(c.first.first);
        std::pair<std::string, Period> secondEntry = convertIndexToCamCorrelationEntry(c.first.second);
        // if we have identical CAM entries (e.g. IR:EUR, IR:EUR) we skip this pair, since we can't specify a
        // correlation in this case
        if (firstEntry.first == secondEntry.first)
            continue;
        auto e = tmpCorrelations.find(std::make_pair(firstEntry.first, secondEntry.first));
        if (e == tmpCorrelations.end() ||
            (firstEntry.second > std::get<1>(e->second) && secondEntry.second > std::get<2>(e->second))) {
            tmpCorrelations[std::make_pair(firstEntry.first, secondEntry.first)] =
                std::make_tuple(c.second, firstEntry.second, secondEntry.second);
        }
    }

    map<CorrelationKey, Handle<Quote>> camCorrelations;
    for (auto const& c : tmpCorrelations) {
        CorrelationFactor f_1 = parseCorrelationFactor(c.first.first, '#');
        CorrelationFactor f_2 = parseCorrelationFactor(c.first.second, '#');
        // update index for JY from 0 to 1 (i.e. to the factor driving the inf index ("fx") process)
        // in all other cases the index 0 is fine, since there is only one driving factor always
        if (infModelType == "JY") {
            if (f_1.type == CrossAssetModel::AssetType::INF)
                f_1.index = 1;
            if (f_2.type == CrossAssetModel::AssetType::INF)
                f_2.index = 1;
        }
        auto q = Handle<Quote>(QuantLib::ext::make_shared<CorrelationValue>(std::get<0>(c.second), 0.0));
        camCorrelations[std::make_pair(f_1, f_2)] = q;
        DLOG("added correlation for " << c.first.first << "/" << c.first.second << ": " << q->value());
    }

    return camCorrelations;
}

} // namespace ore::data
