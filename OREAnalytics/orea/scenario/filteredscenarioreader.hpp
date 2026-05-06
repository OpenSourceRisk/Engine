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
    \brief Scenario reader that filters risk factor keys
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenarioreader.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariofactory.hpp>

#include <boost/optional.hpp>
#include <ql/time/period.hpp>

#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

//! Generic scenario reader wrapper that filters risk factor keys based on an allowed set
/*! This class wraps an existing ScenarioReader and filters the scenarios so that only risk factor
    keys present in the provided allowed set are included in the output scenarios.

    \ingroup scenario
*/
class FilteredScenarioReader : public ScenarioReader {
public:
    /*! Constructor
        \param reader       The underlying scenario reader to wrap
        \param allowedKeys  The set of risk factor keys to include
        \param factory      Scenario factory for creating filtered scenarios
    */
    FilteredScenarioReader(const QuantLib::ext::shared_ptr<ScenarioReader>& reader,
                           const std::set<RiskFactorKey>& allowedKeys,
                           const QuantLib::ext::shared_ptr<ScenarioFactory>& factory);

    void load(const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
             const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& marketParams) override;
    bool next() override;
    QuantLib::Date date() const override;
    QuantLib::ext::shared_ptr<Scenario> scenario() const override;

protected:
    QuantLib::ext::shared_ptr<ScenarioReader> reader_;
    std::set<RiskFactorKey> allowedKeys_;
    QuantLib::ext::shared_ptr<ScenarioFactory> factory_;
};

//! Scenario reader wrapper that filters risk factor keys to only include those matching a given tenor
/*! This class derives from FilteredScenarioReader and builds the allowed key set from the
    ScenarioSimMarketParameters by selecting only keys whose tenor dimension matches the filter period.

    Risk factor types that don't have a tenor dimension (e.g. FXSpot, EquitySpot) are always included.
    For multi-dimensional risk factor types (e.g. SwaptionVolatility with expiry x term x strike),
    the key is included if any of its tenor dimensions matches the filter period.

    \ingroup scenario
*/
class TenorFilteredScenarioReader : public FilteredScenarioReader {
public:
    /*! Constructor with default tenor and optional per-key overrides
        \param reader         The underlying scenario reader to wrap
        \param simParams      The simulation market parameters providing index-to-tenor mapping
        \param filterTenor    The default tenor to filter on (e.g. Period(5, Years))
        \param factory        Scenario factory for creating filtered scenarios
        \param tenorOverrides Per-risk-factor tenor overrides. Key format is "KeyType/Name"
                              (e.g. "DiscountCurve/USD"). If a risk factor matches an override,
                              the override tenor is used instead of the default filterTenor.
    */
    TenorFilteredScenarioReader(const QuantLib::ext::shared_ptr<ScenarioReader>& reader,
                                const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
                                const QuantLib::Period& filterTenor,
                                const QuantLib::ext::shared_ptr<ScenarioFactory>& factory,
                                const std::map<std::string, QuantLib::Period>& tenorOverrides = {});

    /*! Constructor with regex-based tenor filter dictionary.
        Entries are matched in reverse order (last entry first). The first regex that matches
        a risk factor key determines the tenor filter for that key. If no regex matches,
        all tenors for that risk factor are included (no filtering).
        \param reader         The underlying scenario reader to wrap
        \param simParams      The simulation market parameters providing index-to-tenor mapping
        \param factory        Scenario factory for creating filtered scenarios
        \param regexTenors    Ordered list of (regex_pattern, tenor) pairs. Matched bottom-up.
    */
    TenorFilteredScenarioReader(const QuantLib::ext::shared_ptr<ScenarioReader>& reader,
                                const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
                                const QuantLib::ext::shared_ptr<ScenarioFactory>& factory,
                                const std::vector<std::pair<std::string, QuantLib::Period>>& regexTenors);

private:
    //! Build the set of allowed risk factor keys based on the filter tenor
    void buildAllowedKeys();

    //! Get the effective tenor for a given key type and name (uses override if present, else default)
    QuantLib::Period effectiveTenor(RiskFactorKey::KeyType keyType, const std::string& name) const;

    //! Get the effective tenor using regex matching (reverse order). Returns boost::none if no match.
    boost::optional<QuantLib::Period> effectiveTenorRegex(RiskFactorKey::KeyType keyType, const std::string& name) const;

    //! Check if a tenor-based risk factor key at the given index matches the filter
    bool tenorMatches(const std::vector<QuantLib::Period>& tenors, QuantLib::Size index) const;

    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simParams_;
    QuantLib::Period filterTenor_;
    std::map<std::string, QuantLib::Period> tenorOverrides_;
    std::vector<std::pair<std::regex, QuantLib::Period>> regexTenors_;
    bool useRegexMode_ = false;
};

} // namespace analytics
} // namespace ore
