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
#include <orea/scenario/historicalscenarioreturn.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
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

// --- ReturnConfiguration ---
%rename(ReturnConfigurationReturnType) ore::analytics::ReturnConfiguration::ReturnType;
%feature("flatnested") ReturnType;

%shared_ptr(ore::analytics::ReturnConfiguration)

namespace ore { namespace analytics {
class ReturnConfiguration : public ore::data::XMLSerializable {
public:
    enum class ReturnType { Absolute, Relative, Log };

    ReturnConfiguration();
    explicit ReturnConfiguration(
        const std::map<QuantExt::RiskFactorKey::KeyType, ReturnType>& returnType);

    QuantLib::Real returnValue(const QuantExt::RiskFactorKey& key,
                               QuantLib::Real v1, QuantLib::Real v2,
                               const QuantLib::Date& d1, const QuantLib::Date& d2) const;
    QuantLib::Real applyReturn(const QuantExt::RiskFactorKey& key,
                               QuantLib::Real baseValue,
                               QuantLib::Real returnValue) const;

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};
}}

// ReturnConfiguration::Return is a nested struct, which SWIG cannot wrap as a
// proxy class (confirmed: no class is generated, so returnType() would come
// back as an unusable opaque handle with no readable fields). Flatten it
// into two accessors on ReturnConfiguration instead.
%extend ore::analytics::ReturnConfiguration {
    int returnTypeValue(const QuantExt::RiskFactorKey& key) const {
        return (int)self->returnType(key).type;
    }
    double returnDisplacement(const QuantExt::RiskFactorKey& key) const {
        return self->returnType(key).displacement;
    }
}

%template(KeyTypeReturnTypeMap) std::map<QuantExt::RiskFactorKey::KeyType, ore::analytics::ReturnConfiguration::ReturnType>;

// --- HistoricalScenarioGenerator ---
%shared_ptr(ore::analytics::HistoricalScenarioGenerator)

// Python-friendly base scenario setter (the C++ non-const ref overload is unusable from Python)
%extend ore::analytics::HistoricalScenarioGenerator {
    void setBaseScenario(const ext::shared_ptr<QuantExt::Scenario>& s) {
        self->baseScenario() = s;
    }
}

namespace ore { namespace analytics {
class HistoricalScenarioGenerator : public ScenarioGenerator {
public:
    HistoricalScenarioGenerator(
        const ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& historicalScenarioLoader,
        const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory,
        const ext::shared_ptr<ore::analytics::ReturnConfiguration>& returnConfiguration,
        const QuantLib::Calendar& cal,
        const ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors =
            ext::shared_ptr<ore::data::AdjustmentFactors>(),
        QuantLib::Size mporDays = 10,
        bool overlapping = true,
        const std::string& labelPrefix = "",
        bool generateDifferenceScenarios = false,
        bool riskFactorBreakdown = false);

    HistoricalScenarioGenerator(
        const ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& historicalScenarioLoader,
        const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory,
        const ext::shared_ptr<ore::analytics::ReturnConfiguration>& returnConfiguration,
        const ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors =
            ext::shared_ptr<ore::data::AdjustmentFactors>(),
        const std::string& labelPrefix = "",
        bool generateDifferenceScenarios = false,
        bool riskFactorBreakdown = false);

    ext::shared_ptr<QuantExt::Scenario>& baseScenario();
    const QuantLib::Calendar& cal() const;
    QuantLib::Size mporDays() const;
    bool overlapping() const;

    ext::shared_ptr<QuantExt::Scenario> next(const QuantLib::Date& d) override;
    void reset() override;

    QuantLib::Size numScenarios() const;
    const std::vector<QuantLib::Date>& startDates() const;
    const std::vector<QuantLib::Date>& endDates() const;

    const ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& scenarioLoader() const;
    const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory() const;
    const ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors() const;
    const std::string& labelPrefix() const;

    void setGenerateDifferenceScenarios(bool b);
    bool generateDifferenceScenarios() const;

    void setRiskFactorBreakdown(const bool b);
    bool isRiskFactorBreakdown() const;
};
}}

// --- HistoricalScenarioGeneratorRandom ---
%shared_ptr(ore::analytics::HistoricalScenarioGeneratorRandom)

namespace ore { namespace analytics {
class HistoricalScenarioGeneratorRandom : public HistoricalScenarioGenerator {
public:
    HistoricalScenarioGeneratorRandom(
        const ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& historicalScenarioLoader,
        const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory,
        const ext::shared_ptr<ore::analytics::ReturnConfiguration>& returnConfiguration,
        const QuantLib::Calendar& cal,
        const ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors =
            ext::shared_ptr<ore::data::AdjustmentFactors>(),
        QuantLib::Size mporDays = 10,
        bool overlapping = true);

    ext::shared_ptr<QuantExt::Scenario> next(const QuantLib::Date& d) override;
    void reset() override;
};
}}

// --- HistoricalScenarioGeneratorTransform ---
// The C++ ctor takes hsg by non-const ref; %extend provides a const-ref overload usable from Python.
%shared_ptr(ore::analytics::HistoricalScenarioGeneratorTransform)

%extend ore::analytics::HistoricalScenarioGeneratorTransform {
    HistoricalScenarioGeneratorTransform(
        const ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>& hsg,
        const ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
        const ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketConfig)
    {
        auto hsg_ref = const_cast<ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>&>(hsg);
        return new ore::analytics::HistoricalScenarioGeneratorTransform(hsg_ref, simMarket, simMarketConfig);
    }
}

namespace ore { namespace analytics {
class HistoricalScenarioGeneratorTransform : public HistoricalScenarioGenerator {
public:
    ext::shared_ptr<QuantExt::Scenario> next(const QuantLib::Date& d) override;
};
}}

// --- ScenarioWriter ---
%shared_ptr(ore::analytics::ScenarioWriter)

%template(RiskFactorKeyVector) std::vector<QuantExt::RiskFactorKey>;

namespace ore { namespace analytics {
class ScenarioWriter : public ScenarioGenerator {
public:
    // File-based ctor with source generator
    ScenarioWriter(const ext::shared_ptr<ore::analytics::ScenarioGenerator>& src,
                   const std::string& filename,
                   const char sep = ',',
                   const std::string& filemode = "w+");

    // File-based ctor for writing single scenarios
    ScenarioWriter(const std::string& filename,
                   const char sep = ',',
                   const std::string& filemode = "w+");

    virtual ~ScenarioWriter();

    ext::shared_ptr<QuantExt::Scenario> next(const QuantLib::Date& d) override;
    void writeScenario(const ext::shared_ptr<QuantExt::Scenario>& s, bool writeHeader);
    void reset() override;
    void close();
};
}}

// --- ScenarioShiftCalculator ---
%shared_ptr(ore::analytics::ScenarioShiftCalculator)

namespace ore { namespace analytics {
class ScenarioShiftCalculator {
public:
    ScenarioShiftCalculator(const ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityConfig,
                            const ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketConfig,
                            const ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket =
                                ext::shared_ptr<ore::analytics::ScenarioSimMarket>());

    QuantLib::Real shift(const ore::analytics::RiskFactorKey& key, 
                         const ore::analytics::Scenario& s_1,
                         const ore::analytics::Scenario& s_2, 
                         const bool isPar = false) const;
};
}}

#endif
