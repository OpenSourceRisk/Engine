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

#ifndef orea_sensitivity_i
#define orea_sensitivity_i

%include stl.i
%include types.i
%include orea_scenario_ext.i
%include orea_cube.i
%include ored_portfolio.i
%include ored_reports.i

%{
#include <sstream>
#include <orea/engine/sensitivityrecord.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/bufferedsensitivitystream.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/decomposedsensitivitystream.hpp>
#include <orea/engine/sensitivityreportstream.hpp>
%}

// --- SensitivityRecord (value struct, no shared_ptr) ---

namespace ore { namespace analytics {

struct SensitivityRecord {
    std::string tradeId;
    bool isPar;
    QuantExt::RiskFactorKey key_1;
    std::string desc_1;
    QuantLib::Real shift_1;
    QuantExt::RiskFactorKey key_2;
    std::string desc_2;
    QuantLib::Real shift_2;
    std::string currency;
    std::string tradeCurrency;
    QuantLib::Real baseNpv;
    QuantLib::Real delta;
    QuantLib::Real gamma;

    SensitivityRecord();

    SensitivityRecord(const std::string& tradeId, bool isPar,
                      const QuantExt::RiskFactorKey& key_1, const std::string& desc_1,
                      QuantLib::Real shift_1,
                      const QuantExt::RiskFactorKey& key_2, const std::string& desc_2,
                      QuantLib::Real shift_2,
                      const std::string& currency,
                      QuantLib::Real baseNpv, QuantLib::Real delta, QuantLib::Real gamma);

    bool isCrossGamma() const;

    %extend {
        bool __bool__() const { return static_cast<bool>(*$self); }
        std::string __repr__() const {
            std::ostringstream oss;
            oss << *$self;
            return oss.str();
        }
    }
};

}}

%template(SensitivityRecordVector) std::vector<ore::analytics::SensitivityRecord>;

// --- SensitivityStream (abstract base) ---

%shared_ptr(ore::analytics::SensitivityStream)
%nodefaultctor ore::analytics::SensitivityStream;

namespace ore { namespace analytics {

class SensitivityStream {
public:
    virtual ~SensitivityStream() {}
    virtual ore::analytics::SensitivityRecord next() = 0;
    virtual void reset() = 0;

    %extend {
        std::vector<ore::analytics::SensitivityRecord> readAll() {
            std::vector<ore::analytics::SensitivityRecord> records;
            $self->reset();
            while (true) {
                auto rec = $self->next();
                if (!rec) break;
                records.push_back(rec);
            }
            return records;
        }
    }

    #if defined(SWIGPYTHON)
    %pythoncode %{
    def __iter__(self):
        self.reset()
        return self
    def __next__(self):
        r = self.next()
        if not r:
            raise StopIteration
        return r
    %}
    #endif
};

}}

// --- SensitivityCubeStream ---

%shared_ptr(ore::analytics::SensitivityCubeStream)

namespace ore { namespace analytics {

class SensitivityCubeStream : public ore::analytics::SensitivityStream {
public:
    SensitivityCubeStream(const QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>& cube,
                          const std::string& currency,
                          const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio = nullptr);
    SensitivityCubeStream(const std::vector<QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>>& cubes,
                          const std::string& currency,
                          const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio = nullptr);
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- SensitivityFileStream ---

%shared_ptr(ore::analytics::SensitivityFileStream)

namespace ore { namespace analytics {

class SensitivityFileStream : public ore::analytics::SensitivityStream {
public:
    SensitivityFileStream(const std::string& fileName,
                          char delim = ',', char comment = '#',
                          char quoteChar = '\0', char escapeChar = '\\');
    ~SensitivityFileStream() override;
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- SensitivityBufferStream ---

%shared_ptr(ore::analytics::SensitivityBufferStream)

namespace ore { namespace analytics {

class SensitivityBufferStream : public ore::analytics::SensitivityStream {
public:
    SensitivityBufferStream(const std::string& buffer,
                            char delim = ',', char comment = '#',
                            char quoteChar = '\0', char escapeChar = '\\');
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- SensitivityInMemoryStream ---

%shared_ptr(ore::analytics::SensitivityInMemoryStream)

namespace ore { namespace analytics {

class SensitivityInMemoryStream : public ore::analytics::SensitivityStream {
public:
    SensitivityInMemoryStream();
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
    void add(const ore::analytics::SensitivityRecord& sr);
};

}}

// --- FilteredSensitivityStream ---

%shared_ptr(ore::analytics::FilteredSensitivityStream)

namespace ore { namespace analytics {

class FilteredSensitivityStream : public ore::analytics::SensitivityStream {
public:
    FilteredSensitivityStream(const ext::shared_ptr<ore::analytics::SensitivityStream>& ss,
                              QuantLib::Real deltaThreshold, QuantLib::Real gammaThreshold);
    FilteredSensitivityStream(const ext::shared_ptr<ore::analytics::SensitivityStream>& ss,
                              QuantLib::Real threshold);
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- BufferedSensitivityStream ---

%shared_ptr(ore::analytics::BufferedSensitivityStream)

namespace ore { namespace analytics {

class BufferedSensitivityStream : public ore::analytics::SensitivityStream {
public:
    explicit BufferedSensitivityStream(const ext::shared_ptr<ore::analytics::SensitivityStream>& stream);
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- DecomposedSensitivityStream ---

%shared_ptr(ore::analytics::DecomposedSensitivityStream)

namespace ore { namespace analytics {

class DecomposedSensitivityStream : public ore::analytics::SensitivityStream {
public:
    DecomposedSensitivityStream(
        const ext::shared_ptr<ore::analytics::SensitivityStream>& ss, const std::string& baseCurrency,
        const ext::shared_ptr<ore::data::Portfolio>& portfolio,
        const ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager = ext::shared_ptr<ore::data::ReferenceDataManager>(),
        const ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = ext::shared_ptr<ore::data::CurveConfigurations>(),
        const ext::shared_ptr<ore::analytics::SensitivityScenarioData>& scenarioData = ext::shared_ptr<ore::analytics::SensitivityScenarioData>(),
        const ext::shared_ptr<ore::data::Market>& todaysMarket = ext::shared_ptr<ore::data::Market>());
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- SensitivityReportStream ---

%shared_ptr(ore::analytics::SensitivityReportStream)

namespace ore { namespace analytics {

class SensitivityReportStream : public ore::analytics::SensitivityStream {
public:
    SensitivityReportStream(const ext::shared_ptr<ore::data::InMemoryReport>& report);
    ore::analytics::SensitivityRecord next() override;
    void reset() override;
};

}}

// --- Template instantiations for SensitivityCubeStream vector constructor ---

%template(SensitivityCubeVector) std::vector<QuantLib::ext::shared_ptr<ore::analytics::SensitivityCube>>;

// --- SensitivityAggregator ---

%shared_ptr(ore::analytics::SensitivityAggregator)

namespace ore { namespace analytics {

class SensitivityAggregator {
public:
    SensitivityAggregator(const std::map<std::string, std::set<std::pair<std::string, Size>>>& categories);

    void aggregate(ore::analytics::SensitivityStream& ss,
                   const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& filter =
                       QuantLib::ext::make_shared<ore::analytics::ScenarioFilter>());

    void reset();

    const std::set<ore::analytics::SensitivityRecord>& sensitivities(const std::string& category) const;

    typedef std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey> CrossPair;

    void generateDeltaGamma(const std::string& category,
                            std::map<QuantExt::RiskFactorKey, Real>& deltas,
                            std::map<std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>, Real>& gammas);

    %extend {
        SensitivityAggregator(const std::map<std::string, std::vector<std::string>>& categories) {
            std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> cats;
            for (auto const& p : categories) {
                std::set<std::pair<std::string, QuantLib::Size>> s;
                for (auto const& t : p.second) {
                    s.insert(std::make_pair(t, 0));
                }
                cats[p.first] = s;
            }
            return new ore::analytics::SensitivityAggregator(cats);
        }

        SensitivityAggregator(const std::map<std::string, std::vector<std::pair<std::string, QuantLib::Size>>>& categories) {
            std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> cats;
            for (auto const& p : categories) {
                std::set<std::pair<std::string, QuantLib::Size>> s(p.second.begin(), p.second.end());
                cats[p.first] = s;
            }
            return new ore::analytics::SensitivityAggregator(cats);
        }

        std::pair<std::map<QuantExt::RiskFactorKey, Real>,
                  std::map<std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>, Real>>
        getDeltaGamma(const std::string& category) {
            std::map<QuantExt::RiskFactorKey, Real> deltas;
            std::map<std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>, Real> gammas;
            $self->generateDeltaGamma(category, deltas, gammas);
            return std::make_pair(deltas, gammas);
        }
    }
};

}}

// --- Template instantiations for SensitivityAggregator and related types ---

%template(StringVectorMap) std::map<std::string, std::vector<std::string>>;
%template(StringSizePairVector) std::vector<std::pair<std::string, QuantLib::Size>>;
%template(StringStringSizePairVectorMap) std::map<std::string, std::vector<std::pair<std::string, QuantLib::Size>>>;

%template(StringSizePair) std::pair<std::string, QuantLib::Size>;
%template(StringSizePairSet) std::set<std::pair<std::string, QuantLib::Size>>;
%template(StringStringSizePairSetMap) std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>>;

%template(SensitivityRecordSet) std::set<ore::analytics::SensitivityRecord>;

%template(RiskFactorKeyPair) std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>;
%template(RiskFactorKeyRealMap) std::map<QuantExt::RiskFactorKey, Real>;
%template(RiskFactorKeyPairRealMap) std::map<std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>, Real>;

%template(DeltaGammaPair) std::pair<std::map<QuantExt::RiskFactorKey, Real>,
                                   std::map<std::pair<QuantExt::RiskFactorKey, QuantExt::RiskFactorKey>, Real>>;

#endif
