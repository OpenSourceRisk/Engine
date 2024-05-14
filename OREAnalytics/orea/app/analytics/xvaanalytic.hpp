/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytic.hpp
    \brief ORE Analytics Manager
*/

#pragma once

#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {

class XvaAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA";

    explicit XvaAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    void checkConfigurations(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    
protected:
    QuantLib::ext::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError);
    void buildScenarioGenerator(bool continueOnError);

    void initCubeDepth();
    void initCube(QuantLib::ext::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth);    

    void initClassicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    void buildClassicCube(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);
    QuantLib::ext::shared_ptr<Portfolio> classicRun(const QuantLib::ext::shared_ptr<Portfolio>& portfolio);

    QuantLib::ext::shared_ptr<EngineFactory> amcEngineFactory(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                      const std::vector<Date>& grid);
    void buildAmcPortfolio();
    void amcRun(bool doClassicRun);

    void runPostProcessor();

    Matrix creditStateCorrelationMatrix() const;

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarketCalibration_;
    QuantLib::ext::shared_ptr<EngineFactory> engineFactory_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    QuantLib::ext::shared_ptr<Portfolio> amcPortfolio_, classicPortfolio_;
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_, amcCube_;
    QuantLib::RelinkableHandle<AggregationScenarioData> scenarioData_;
    QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpreter_;
    QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    QuantLib::ext::shared_ptr<PostProcess> postProcess_;
    QuantLib::ext::shared_ptr<Scenario> offsetScenario_;

    Size cubeDepth_ = 0;
    QuantLib::ext::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;

    bool runSimulation_ = false;
    bool runXva_ = false;
};

static const std::set<std::string> xvaAnalyticSubAnalytics{"XVA", "EXPOSURE"};

struct XvaResult {
    std::string tradeId;
    std::string nettingSetId;
    double cva=0;
    double dva=0;
    double pfe=0;
};

class XvaAnalytic : public Analytic {
public:
    explicit XvaAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                         const QuantLib::ext::shared_ptr<Scenario> offSetScenario = nullptr)
        : Analytic(std::make_unique<XvaAnalyticImpl>(inputs), xvaAnalyticSubAnalytics, inputs, false, false, false,
                   false), offsetScenario_(offSetScenario) {}

    void setOffsetScenario(const QuantLib::ext::shared_ptr<Scenario>& scenario) { offsetScenario_ = scenario; }
    const QuantLib::ext::shared_ptr<Scenario>& offsetScenario() const { return offsetScenario_; }

    void setNettingSetResults(const std::map<std::string, XvaResult>& results) { nettingSetResults_ = results; }
    void setTradeLevelResults(const std::map<std::string, XvaResult>& results) { tradeLevelResults_ = results; }
    
    const std::map<std::string, XvaResult>& nettingSetResults() const { return nettingSetResults_; }
    const std::map<std::string, XvaResult>& tradeLevelResults() const { return tradeLevelResults_; }

private:
    QuantLib::ext::shared_ptr<Scenario> offsetScenario_;
    std::map<std::string, XvaResult> nettingSetResults_;
    std::map<std::string, XvaResult> tradeLevelResults_;
};

} // namespace analytics
} // namespace oreplus
