/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/filteredscenarioreader.hpp
    \brief Scenario reader that filters risk factor keys by tenor
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenarioreader.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariofactory.hpp>

#include <ql/time/period.hpp>

#include <set>

namespace ore {
namespace analytics {

//! Scenario reader wrapper that filters risk factor keys to only include those matching a given tenor
/*! This class wraps an existing ScenarioReader and filters the scenarios so that only risk factor
    keys whose tenor matches the specified filter period are included. The mapping from integer index
    to tenor is derived from the ScenarioSimMarketParameters (parsed from the simulation XML Market section).

    Risk factor types that don't have a tenor dimension (e.g. FXSpot, EquitySpot) are always included.
    For multi-dimensional risk factor types (e.g. SwaptionVolatility with expiry x term x strike),
    the key is included if any of its tenor dimensions matches the filter period.

    \ingroup scenario
*/
class FilteredScenarioReader : public ScenarioReader {
public:
    /*! Constructor
        \param reader       The underlying scenario reader to wrap
        \param simParams    The simulation market parameters providing index-to-tenor mapping
        \param filterTenor  The tenor to filter on (e.g. Period(5, Years))
        \param factory      Scenario factory for creating filtered scenarios
    */
    FilteredScenarioReader(const QuantLib::ext::shared_ptr<ScenarioReader>& reader,
                           const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
                           const QuantLib::Period& filterTenor,
                           const QuantLib::ext::shared_ptr<ScenarioFactory>& factory);

    bool next() override;
    QuantLib::Date date() const override;
    QuantLib::ext::shared_ptr<Scenario> scenario() const override;

private:
    //! Build the set of allowed risk factor keys based on the filter tenor
    void buildAllowedKeys();

    //! Check if a tenor-based risk factor key at the given index matches the filter
    bool tenorMatches(const std::vector<QuantLib::Period>& tenors, QuantLib::Size index) const;

    QuantLib::ext::shared_ptr<ScenarioReader> reader_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simParams_;
    QuantLib::Period filterTenor_;
    QuantLib::ext::shared_ptr<ScenarioFactory> factory_;
    std::set<RiskFactorKey> allowedKeys_;
};

} // namespace analytics
} // namespace ore
