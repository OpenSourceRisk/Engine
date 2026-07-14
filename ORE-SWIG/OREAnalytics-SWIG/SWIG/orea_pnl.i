/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

#ifndef orea_pnl_i
#define orea_pnl_i

%include orea_engine.i

%{
#include <orea/engine/pnlexplainreport.hpp>
#include <orea/engine/historicalpnlgenerator.hpp>
%}

%shared_ptr(ore::analytics::HistoricalPnlGenerator)

// PnlExplainReport has no default constructor and is not otherwise wrapped;
// this bare redeclaration exists solely to expose the nested PnlExplainResults
// struct as a flat top-level Python type.
%nodefaultctor ore::analytics::PnlExplainReport;
%feature("flatnested") ore::analytics::PnlExplainReport::PnlExplainResults;
%rename(PnlExplainResults) ore::analytics::PnlExplainReport::PnlExplainResults;

namespace ore {
namespace analytics {

class PnlExplainReport {
  public:
    struct PnlExplainResults {
        std::string riskFactor;
        QuantLib::Real pnl;
        QuantLib::Real delta;
        QuantLib::Real gamma;
        QuantLib::Real vega;
        QuantLib::Real irDelta;
        QuantLib::Real irGamma;
        QuantLib::Real irVega;
        QuantLib::Real eqDelta;
        QuantLib::Real eqGamma;
        QuantLib::Real eqVega;
        QuantLib::Real fxDelta;
        QuantLib::Real fxGamma;
        QuantLib::Real fxVega;
        QuantLib::Real infDelta;
        QuantLib::Real infGamma;
        QuantLib::Real infVega;
        QuantLib::Real creditDelta;
        QuantLib::Real creditGamma;
        QuantLib::Real creditVega;
        QuantLib::Real comDelta;
        QuantLib::Real comGamma;
        QuantLib::Real comVega;
    };
};

}
}

namespace ore {
namespace analytics {

class HistoricalPnlGenerator {
  public:
    HistoricalPnlGenerator(const std::string& baseCurrency, 
                           const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                           const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                           const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
                           const QuantLib::ext::shared_ptr<NPVCube>& cube,
                           const set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>& modelBuilders = {},
                           bool dryRun = false);

    void generateCube(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter, const bool runRiskFactorBreakdown = false);
    
    std::vector<QuantLib::Real> pnl() const;
    std::vector<QuantLib::Real> pnl(const ore::data::TimePeriod& period) const;
    
    // TradePnlStore = std::vector<std::vector<QuantLib::Real>>, already wrapped
    // as DoubleVectorVector in vectors.i (QuantLib::Real is a double typedef).
    std::vector<std::vector<QuantLib::Real>> tradeLevelPnl() const;
    std::vector<std::vector<QuantLib::Real>> tradeLevelPnl(const ore::data::TimePeriod& period) const;
    
    const QuantLib::ext::shared_ptr<NPVCube>& cube() const;
};

}
}

#endif
