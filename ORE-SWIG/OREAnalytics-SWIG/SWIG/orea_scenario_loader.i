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

#ifndef orea_scenario_loader_i
#define orea_scenario_loader_i

%include orea_scenario_ext.i

%{
#include <orea/scenario/scenarioreader.hpp>
#include <orea/scenario/scenariofilereader.hpp>
#include <orea/scenario/scenarioloader.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
%}

// STL templates needed by this module
%template(ScenarioVector) std::vector<ext::shared_ptr<QuantExt::Scenario>>;
%template(DateSet) std::set<QuantLib::Date>;

// --- ScenarioReader (abstract base) ---
%shared_ptr(ore::analytics::ScenarioReader)
%nodefaultctor ore::analytics::ScenarioReader;

namespace ore { namespace analytics {
class ScenarioReader {
public:
    virtual ~ScenarioReader() {}
    virtual bool next() = 0;
    virtual QuantLib::Date date() const = 0;
    virtual ext::shared_ptr<QuantExt::Scenario> scenario() const = 0;
};
}}

// --- ScenarioCSVReader (skip direct construction - requires CSVReader) ---
%shared_ptr(ore::analytics::ScenarioCSVReader)
%nodefaultctor ore::analytics::ScenarioCSVReader;

namespace ore { namespace analytics {
class ScenarioCSVReader : public ScenarioReader {
public:
    ~ScenarioCSVReader() override;
    bool next() override;
    QuantLib::Date date() const override;
    ext::shared_ptr<QuantExt::Scenario> scenario() const override;
};
}}

// --- ScenarioFileReader ---
%shared_ptr(ore::analytics::ScenarioFileReader)

namespace ore { namespace analytics {
class ScenarioFileReader : public ScenarioCSVReader {
public:
    ScenarioFileReader(const std::string& file,
                       const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory);
    ~ScenarioFileReader() override;
};
}}

// --- ScenarioBufferReader ---
%shared_ptr(ore::analytics::ScenarioBufferReader)

namespace ore { namespace analytics {
class ScenarioBufferReader : public ScenarioCSVReader {
public:
    ScenarioBufferReader(const std::string& buffer,
                         const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory);
};
}}

// --- ScenarioLoader ---
%shared_ptr(ore::analytics::ScenarioLoader)

namespace ore { namespace analytics {
class ScenarioLoader {
public:
    ScenarioLoader();
    QuantLib::Size numScenarios() const;
    void add(const QuantLib::Date& date, QuantLib::Size index,
             const ext::shared_ptr<QuantExt::Scenario>& scenario);
};
}}

// --- SimpleScenarioLoader ---
%shared_ptr(ore::analytics::SimpleScenarioLoader)

namespace ore { namespace analytics {
class SimpleScenarioLoader : public ScenarioLoader {
public:
    SimpleScenarioLoader();
    SimpleScenarioLoader(
        const ext::shared_ptr<ore::analytics::ScenarioReader>& scenarioReader);
    QuantLib::Size samples();
};
}}

// --- HistoricalScenarioLoader ---
%shared_ptr(ore::analytics::HistoricalScenarioLoader)

namespace ore { namespace analytics {
class HistoricalScenarioLoader : public ScenarioLoader {
public:
    HistoricalScenarioLoader();

    HistoricalScenarioLoader(
        const ext::shared_ptr<ore::analytics::ScenarioReader>& scenarioReader,
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        const QuantLib::Calendar& calendar);

    HistoricalScenarioLoader(
        const ext::shared_ptr<ore::analytics::ScenarioReader>& scenarioReader,
        const std::set<QuantLib::Date>& dates);

    HistoricalScenarioLoader(
        const std::vector<ext::shared_ptr<QuantExt::Scenario>>& scenarios,
        const std::set<QuantLib::Date>& dates);

    ext::shared_ptr<QuantExt::Scenario> getScenario(
        const QuantLib::Date& date) const;

    std::vector<QuantLib::Date> dates();
};
}}

// --- SimpleScenarioFactory ---
%shared_ptr(ore::analytics::SimpleScenarioFactory)

namespace ore { namespace analytics {
class SimpleScenarioFactory : public ScenarioFactory {
public:
    explicit SimpleScenarioFactory(bool useCommonSharedDataBlock = false);
    const ext::shared_ptr<QuantExt::Scenario> buildScenario(
        QuantLib::Date asof, bool isAbsolute, bool isPar = false,
        const std::string& label = "", QuantLib::Real numeraire = 0.0) const override;
};
}}

#endif
