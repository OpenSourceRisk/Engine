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

    explicit XvaAnalyticImpl(
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        const QuantLib::ext::shared_ptr<Scenario>& offsetScenario = nullptr,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : Analytic::Impl(inputs), offsetScenario_(offsetScenario), offsetSimMarketParams_(offsetSimMarketParams) {
        QL_REQUIRE(!((offsetScenario_ == nullptr) ^ (offsetSimMarketParams_ == nullptr)),
                   "Need offsetScenario and corresponding simMarketParameter");
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

    QuantLib::ext::shared_ptr<EngineFactory>
    amcEngineFactory(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam, const std::vector<Date>& grid);
    void buildAmcPortfolio();
    void amcRun(bool doClassicRun);

    void runPostProcessor();

    Matrix creditStateCorrelationMatrix() const;

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarketCalibration_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> offsetSimMarket_;
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
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> offsetSimMarketParams_;
    Size cubeDepth_ = 0;
    QuantLib::ext::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;

    bool runSimulation_ = false;
    bool runXva_ = false;
};

static const std::set<std::string> xvaAnalyticSubAnalytics{"XVA", "EXPOSURE"};

class XvaAnalytic : public Analytic {
public:
    explicit XvaAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                         const QuantLib::ext::shared_ptr<Scenario>& offSetScenario = nullptr,
                         const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : Analytic(std::make_unique<XvaAnalyticImpl>(inputs, offSetScenario, offsetSimMarketParams),
                   xvaAnalyticSubAnalytics, inputs, false, false, false, false) {}
};

} // namespace analytics
} // namespace ore
