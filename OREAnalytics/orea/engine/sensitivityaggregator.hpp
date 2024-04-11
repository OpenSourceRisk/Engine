/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/engine/sensitivityaggregator.hpp
    \brief Class for aggregating SensitivityRecords
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

#include <functional>
#include <map>
#include <set>
#include <string>

namespace ore {
namespace analytics {

/*! Class for aggregating SensitivityRecords.

    The SensitivityRecords are aggregated according to categories of predefined trade IDs.
*/
class SensitivityAggregator {
public:
    /*! Constructor that uses sets of trades to define the aggregation categories.

        The \p categories map has a string key that defines the name of the category and a value
        that defines the set of trade IDs in that category.
    */
    SensitivityAggregator(const std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>>& categories);

    /*! Constructor that uses functions to define the aggregation categories.

        The \p categories map has a string key that defines the name of the category. The map value
        is a function that when given a trade ID, returns a bool indicating if the trade ID is in the
        category.
    */
    SensitivityAggregator(const std::map<std::string, std::function<bool(std::string)>>& categories);

    /*! Update the aggregator with SensitivityRecords from the stream \p ss after applying the
        optional filter. If no filter is specified, all risk factors are aggregated.

        \warning No checks are performed for duplicate records from the stream. It is the stream's
                 responsibility to guard against duplicates if it needs to.
    */
    void aggregate(SensitivityStream& ss, const QuantLib::ext::shared_ptr<ScenarioFilter>& filter =
                                              QuantLib::ext::make_shared<ScenarioFilter>());

    //! Reset the aggregator to it's initial state by clearing all aggregations
    void reset();

    /*! Return the set of aggregated sensitivities for the given \p category
     */
    const std::set<SensitivityRecord>& sensitivities(const std::string& category) const;

    /*! Return the deltas and gammas for the given \p category
     */
    typedef std::pair<RiskFactorKey, RiskFactorKey> CrossPair;
    void generateDeltaGamma(const std::string& category, std::map<RiskFactorKey, QuantLib::Real>& deltas,
                             std::map<CrossPair, QuantLib::Real>& gammas);

private:
    /*! Container for category names and their definition via sets. This will be
        empty if constructor is provided functions directly.
    */
    std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> setCategories_;
    //! Container for category names and their definition via functions
    std::map<std::string, std::function<bool(std::string)>> categories_;
    //! Sensitivity records aggregated according to <code>categories_</code>
    std::map<std::string, std::set<SensitivityRecord>> aggRecords_;

    //! Initialise the container of aggregated records
    void init();
    //! Add a sensitivity record to the set of aggregated \p records
    void add(SensitivityRecord& sr, std::set<SensitivityRecord>& records);
    //! Determine if the \p tradeId is in the given \p category
    bool inCategory(const std::string& tradeId, const std::string& category) const;
};

} // namespace analytics
} // namespace ore
