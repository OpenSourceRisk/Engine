/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/scripting/models/model.hpp>
#include <ored/scripting/models/modelcg.hpp>
#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/scripting/ast.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <ored/portfolio/enginefactory.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {

class ScriptedTradeEngineBuilder : public EngineBuilder {
public:
    //! constructor that builds a usual pricing engine
    ScriptedTradeEngineBuilder() : EngineBuilder("Generic", "Generic", {"ScriptedTrade"}) {}

    //! constructor that builds an AMC - enabled pricing engine
    ScriptedTradeEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& amcCam,
                               const std::vector<Date>& amcGrid)
        : EngineBuilder("Generic", "Generic", {"ScriptedTrade"}), buildingAmc_(true), amcCam_(amcCam),
          amcGrid_(amcGrid) {}

    //! constructor that builds an AMCCG pricing engine
    ScriptedTradeEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& amcCgModel,
                               const std::vector<Date>& amcGrid)
        : EngineBuilder("Generic", "Generic", {"ScriptedTrade"}), buildingAmc_(true), amcCgModel_(amcCgModel),
          amcGrid_(amcGrid) {}

    QuantLib::ext::shared_ptr<QuantExt::ScriptedInstrument::engine>
    engine(const std::string& id, const ScriptedTrade& scriptedTrade,
           const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
           const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig());

    // these are guaranteed to be set only after engine() was called
    const std::string& npvCurrency() const { return model_ ? model_->baseCcy() : modelCG_->baseCcy(); }
    const QuantLib::Date& lastRelevantDate() const { return lastRelevantDate_; }
    const std::string& simmProductClass() const { return simmProductClass_; }
    const std::string& scheduleProductClass() const { return scheduleProductClass_; }
    const std::string& sensitivityTemplate() const { return sensitivityTemplate_; }
    const std::map<std::string, std::set<Date>>& fixings() const { return fixings_; }

protected:
    // hook for correlation retrieval - by default the correlation for a pair of indices is queried from the market
    // other implementations might want to estimate the correlation on the fly based on historical data
    virtual QuantLib::Handle<QuantExt::CorrelationTermStructure> correlationCurve(const std::string& index1,
                                                                                  const std::string& index2);

    // sub tasks for engine building
    void clear();
    void extractIndices(const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr);
    void deriveProductClass(const std::vector<ScriptedTradeValueTypeData>& indices);
    void populateModelParameters();
    void populateFixingsMap(const IborFallbackConfig& iborFallbackConfig);
    void extractPayCcys();
    void determineBaseCcy();
    void compileModelCcyList();
    void compileModelIndexLists();
    void setupCorrelations();
    void setLastRelevantDate();
    virtual void setupBlackScholesProcesses(); // hook for custom building of processes
    void setupIrReversions();
    void compileSimulationAndAddDates();
    void buildBlackScholes(const std::string& id, const IborFallbackConfig& iborFallbackConfig);
    void buildFdBlackScholes(const std::string& id, const IborFallbackConfig& iborFallbackConfig);
    void buildLocalVol(const std::string& id, const IborFallbackConfig& iborFallbackConfig);
    void buildGaussianCam(const std::string& id, const IborFallbackConfig& iborFallbackConfig,
                          const std::vector<std::string>& conditionalExpectationModelStates);
    void buildFdGaussianCam(const std::string& id, const IborFallbackConfig& iborFallbackConfig);
    void buildGaussianCamAMC(const std::string& id, const IborFallbackConfig& iborFallbackConfig,
                             const std::vector<std::string>& conditionalExpectationModelStates);
    void buildAMCCGModel(const std::string& id, const IborFallbackConfig& iborFallbackConfig,
                         const std::vector<std::string>& conditionalExpectationModelStates);
    void addAmcGridToContext(QuantLib::ext::shared_ptr<Context>& context) const;
    void setupCalibrationStrikes(const ScriptedTradeScriptData& script, const QuantLib::ext::shared_ptr<Context>& context);

    // gets eq ccy from market
    std::string getEqCcy(const IndexInfo& e);

    // gets comm ccy from market
    std::string getCommCcy(const IndexInfo& e);

    // input data (for amc, amcCam_, amcCgModel_ are mutually exclusive)
    bool buildingAmc_ = false;
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> amcCam_;
    const QuantLib::ext::shared_ptr<ore::data::ModelCG> amcCgModel_;
    const std::vector<Date> amcGrid_;

    // cache for parsed asts
    std::map<std::string, ASTNodePtr> astCache_;

    // populated by a call to engine()
    ASTNodePtr ast_;
    std::string npvCurrency_;
    QuantLib::Date lastRelevantDate_;
    std::string simmProductClass_;
    std::string scheduleProductClass_;
    std::string sensitivityTemplate_;
    std::map<std::string, std::set<Date>> fixings_;

    // temporary variables used during engine building
    QuantLib::ext::shared_ptr<StaticAnalyser> staticAnalyser_;
    std::set<IndexInfo> eqIndices_, commIndices_, irIndices_, infIndices_, fxIndices_;
    std::string resolvedProductTag_, assetClassReplacement_;
    std::set<std::string> payCcys_;
    std::string baseCcy_;
    std::vector<std::string> modelCcys_;
    std::vector<Handle<YieldTermStructure>> modelCurves_;
    std::vector<Handle<Quote>> modelFxSpots_;
    std::vector<std::string> modelIndices_, modelIndicesCurrencies_;
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>> modelIrIndices_;
    std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>> modelInfIndices_;
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations_;
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    std::map<std::string, Real> irReversions_;
    std::set<Date> simulationDates_, addDates_;
    QuantLib::ext::shared_ptr<Model> model_;
    QuantLib::ext::shared_ptr<ModelCG> modelCG_;
    std::map<std::string, std::vector<Real>> calibrationStrikes_;

    // model / engine parameters
    std::string modelParam_, infModelType_, engineParam_, baseCcyParam_, gridCoarsening_;
    bool fullDynamicFx_, fullDynamicIr_, enforceBaseCcy_;
    Size modelSize_, timeStepsPerYear_;
    Model::McParams mcParams_;
    bool interactive_, zeroVolatility_, continueOnCalibrationError_;
    std::vector<Real> calibrationMoneyness_;
    Real mesherEpsilon_, mesherScaling_, mesherConcentration_;
    Size mesherMaxConcentratingPoints_;
    bool mesherIsStatic_;
    std::string referenceCalibrationGrid_;
    Real bootstrapTolerance_;
    bool calibrate_;
    std::string calibration_;
    bool useCg_;
    bool useAd_;
    bool useExternalComputeDevice_;
    bool useDoublePrecisionForExternalCalculation_;
    bool externalDeviceCompatibilityMode_;
    std::string externalComputeDevice_;
    bool includePastCashflows_;
};

} // namespace data
} // namespace ore
