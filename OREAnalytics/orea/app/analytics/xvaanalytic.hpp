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

    XvaAnalyticImpl(const boost::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) { setLabel(LABEL); }
    virtual void runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    void checkConfigurations(const boost::shared_ptr<Portfolio>& portfolio);
    
protected:
    boost::shared_ptr<ore::data::EngineFactory> engineFactory() override;
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError);
    void buildScenarioGenerator(bool continueOnError);

    void initCubeDepth();
    void initCube(boost::shared_ptr<NPVCube>& cube, const std::set<std::string>& ids, Size cubeDepth);    

    void initClassicRun(const boost::shared_ptr<Portfolio>& portfolio);
    void buildClassicCube(const boost::shared_ptr<Portfolio>& portfolio);
    boost::shared_ptr<Portfolio> classicRun(const boost::shared_ptr<Portfolio>& portfolio);

    boost::shared_ptr<EngineFactory> amcEngineFactory(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                                      const std::vector<Date>& grid);
    void buildAmcPortfolio();
    void amcRun(bool doClassicRun);

    void runPostProcessor();

    Matrix creditStateCorrelationMatrix() const;

    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<EngineFactory> engineFactory_;
    boost::shared_ptr<CrossAssetModel> model_;
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    boost::shared_ptr<Portfolio> amcPortfolio_, classicPortfolio_;
    boost::shared_ptr<NPVCube> cube_, nettingSetCube_, cptyCube_, amcCube_;
    QuantLib::RelinkableHandle<AggregationScenarioData> scenarioData_;
    boost::shared_ptr<CubeInterpretation> cubeInterpreter_;
    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    boost::shared_ptr<PostProcess> postProcess_;
    
    Size cubeDepth_ = 0;
    boost::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;

    bool runSimulation_ = false;
    bool runXva_ = false;
};

class XvaAnalytic : public Analytic {
public:
    XvaAnalytic(const boost::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<XvaAnalyticImpl>(inputs), {"XVA", "EXPOSURE"}, inputs, false, false, false, false) {}
};

} // namespace analytics
} // namespace oreplus
