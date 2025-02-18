/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/xvasensitivityanalytic.hpp
    \brief xva sensitivity analytic
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/engine/zerotoparcube.hpp>
namespace ore {
namespace analytics {

class XvaResults {
public:
    enum class Adjustment { CVA, DVA, FBA, FCA };
    XvaResults() {}

    XvaResults(const QuantLib::ext::shared_ptr<InMemoryReport>& xvaReport);

    const std::map<std::string, double>& getTradeXVAs(Adjustment adjustment) {
        return tradeValueAdjustments_[adjustment];
    }

     const std::map<std::string, double>& getNettingSetXVAs(Adjustment adjustment) {
        return nettingSetValueAdjustments_[adjustment];
    }

    const std::set<std::string>& nettingSetIds() const { return nettingSetIds_; };
    const std::set<std::string>& tradeIds() const { return tradeIds_; }

    const std::map<std::string, std::string>& tradeNettingSetMapping() const { return tradeNettingSetMapping_; }

private:
    std::map<Adjustment, std::map<std::string, double>> tradeValueAdjustments_;
    std::map<Adjustment, std::map<std::string, double>> nettingSetValueAdjustments_;
    std::set<std::string> nettingSetIds_;
    std::set<std::string> tradeIds_;
    std::map<std::string, std::string> tradeNettingSetMapping_;
};

//! Write FrtbCorrelationScenario to stream
std::ostream& operator<<(std::ostream& os, const XvaResults::Adjustment adjustment);

struct ZeroSensiResults {
    std::map<XvaResults::Adjustment, QuantLib::ext::shared_ptr<SensitivityCube>> tradeCubes_;
    std::map<XvaResults::Adjustment, QuantLib::ext::shared_ptr<SensitivityCube>> nettingCubes_;
    std::map<std::string, std::string> tradeNettingSetMap_;
};
struct ParSensiResults {
    std::map<XvaResults::Adjustment, QuantLib::ext::shared_ptr<ZeroToParCube>> tradeParSensiCube_;
    std::map<XvaResults::Adjustment, QuantLib::ext::shared_ptr<ZeroToParCube>> nettingParSensiCube_;
};

class XvaSensitivityAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA_SENSITIVITY";
    explicit XvaSensitivityAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs);
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    void setParCvaSensiCubeStream(const QuantLib::ext::shared_ptr<ParSensitivityCubeStream>& parCvaSensiCubeStream) {
        parCvaSensiCubeStream_ = parCvaSensiCubeStream;
    }
    const QuantLib::ext::shared_ptr<ParSensitivityCubeStream>& parCvaSensiCubeStream() const {
        return parCvaSensiCubeStream_;
    }

private:
    QuantLib::ext::shared_ptr<ScenarioSimMarket> buildSimMarket(bool overrideTenors);
    
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> buildScenarioGenerator(QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket, bool overrideTenors);
    
    //! Build a simMarket, scenarioGenerator, loop through all scenarios and compute xva under each scenario and collect the valueadjustments on trade and nettingset
    // level and build sensicubes for each value adjustment.
    ZeroSensiResults computeZeroXvaSensitivity(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
    
    void computeXvaUnderScenarios(std::map<size_t, QuantLib::ext::shared_ptr<XvaResults>>& xvaResults, const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator);

    ZeroSensiResults
    convertXvaResultsToSensiCubes(const std::map<size_t, QuantLib::ext::shared_ptr<XvaResults>>& xvaResults,
                                  const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator);

    //! Convert the sensitivity cubes into sensistreams and create a report 
    void createZeroReports(ZeroSensiResults& xvaZeroSeniCubes);
    
    //! 
    ParSensiResults parConversion(ZeroSensiResults& zeroResults);
    void createParReports(ParSensiResults& xvaParSensiCubes, const std::map<std::string, std::string>& tadeNettingSetMap);

    //! Create a report containing all value adjustment values for each scenario
    void createDetailReport(
    const QuantLib::ext::shared_ptr<SensitivityScenarioGenerator>& scenarioGenerator,
    const std::map<std::string, std::vector<ext::shared_ptr<InMemoryReport>>>& xvaReports);

    QuantLib::ext::shared_ptr<ParSensitivityCubeStream> parCvaSensiCubeStream_;
};

class XvaSensitivityAnalytic : public Analytic {
public:
    explicit XvaSensitivityAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                    const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager = nullptr)
        : Analytic(std::make_unique<XvaSensitivityAnalyticImpl>(inputs), {"XVA_SENSITIVITY"}, inputs, analyticsManager,
                   true, true, false, false) {
    }
};

} // namespace analytics
} // namespace ore

