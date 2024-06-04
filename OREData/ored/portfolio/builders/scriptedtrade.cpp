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

#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/blackscholescg.hpp>
#include <ored/scripting/models/fdblackscholesbase.hpp>
#include <ored/scripting/models/fdgaussiancam.hpp>
#include <ored/scripting/models/gaussiancam.hpp>
#include <ored/scripting/models/gaussiancamcg.hpp>
#include <ored/scripting/models/localvol.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingengine.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/context.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

#include <ored/configuration/correlationcurveconfig.hpp>
#include <ored/marketdata/strike.hpp>
#include <ored/model/blackscholesmodelbuilder.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/model/localvolmodelbuilder.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/math/computeenvironment.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

QuantLib::Handle<QuantExt::CorrelationTermStructure>
ScriptedTradeEngineBuilder::correlationCurve(const std::string& index1, const std::string& index2) {
    if (index1 == index2) {
        // need to handle this case here, we might have calls with index1 == index arising from COMM
        // indices with different spot / future reference, for which we expect the correlation on
        // the name level (i.e. for the spot index)
        return Handle<QuantExt::CorrelationTermStructure>(
            QuantLib::ext::make_shared<FlatCorrelation>(0, NullCalendar(), 1.0, ActualActual(ActualActual::ISDA)));
    } else {
        return market_->correlationCurve(index1, index2, configuration(MarketContext::pricing));
    }
}

QuantLib::ext::shared_ptr<ScriptedInstrument::engine>
ScriptedTradeEngineBuilder::engine(const std::string& id, const ScriptedTrade& scriptedTrade,
                                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                   const IborFallbackConfig& iborFallbackConfig) {

    const std::vector<ScriptedTradeEventData>& events = scriptedTrade.events();
    const std::vector<ScriptedTradeValueTypeData>& numbers = scriptedTrade.numbers();
    const std::vector<ScriptedTradeValueTypeData>& indices = scriptedTrade.indices();
    const std::vector<ScriptedTradeValueTypeData>& currencies = scriptedTrade.currencies();
    const std::vector<ScriptedTradeValueTypeData>& daycounters = scriptedTrade.daycounters();

    LOG("Building engine for scripted trade " << id);

    // 0 clear members

    clear();

    // 1 set the SIMM product class, simple EQ > COM > FX approach for Hybrids, that's all we can do automatically?
    //   also set the assetClassReplacement string which is used to replace {AssetClass} in product tags

    deriveProductClass(indices);

    // 1b get product tag from scripted trade or library and build resolved product tag

    std::string productTag = getScript(scriptedTrade, ScriptLibraryStorage::instance().get(), "", false).first;
    resolvedProductTag_ = boost::replace_all_copy(productTag, "{AssetClass}", assetClassReplacement_);
    DLOG("got product tag '" << productTag << "', resolved product tag is '" << resolvedProductTag_);

    // 2 populate model and engine parameters

    populateModelParameters();

    // 3 define purpose, get suitable script and build ast (i.e. parse it or retrieve it from cache)

    std::string purpose = "";
    if (buildingAmc_)
        purpose = "AMC";
    else if (engineParam_ == "FD")
        purpose = "FD";

    ScriptedTradeScriptData script =
        getScript(scriptedTrade, ScriptLibraryStorage::instance().get(), purpose, true).second;

    auto f = astCache_.find(script.code());

    if (f != astCache_.end()) {
        ast_ = f->second;
        DLOG("retrieved ast from cache");
    } else {
        ast_ = parseScript(script.code());
        astCache_[script.code()] = ast_;
        DLOGGERSTREAM("built ast:\n" << to_string(ast_));
    }

    // 4 set up context

    auto context = makeContext(modelSize_, gridCoarsening_, script.schedulesEligibleForCoarsening(), referenceData,
                               events, numbers, indices, currencies, daycounters);
    addAmcGridToContext(context);
    addNewSchedulesToContext(context, script.newSchedules());

    DLOG("Built initial context:");
    DLOGGERSTREAM(*context);

    // 4b set up calibration strike information

    if (!buildingAmc_)
        setupCalibrationStrikes(script, context);

    // 5 run static analyser

    DLOG("Run static analyser on script");
    staticAnalyser_ = QuantLib::ext::make_shared<StaticAnalyser>(ast_, context);
    staticAnalyser_->run(script.code());

    // 6 extract eq, fx, ir indices from script

    extractIndices(referenceData);

    // 7 populate fixings map

    populateFixingsMap(iborFallbackConfig);

    // 8 init result variable for NPV

    checkDuplicateName(context, script.npv());
    context->scalars[script.npv()] = RandomVariable(modelSize_, 0.0);

    // 9 extract pay currencies

    extractPayCcys();

    // 10 determine base ccy, this might be overwritten in 11 when an AMC engine is built, see the implementation of 11

    determineBaseCcy();

    // 11 compile the model currency list (depends on the model actually)

    compileModelCcyList();

    // 12 get the t0 curves for each model ccy

    std::string externalDiscountCurve = scriptedTrade.envelope().additionalField("discount_curve", false);
    std::string externalSecuritySpread = scriptedTrade.envelope().additionalField("security_spread", false);
    for (auto const& c : modelCcys_) {
        // for base ccy we account for an external discount curve and security spread if given
        Handle<YieldTermStructure> yts =
            externalDiscountCurve.empty() || c != baseCcy_
                ? market_->discountCurve(c, configuration(MarketContext::pricing))
                : indexOrYieldCurve(market_, externalDiscountCurve, configuration(MarketContext::pricing));
        if (!externalSecuritySpread.empty() && c == baseCcy_)
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market_->securitySpread(externalSecuritySpread, configuration(MarketContext::pricing))));
        modelCurves_.push_back(yts);
        DLOG("curve for " << c << " added.");
    }

    // 13 get the fx spots for each model ccy vs. the base ccy

    for (Size i = 1; i < modelCcys_.size(); ++i) {
        modelFxSpots_.push_back(market_->fxRate(modelCcys_[i] + baseCcy_, configuration(MarketContext::pricing)));
        DLOG("fx spot " << modelCcys_[i] + baseCcy_ << " added.");
    }

    // 14 compile the model index (eq, fx, comm) and ir index lists

    compileModelIndexLists();

    // 15 determine last relevant date as max over index eval, regression, pay obs/pay dates

    setLastRelevantDate();

    // 16 set up correlations between model indices (ir, eq, fx, comm)

    setupCorrelations();

    // 17 compile the processes (needed for BlackScholes, LocalVol only)

    if (modelParam_ == "BlackScholes" || modelParam_ == "LocalVolDupire" || modelParam_ == "LocalVolAndreasenHuge")
        setupBlackScholesProcesses();

    // 18 setup IR reversion values (needed for Gaussian CAM only)

    if (modelParam_ == "GaussianCam") {
        setupIrReversions();
    }

    // 19 compile the sim and add dates required by the model ctors below

    compileSimulationAndAddDates();

    // 20 build the model adapter

    QL_REQUIRE(!buildingAmc_ || modelParam_ == "GaussianCam",
               "model/engine = GaussianCam/MC required to build an amc model, got " << modelParam_ << "/"
                                                                                    << engineParam_);

    if(staticAnalyser_->regressionDates().empty())
        mcParams_.trainingSamples = Null<Size>();

    if (modelParam_ == "BlackScholes" && engineParam_ == "MC") {
        buildBlackScholes(id, iborFallbackConfig);
    } else if (modelParam_ == "BlackScholes" && engineParam_ == "FD") {
        buildFdBlackScholes(id, iborFallbackConfig);
    } else if ((modelParam_ == "LocalVolDupire" || modelParam_ == "LocalVolAndreasenHuge") && engineParam_ == "MC") {
        buildLocalVol(id, iborFallbackConfig);
    } else if (modelParam_ == "GaussianCam" && engineParam_ == "MC") {
        if (amcCam_) {
            buildGaussianCamAMC(id, iborFallbackConfig, script.conditionalExpectationModelStates());
        } else if (amcCgModel_) {
            buildAMCCGModel(id, iborFallbackConfig, script.conditionalExpectationModelStates());
        } else {
            buildGaussianCam(id, iborFallbackConfig, script.conditionalExpectationModelStates());
        }
    } else if (modelParam_ == "GaussianCam" && engineParam_ == "FD") {
        buildFdGaussianCam(id, iborFallbackConfig);
    } else {
        QL_FAIL("model '" << modelParam_ << "' / engine '" << engineParam_
                          << "' not recognised, expected BlackScholes/[MC|FD], LocalVolDupire/MC, "
                             "LocalVolAndreasenHuge/MC, GaussianCam/MC");
    }

    QL_REQUIRE(model_ != nullptr || modelCG_ != nullptr, "internal error: both model_ and modelCG_ are null");

    // 21 log some summary information

    DLOG("built model          : " << modelParam_ << " / " << engineParam_);
    DLOG("useCg                = " << std::boolalpha << useCg_);
    DLOG("useAd                = " << std::boolalpha << useAd_);
    DLOG("useExternalDevice    = " << std::boolalpha << useExternalComputeDevice_);
    DLOG("useDblPrecExtCalc    = " << std::boolalpha << useDoublePrecisionForExternalCalculation_);
    DLOG("extDeviceCompatMode  = " << std::boolalpha << externalDeviceCompatibilityMode_);
    DLOG("externalDevice       = " << (useExternalComputeDevice_ ? externalComputeDevice_ : "na"));
    DLOG("calibration          = " << calibration_);
    DLOG("base ccy             = " << baseCcy_);
    DLOG("model base (npv) ccy = " << (model_ != nullptr ? model_->baseCcy() : modelCG_->baseCcy()));
    DLOG("ccys                 = " << modelCcys_.size());
    DLOG("eq,fx,com indices    = " << modelIndices_.size());
    DLOG("ir indices           = " << irIndices_.size());
    DLOG("inf indices          = " << infIndices_.size());
    DLOG("sim dates            = " << simulationDates_.size());
    DLOG("add dates            = " << addDates_.size());
    DLOG("timeStepsPerYear     = " << timeStepsPerYear_);
    DLOG("fullDynamicFx        = " << std::boolalpha << fullDynamicFx_);
    if (engineParam_ == "MC") {
        DLOG("seed                 = " << mcParams_.seed);
        DLOG("paths                = " << modelSize_);
        DLOG("regressionOrder      = " << mcParams_.regressionOrder);
        DLOG("sequence type        = " << mcParams_.sequenceType);
        DLOG("polynom type         = " << mcParams_.polynomType);
        if (mcParams_.trainingSamples != Null<Size>()) {
            DLOG("training seed        = " << mcParams_.trainingSeed);
            DLOG("training paths       = " << mcParams_.trainingSamples);
            DLOG("training seq. type   = " << mcParams_.trainingSequenceType);
        }
        DLOG("sobol bb ordering    = " << mcParams_.sobolOrdering);
        DLOG("sobol direction int. = " << mcParams_.sobolDirectionIntegers);
    } else if (engineParam_ == "FD") {
        DLOG("stateGridPoints      = " << modelSize_);
        DLOG("mesherEpsilon        = " << mesherEpsilon_);
        DLOG("mesherScaling        = " << mesherScaling_);
        DLOG("mesherConcentration  = " << mesherConcentration_);
        DLOG("mesherMaxConcentrPts = " << mesherMaxConcentratingPoints_);
        DLOG("mesherIsStatic       = " << std::boolalpha << mesherIsStatic_);
    }
    if (modelParam_ == "GaussianCam") {
        DLOG("fullDynamicIr        = " << std::boolalpha << fullDynamicIr_);
        DLOG("ref calibration grid = " << referenceCalibrationGrid_);
        DLOG("bootstrap tolerance  = " << bootstrapTolerance_);
        DLOG("infModelType         = " << infModelType_);
        DLOG("condExpMdlStates     = " << boost::algorithm::join(script.conditionalExpectationModelStates(), ","));
    } else if (modelParam_ == "LocalVolAndreasenHuge") {
        DLOG("moneyness points = " << calibrationMoneyness_.size());
    }

    // 22 build the pricing engine and return it

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    QuantLib::ext::shared_ptr<ScriptedInstrument::engine> engine;
    if (model_) {
        engine = QuantLib::ext::make_shared<ScriptedInstrumentPricingEngine>(
            script.npv(), script.results(), model_, ast_, context, script.code(), interactive_, amcCam_ != nullptr,
            std::set<std::string>(script.stickyCloseOutStates().begin(), script.stickyCloseOutStates().end()),
            generateAdditionalResults, includePastCashflows_);
    } else if (modelCG_) {
        auto rt = globalParameters_.find("RunType");
        std::string runType = rt != globalParameters_.end() ? rt->second : "<<no run type set>>";
        bool useCachedSensis = useAd_ && (runType == "SensitivityDelta");
        bool useExternalDev = useExternalComputeDevice_ && !generateAdditionalResults && !useCachedSensis;
        if (useAd_ && !useCachedSensis) {
            WLOG("Will not apply AD although useAD is configured, because runType ("
                 << runType << ") does not match SensitivitiyDelta");
        }
        if (useExternalComputeDevice_ && !useExternalDev) {
            WLOG("Will not use exxternal compute deivce although useExternalComputeDevice is configured, because we "
                 "are either applying AD ("
                 << std::boolalpha << useCachedSensis << ") or we are generating add results ("
                 << generateAdditionalResults << "), both of which do not support external devices at the moment.");
        }
        engine = QuantLib::ext::make_shared<ScriptedInstrumentPricingEngineCG>(
            script.npv(), script.results(), modelCG_, ast_, context, mcParams_, script.code(), interactive_,
            generateAdditionalResults, includePastCashflows_, useCachedSensis, useExternalDev,
            useDoublePrecisionForExternalCalculation_);
        if (useExternalDev) {
            ComputeEnvironment::instance().selectContext(externalComputeDevice_);
        }
    }

    LOG("engine built for model " << modelParam_ << " / " << engineParam_ << ", modelSize = " << modelSize_
                                  << ", interactive = " << interactive_ << ", amcEnabled = " << buildingAmc_
                                  << ", generateAdditionalResults = " << generateAdditionalResults);
    return engine;
}

void ScriptedTradeEngineBuilder::clear() {
    fixings_.clear();
    eqIndices_.clear();
    commIndices_.clear();
    irIndices_.clear();
    infIndices_.clear();
    fxIndices_.clear();
    payCcys_.clear();
    modelCcys_.clear();
    modelCurves_.clear();
    modelFxSpots_.clear();
    modelIndices_.clear();
    modelIndicesCurrencies_.clear();
    modelIrIndices_.clear();
    modelInfIndices_.clear();
    correlations_.clear();
    processes_.clear();
    irReversions_.clear();
    simulationDates_.clear();
    addDates_.clear();
    calibrationStrikes_.clear();
    model_ = nullptr;
    modelCG_ = nullptr;
}

void ScriptedTradeEngineBuilder::extractIndices(
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData) {
    DLOG("Extract indices from script:");
    for (auto const& i : staticAnalyser_->indexEvalDates()) {
        IndexInfo ind(i.first);
        if (ind.isEq()) {
            eqIndices_.insert(ind);
        } else if (ind.isIr()) {
            irIndices_.insert(ind);
        } else if (ind.isInf()) {
            infIndices_.insert(ind);
        } else if (ind.isFx()) {
            // ignore trivial fx indices
            if (ind.fx()->sourceCurrency() != ind.fx()->targetCurrency())
                fxIndices_.insert(ind);
        } else if (ind.isComm()) {
            commIndices_.insert(ind);
        } else if (ind.isGeneric()) {
            // ignore generic indices, only historical fixings can be retrieved from them
        } else {
            QL_FAIL("unexpected index type for '" << ind.name() << "'");
        }
        DLOG("got " << ind);
    }
    for (auto const& i : staticAnalyser_->fwdCompAvgFixingDates()) {
        IndexInfo ind(i.first);
        QL_REQUIRE(ind.isIr(), "expected IR (ON) index for " << ind.name());
        irIndices_.insert(ind);
        DLOG("got " << ind);
    }
}

void ScriptedTradeEngineBuilder::deriveProductClass(const std::vector<ScriptedTradeValueTypeData>& indices) {
    std::set<std::string> names;
    std::set<IndexInfo> commIndices, eqIndices, fxIndices, irIndices, infIndices;

    for (auto const& i : indices) {
        if (i.isArray()) {
            names.insert(i.values().begin(), i.values().end());
        } else {
            names.insert(i.value());
        }
    }

    for (auto const& n : names) {
        IndexInfo ind(n);
        if (ind.isFx())
            fxIndices.insert(ind);
        else if (ind.isEq())
            eqIndices.insert(ind);
        else if (ind.isComm())
            commIndices.insert(ind);
        else if (ind.isIr())
            irIndices.insert(ind);
        else if (ind.isInf())
            infIndices.insert(ind);
    }

    assetClassReplacement_ = "";
    if (!commIndices.empty()) {
        simmProductClass_ = "Commodity";
        scheduleProductClass_ = "Commodity";
        assetClassReplacement_ = "COMM";
    } else if (!eqIndices.empty()) {
        simmProductClass_ = "Equity";
        scheduleProductClass_ = "Equity";
        assetClassReplacement_ = "EQ";
    } else if (!fxIndices.empty()) {
        simmProductClass_ = "RatesFX";
        scheduleProductClass_ = "FX";
        assetClassReplacement_ = "FX";
        for (auto const& i : fxIndices) {
            std::string f = i.fx()->sourceCurrency().code();
            std::string d = i.fx()->targetCurrency().code();
            if (isPseudoCurrency(f) || isPseudoCurrency(d)) {
                simmProductClass_ = "Commodity";
                scheduleProductClass_ = "Commodity";
                // in terms of the asset class replacement we stick with FX for precious metals
                // assetClassReplacement_ = "COMM";
            }
        }
    } else if (!irIndices.empty() || !infIndices.empty()) {
        simmProductClass_ = "RatesFX";
        scheduleProductClass_ = "Rates";
    } else {
        simmProductClass_ = "RatesFx";   // fallback if we do not have any indices (an edge case really...)
        scheduleProductClass_ = "Rates"; // fallback if we do not have any indices (an edge case really...)
    }

    if ((int)!eqIndices.empty() + (int)!fxIndices.empty() + (int)!commIndices.empty() > 1) {
        WLOG("SIMM product class for hybrid trade is set to " << simmProductClass_);
        WLOG("IM Schedule product class for hybrid trade is set to " << scheduleProductClass_);
        assetClassReplacement_ = "HYBRID";
    } else {
        LOG("SIMM product class is set to " << simmProductClass_);
        LOG("IM Schedule product class is set to " << scheduleProductClass_);
    }
}

void ScriptedTradeEngineBuilder::populateModelParameters() {
    DLOG("Retrieve model and engine parameters using product tag '" << resolvedProductTag_ << "'");

    // mandatory parameters

    modelParam_ = modelParameter("Model", {resolvedProductTag_});
    baseCcyParam_ = modelParameter("BaseCcy", {resolvedProductTag_});
    fullDynamicFx_ = parseBool(modelParameter("FullDynamicFx", {resolvedProductTag_}));
    enforceBaseCcy_ = parseBool(modelParameter("EnforceBaseCcy", {resolvedProductTag_}));
    gridCoarsening_ = modelParameter("GridCoarsening", {resolvedProductTag_});

    engineParam_ = engineParameter("Engine", {resolvedProductTag_});
    timeStepsPerYear_ = parseInteger(engineParameter("TimeStepsPerYear", {resolvedProductTag_}));
    interactive_ = parseBool(engineParameter("Interactive", {resolvedProductTag_}));

    // optional parameters

    zeroVolatility_ = parseBool(engineParameter("ZeroVolatility", {resolvedProductTag_}, false, "false"));
    calibration_ = modelParameter("Calibration", {resolvedProductTag_}, false, "Deal");
    useCg_ = parseBool(engineParameter("UseCG", {resolvedProductTag_}, false, "false"));
    useAd_ = parseBool(engineParameter("UseAD", {resolvedProductTag_}, false, "false"));
    useExternalComputeDevice_ =
        parseBool(engineParameter("UseExternalComputeDevice", {resolvedProductTag_}, false, "false"));
    useDoublePrecisionForExternalCalculation_ =
        parseBool(engineParameter("UseDoublePrecisionForExternalCalculation", {resolvedProductTag_}, false, "false"));
    externalComputeDevice_ = engineParameter("ExternalComputeDevice", {}, false, "");
    externalDeviceCompatibilityMode_ = parseBool(engineParameter("ExternalDeviceCompatibilityMode", {}, false, "false"));
    includePastCashflows_ = parseBool(engineParameter("IncludePastCashflows", {resolvedProductTag_}, false, "false"));

    // usage of ad or an external device implies usage of cg
    if (useAd_ || useExternalComputeDevice_)
        useCg_ = true;

    // default values for parameters that are only read for specific models

    fullDynamicIr_ = false;
    referenceCalibrationGrid_ = "";
    bootstrapTolerance_ = 0.0;
    infModelType_ = "DK";
    mesherEpsilon_ = 1.0E-4;
    mesherScaling_ = 1.5;
    mesherConcentration_ = 0.1;
    mesherMaxConcentratingPoints_ = 9999;
    mesherIsStatic_ = false;

    // parameters only needed for certain model / engine pairs

    DLOG("Retrieve model / engine specific parameters for " << modelParam_ << " / " << engineParam_);

    if (modelParam_ == "GaussianCam") {
        fullDynamicIr_ = parseBool(modelParameter("FullDynamicIr", {resolvedProductTag_}));
        referenceCalibrationGrid_ = modelParameter("ReferenceCalibrationGrid", {resolvedProductTag_}, false, "");
        bootstrapTolerance_ = parseReal(engineParameter("BootstrapTolerance", {resolvedProductTag_}));
        infModelType_ = modelParameter("InfModelType", {resolvedProductTag_}, false, "DK");
    } else if (modelParam_ == "LocalVolAndreasenHuge") {
        calibrationMoneyness_ =
            parseListOfValues<Real>(engineParameter("CalibrationMoneyness", {resolvedProductTag_}), &parseReal);
    }

    if (engineParam_ == "MC") {
        mcParams_.seed = parseInteger(engineParameter("Seed", {resolvedProductTag_}, false, "42"));
        modelSize_ = parseInteger(engineParameter("Samples", {resolvedProductTag_}));
        mcParams_.regressionOrder = parseInteger(engineParameter("RegressionOrder", {resolvedProductTag_}));
        mcParams_.sequenceType =
            parseSequenceType(engineParameter("SequenceType", {resolvedProductTag_}, false, "SobolBrownianBridge"));
        mcParams_.polynomType =
            parsePolynomType(engineParameter("PolynomType", {resolvedProductTag_}, false, "Monomial"));
        mcParams_.trainingSequenceType =
            parseSequenceType(engineParameter("TrainingSequenceType", {resolvedProductTag_}, false, "MersenneTwister"));
        mcParams_.sobolOrdering = parseSobolBrownianGeneratorOrdering(
            engineParameter("SobolOrdering", {resolvedProductTag_}, false, "Steps"));
        mcParams_.sobolDirectionIntegers = parseSobolRsgDirectionIntegers(
            engineParameter("SobolDirectionIntegers", {resolvedProductTag_}, false, "JoeKuoD7"));
        if (auto tmp = engineParameter("TrainingSamples", {resolvedProductTag_}, false, ""); !tmp.empty()) {
            mcParams_.trainingSamples = parseInteger(tmp);
            mcParams_.trainingSeed = parseInteger(engineParameter("TrainingSeed", {resolvedProductTag_}, false, "43"));
        } else {
            mcParams_.trainingSamples = Null<Size>();
        }
        mcParams_.regressionVarianceCutoff =
            parseRealOrNull(engineParameter("RegressionVarianceCutoff", {resolvedProductTag_}, false, std::string()));
        mcParams_.externalDeviceCompatibilityMode = externalDeviceCompatibilityMode_;
    } else if (engineParam_ == "FD") {
        modelSize_ = parseInteger(engineParameter("StateGridPoints", {resolvedProductTag_}));
        mesherEpsilon_ = parseReal(engineParameter("MesherEpsilon", {resolvedProductTag_}, false, "1.0E-4"));
        mesherScaling_ = parseReal(engineParameter("MesherScaling", {resolvedProductTag_}, false, "1.5"));
        mesherConcentration_ = parseReal(engineParameter("MesherConcentration", {resolvedProductTag_}, false, "0.1"));
        mesherMaxConcentratingPoints_ =
            parseInteger(engineParameter("MesherMaxConcentratingPoints", {resolvedProductTag_}, false, "9999"));
        mesherIsStatic_ = parseBool(engineParameter("MesherIsStatic", {resolvedProductTag_}, false, "false"));
    }

    // global parameters that are relevant

    calibrate_ = globalParameters_.count("Calibrate") == 0 || parseBool(globalParameters_.at("Calibrate"));

    if (!calibrate_) {
        DLOG("model calibration is disalbed in global pricing engine parameters");
    }

    continueOnCalibrationError_ = globalParameters_.count("ContinueOnCalibrationError") > 0 &&
                                  parseBool(globalParameters_.at("ContinueOnCalibrationError"));

    // sensitivity template

    sensitivityTemplate_ = engineParameter("SensitivityTemplate", {resolvedProductTag_}, false, std::string());
}

void ScriptedTradeEngineBuilder::populateFixingsMap(const IborFallbackConfig& iborFallbackConfig) {
    DLOG("Populate fixing map");

    // this might be a superset of the actually required fixings, since index evaluations with fwd date are also
    // returned, in which case only future estimations are allowed

    std::map<std::string, std::set<std::pair<Date, bool>>> indexFixings;

    for (auto const& [name, fixings] : staticAnalyser_->probFixingDates()) {
        for (auto const& d : fixings) {
            indexFixings[name].insert(std::make_pair(d, true));
        }
    }

    for (auto const& [name, fixings] : staticAnalyser_->indexEvalDates()) {
        for (auto const& d : fixings) {
            indexFixings[name].insert(std::make_pair(d, false));
        }
    }

    for (auto const& [name, fixings] : indexFixings) {
        IndexInfo i(name);
        if (i.isComm()) {
            // COMM indices require a special treatment, since they might need resolution
            std::map<std::string, Size> stats;
            for (auto const& [d, _] : fixings) {
                auto idx = i.comm(d);
                std::string name = idx->name();
                fixings_[name].insert(idx->fixingCalendar().adjust(d, Preceding));
                stats[name]++;
            }
            for (auto const& s : stats) {
                DLOG("added " << s.second << " fixings for '" << s.first << "' (from eval op, prob fcts)");
            }
        } else {
            // all other indices can be handled generically, notice for inf we include the scripting specific
            // suffixes #L, #F in the index name, this is handled in the scripted trade builder when populating the
            // required fixings

            if (i.irIborFallback(iborFallbackConfig)) {
                // well, except ibor fallback indices that we handle here...
                Size nIbor = 0, nRfr = 0;
                for (auto [d, _] : fixings) {
                    d = i.index()->fixingCalendar().adjust(d, Preceding);
                    if (d >= i.irIborFallback(iborFallbackConfig)->switchDate()) {
                        auto fd = i.irIborFallback(iborFallbackConfig)->onCoupon(d)->fixingDates();
                        fixings_[iborFallbackConfig.fallbackData(name).rfrIndex].insert(fd.begin(), fd.end());
                        nRfr += fd.size();
                    } else {
                        fixings_[i.name()].insert(d);
                        nIbor++;
                    }
                }
                DLOG("added " << nIbor << " Ibor and " << nRfr << " Rfr fixings for ibor fallback '" << i.name()
                              << "' (from eval op, prob fcts)");
            } else if (i.irOvernightFallback(iborFallbackConfig)) {
                Size nOis = 0, nRfr = 0;
                for (auto [d, _] : fixings) {
                    d = i.index()->fixingCalendar().adjust(d, Preceding);
                    if (d >= i.irOvernightFallback(iborFallbackConfig)->switchDate()) {
                        fixings_[iborFallbackConfig.fallbackData(name).rfrIndex].insert(d);
                        nRfr++;
                    } else {
                        fixings_[i.name()].insert(d);
                        nOis++;
                    }
                }
                DLOG("added " << nOis << " OIS and " << nRfr << " Rfr fallback fixings for OIS fallback '" << i.name()
                              << "' (from eval op, prob fcts)");
            } else {
                // ... and all the others here:
                IndexInfo imkt(name, market_);
                for (const auto& [d, prob] : fixings) {
                    fixings_[i.name()].insert((prob ? imkt : i).index()->fixingCalendar().adjust(d, Preceding));
                }
                DLOG("added " << fixings.size() << " fixings for '" << i.name() << "' (from eval op, prob fcts)");
            }
        }
    }

    // add fixings from FWDCOMP(), FWDAVG()
    for (auto const& f : staticAnalyser_->fwdCompAvgFixingDates()) {
        QL_REQUIRE(IndexInfo(f.first).isIr(),
                   "FWD[COMP|AVG]() only supports IR ON indices, got '" << f.first << "' during fixing map population");
        fixings_[f.first].insert(f.second.begin(), f.second.end());
        DLOG("added " << f.second.size() << " fixings for '" << f.first << "' (from FWD[COMP|AVG]())");
    }
}

void ScriptedTradeEngineBuilder::extractPayCcys() {
    DLOG("Extract pay ccys and determine the model's base ccy");
    for (auto const& c : staticAnalyser_->payObsDates()) {
        payCcys_.insert(c.first);
        DLOG("got pay currency " << c.first);
    }
}

void ScriptedTradeEngineBuilder::determineBaseCcy() {
    std::set<std::string> baseCcyCandidates;

    // candidates are target currencies from the fx indices
    for (auto const& i : fxIndices_) {
        std::string ccy = i.fx()->targetCurrency().code();
        baseCcyCandidates.insert(ccy);
        DLOG("add base ccy candidate " << ccy << " from " << i);
    }

    // add pay currencies as base ccy canditate only if there are no candidates from the fx indices
    if (baseCcyCandidates.empty()) {
        for (auto const& p : payCcys_) {
            baseCcyCandidates.insert(p);
            DLOG("add base ccy candidate " << p << " from pay ccys");
        }
    }

    // if there is only one candidate and we do not enforce the base ccy from the model parameters we take that,
    // otherweise the base ccy from the model parameters

    if (baseCcyCandidates.size() == 1 && !enforceBaseCcy_) {
        baseCcy_ = *baseCcyCandidates.begin();
    } else {
        baseCcy_ = baseCcyParam_;
    }

    DLOG("base ccy is " << baseCcy_
                        << (amcCam_ != nullptr ? "(this choice might be overwritten below for AMC builders)" : ""));
}

std::string ScriptedTradeEngineBuilder::getEqCcy(const IndexInfo& e) {
    QL_REQUIRE(e.isEq(), "ScriptedTradeEngineBuilder::getEqCcy(): expected eq index, got " << e.name());
    // the eq currency can only be retrieved from the market
    Currency tmp = market_->equityCurve(e.eq()->name(), configuration(MarketContext::pricing))->currency();
    QL_REQUIRE(!tmp.empty(), "ScriptedTradeEngineBuilder: Cannot find currency for equity '"
                                 << e.eq()->name() << "'. Check if equity is present in curveconfig.");
    return tmp.code();
}

std::string ScriptedTradeEngineBuilder::getCommCcy(const IndexInfo& e) {
    QL_REQUIRE(e.isComm(), "ScriptedTradeEngineBuilder::getCommCcy(): expected comm index, got " << e.name());
    // the comm currency can only be retrieved from the market
    Currency tmp = market_->commodityPriceCurve(e.commName(), configuration(MarketContext::pricing))->currency();
    QL_REQUIRE(!tmp.empty(), "ScriptedTradeEngineBuilder: Cannot find currency for commodity '"
                                 << e.commName() << "'. Check if Commodity is present in curveconfig.");
    return tmp.code();
}

void ScriptedTradeEngineBuilder::compileModelCcyList() {
    std::set<std::string> tmpCcys;
    tmpCcys.insert(baseCcy_);

    DLOG("Compile the model currencies list");

    for (auto const& c : payCcys_) {
        tmpCcys.insert(c);
    }

    for (auto const& i : fxIndices_) {
        std::string f = i.fx()->sourceCurrency().code();
        std::string d = i.fx()->targetCurrency().code();
        tmpCcys.insert(f);
        tmpCcys.insert(d);
    }

    // ir index currencies are only added for the cam model, for bs or local vol they are not needed
    // inf index currencies are not needed for the dk in the cam model, but for jy they are
    if (modelParam_ == "GaussianCam") {
        for (auto const& i : irIndices_)
            tmpCcys.insert(i.ir()->currency().code());
        if (infModelType_ == "DK") {
            for (auto const& i : infIndices_)
                tmpCcys.insert(i.inf()->currency().code());
        }
    }

    // we only add eqCurrencies / comCurrencies to the modelCcys if we
    // - build the GaussianCam model which requires all relevant currencies to be present
    // - or require a dynamic FX process for each currency
    // In case we build a GaussianCam and fullDynamicIr_ = false, there will be a zero vol process set up
    // for the IR component though, if there is no requirement for the currency from an IR index.
    // If fullDynamicFx_ = false the FX components for that currency will be zero vol, too, if there is no
    // FX index requiring the ccy. See buildGaussianCam().
    if (fullDynamicFx_ || modelParam_ == "GaussianCam") {
        for (auto const& e : eqIndices_) {
            tmpCcys.insert(getEqCcy(e));
        }
        for (auto const& c : commIndices_) {
            tmpCcys.insert(getCommCcy(c));
        }
    }

    // if we build an AMC builder, we set the base ccy to the amc model base ccy, otherwise we won't have the
    // required FX spot processes in the projected model we use for the scripted trade; the only exception is
    // if we have only one ccy in the final scripted trade model anyway (i.e. only one IR process), in which
    // case we can go with that one currency and don't need a more complicated model
    if (amcCam_ != nullptr) {
        std::string newBaseCcy_ = amcCam_->ir(0)->currency().code();
        if (newBaseCcy_ == baseCcy_) {
            DLOG("base ccy and AMC model base ccy are identical (" << baseCcy_ << ")");
        } else {
            if (tmpCcys.size() > 1) {
                DLOG("base ccy " << baseCcy_ << " is overwritten with AMC model base ccy " << newBaseCcy_
                                 << ", since more than one ccy is needed in the final model.");
                baseCcy_ = newBaseCcy_;
            } else {
                DLOG("base ccy " << baseCcy_ << " is kept although AMC model base ccy is different (" << newBaseCcy_
                                 << "), because it is a single currency model");
            }
        }
    }

    // build currency vector with the base ccy at the front
    modelCcys_ = std::vector<std::string>(1, baseCcy_);
    for (auto const& c : tmpCcys)
        if (c != baseCcy_)
            modelCcys_.push_back(c);

    // log ccys
    for (auto const& c : modelCcys_)
        DLOG("model ccy " << c << " added");
}

void ScriptedTradeEngineBuilder::compileModelIndexLists() {
    for (auto const& i : eqIndices_) {
        modelIndices_.push_back(i.name());
        modelIndicesCurrencies_.push_back(getEqCcy(i));
        DLOG("added model index " << modelIndices_.back());
    }

    for (auto const& i : commIndices_) {
        modelIndices_.push_back(i.name());
        modelIndicesCurrencies_.push_back(getCommCcy(i));
        DLOG("added model index " << modelIndices_.back());
    }

    // cover the ccys from the actual fx indices
    std::set<std::string> coveredCcys;
    coveredCcys.insert(baseCcy_);
    for (auto const& i : fxIndices_) {
        std::string targetCcy = i.fx()->targetCurrency().code();
        std::string sourceCcy = i.fx()->sourceCurrency().code();
        if (sourceCcy != baseCcy_ &&
            std::find(coveredCcys.begin(), coveredCcys.end(), sourceCcy) == coveredCcys.end()) {
            modelIndices_.push_back("FX-GENERIC-" + sourceCcy + "-" + baseCcy_);
            modelIndicesCurrencies_.push_back(sourceCcy);
            coveredCcys.insert(sourceCcy);
            DLOG("added model index " << modelIndices_.back());
        }
        if (targetCcy != baseCcy_ &&
            std::find(coveredCcys.begin(), coveredCcys.end(), targetCcy) == coveredCcys.end()) {
            modelIndices_.push_back("FX-GENERIC-" + targetCcy + "-" + baseCcy_);
            modelIndicesCurrencies_.push_back(targetCcy);
            coveredCcys.insert(targetCcy);
            DLOG("added model index " << modelIndices_.back());
        }
    }

    // cover the remaining model currencies, if we require this via the fullDynamicFx parameter
    if (fullDynamicFx_) {
        for (Size i = 1; i < modelCcys_.size(); ++i) {
            if (std::find(coveredCcys.begin(), coveredCcys.end(), modelCcys_[i]) == coveredCcys.end()) {
                modelIndices_.push_back("FX-GENERIC-" + modelCcys_[i] + "-" + baseCcy_);
                modelIndicesCurrencies_.push_back(modelCcys_[i]);
                coveredCcys.insert(modelCcys_[i]);
                DLOG("added model index " << modelIndices_.back() << " (since fullDynamicFx = true)");
            }
        }
    }

    for (auto const& i : irIndices_) {
        if (i.irSwap())
            modelIrIndices_.push_back(
                std::make_pair(i.name(), *market_->swapIndex(i.name(), configuration(MarketContext::pricing))));
        else
            modelIrIndices_.push_back(
                std::make_pair(i.name(), *market_->iborIndex(i.name(), configuration(MarketContext::pricing))));
        DLOG("added model ir index " << i.name());
    }

    for (auto const& i : infIndices_) {
        modelInfIndices_.push_back(
            std::make_pair(i.name(), *market_->zeroInflationIndex(i.infName(), configuration(MarketContext::pricing))));
        DLOG("added model inf index " << i.name());
    }
}

void ScriptedTradeEngineBuilder::setupCorrelations() {

    if (zeroVolatility_) {
        DLOG("skipping correlation setup because we are using zero volatility");
        return;
    }

    // collect pairs of model index names and correlation curve lookup names
    std::set<std::pair<std::string, std::string>> tmp;

    // EQ, FX, COMM indices
    for (auto const& m : modelIndices_) {
        IndexInfo ind(m);
        if (ind.isComm()) {
            // for COMM indices we expect the correlation on the COMM name level (not on single futures)
            // notice we might have different COMM indices on the same name (via spot, future, dynamic future
            // reference) - for those the correlationCurve() call below will return a constant 1.0 correlation
            tmp.insert(std::make_pair(m, "COMM-" + ind.commName()));
        } else {
            // for EQ, FX the lookup name is the same as the model index name
            tmp.insert(std::make_pair(m, m));
        }
    }

    // need the ir, inf indices only for GaussianCam
    if (modelParam_ == "GaussianCam") {
        for (auto const& irIdx : modelIrIndices_) {
            // for IR the lookup name is the same as the model index name
            tmp.insert(std::make_pair(irIdx.first, irIdx.first));
        }
        for (auto const& infIdx : modelInfIndices_) {
            // for INF the lookup name is without the #L, #F suffix
            IndexInfo ind(infIdx.first);
            tmp.insert(std::make_pair(infIdx.first, ind.infName()));
        }
    }

    DLOG("adding correlations for indices:");
    for (auto const& n : tmp)
        DLOG("model index '" << n.first << "' lookup name '" << n.second << "'");

    std::vector<std::pair<std::string, std::string>> corrModelIndices(tmp.begin(), tmp.end());

    for (Size i = 0; i < corrModelIndices.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            try {
                correlations_[std::make_pair(corrModelIndices[i].first, corrModelIndices[j].first)] =
                    correlationCurve(corrModelIndices[i].second, corrModelIndices[j].second);
                DLOG("added correlation for " << corrModelIndices[j].second << " ~ " << corrModelIndices[i].second);
            } catch (const std::exception& e) {
                WLOG("no correlation provided for " << corrModelIndices[j].second << " ~ " << corrModelIndices[i].second
                                                    << "(" << e.what() << ")");
            }
        }
    }
}

void ScriptedTradeEngineBuilder::setLastRelevantDate() {
    lastRelevantDate_ = Date::minDate();
    for (auto const& s : staticAnalyser_->indexEvalDates())
        for (auto const& d : s.second)
            lastRelevantDate_ = std::max(lastRelevantDate_, d);
    for (auto const& d : staticAnalyser_->regressionDates())
        lastRelevantDate_ = std::max(lastRelevantDate_, d);
    for (auto const& s : staticAnalyser_->payObsDates())
        for (auto const& d : s.second)
            lastRelevantDate_ = std::max(lastRelevantDate_, d);
    for (auto const& s : staticAnalyser_->payPayDates())
        for (auto const& d : s.second)
            lastRelevantDate_ = std::max(lastRelevantDate_, d);
    for (auto const& s : staticAnalyser_->discountObsDates())
        for (auto const& d : s.second)
            lastRelevantDate_ = std::max(lastRelevantDate_, d);
    for (auto const& s : staticAnalyser_->discountPayDates())
        for (auto const& d : s.second)
            lastRelevantDate_ = std::max(lastRelevantDate_, d);
    DLOG("last relevant date: " << lastRelevantDate_);
}

void ScriptedTradeEngineBuilder::setupBlackScholesProcesses() {
    Handle<BlackVolTermStructure> vol;
    if (zeroVolatility_) {
        vol = Handle<BlackVolTermStructure>(
            QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), 0.0, ActualActual(ActualActual::ISDA)));
        DLOG("using zero volatility processes");
    }
    for (Size i = 0; i < modelIndices_.size(); ++i) {
        IndexInfo ind(modelIndices_[i]);
        if (ind.isEq()) {
            std::string name = ind.eq()->name();
            auto spot = market_->equitySpot(name, configuration(MarketContext::pricing));
            auto div = market_->equityDividendCurve(name, configuration(MarketContext::pricing));
            auto fc = market_->equityForecastCurve(name, configuration(MarketContext::pricing));
            if (!zeroVolatility_)
                vol = market_->equityVol(name, configuration(MarketContext::pricing));
            processes_.push_back(QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spot, div, fc, vol));
            DLOG("added process for equity " << name);
        } else if (ind.isComm()) {
            std::string name = ind.commName();
            auto spot = Handle<Quote>(QuantLib::ext::make_shared<DerivedPriceQuote>(
                market_->commodityPriceCurve(name, configuration(MarketContext::pricing))));
            auto priceCurve = market_->commodityPriceCurve(name, configuration(MarketContext::pricing));
            auto fc = modelIndicesCurrencies_[i] == baseCcy_
                          ? modelCurves_.front()
                          : market_->discountCurve(modelIndicesCurrencies_[i], configuration(MarketContext::pricing));
            auto div = Handle<YieldTermStructure>(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*priceCurve, *fc));
            div->enableExtrapolation();
            if (!zeroVolatility_)
                vol = market_->commodityVolatility(name, configuration(MarketContext::pricing));
            processes_.push_back(QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spot, div, fc, vol));
            DLOG("added process for commodity " << name);
        } else if (ind.isFx()) {
            std::string targetCcy = ind.fx()->targetCurrency().code();
            std::string sourceCcy = ind.fx()->sourceCurrency().code();
            auto spot = market_->fxSpot(sourceCcy + targetCcy, configuration(MarketContext::pricing));
            auto div = sourceCcy == baseCcy_ ? modelCurves_.front()
                                             : market_->discountCurve(sourceCcy, configuration(MarketContext::pricing));
            auto fc = targetCcy == baseCcy_ ? modelCurves_.front()
                                            : market_->discountCurve(targetCcy, configuration(MarketContext::pricing));
            if (!zeroVolatility_)
                vol = market_->fxVol(sourceCcy + targetCcy, configuration(MarketContext::pricing));
            processes_.push_back(QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spot, div, fc, vol));
            DLOG("added process for fx " << sourceCcy << "-" << targetCcy);
        } else {
            QL_FAIL("unexpected model index " << ind);
        }
    }
}

void ScriptedTradeEngineBuilder::setupIrReversions() {
    if (zeroVolatility_) {
        DLOG("skipping IR reversion setup because we are using zero volatility");
        return;
    }
    // get reversions for ir index currencies ...
    std::set<std::string> irCcys;
    for (auto const& i : irIndices_)
        irCcys.insert(i.ir()->currency().code());
    // ... or all currencies if we require dynamic processes for all
    if (fullDynamicIr_) {
        irCcys.insert(modelCcys_.begin(), modelCcys_.end());
    }
    for (auto const& ccy : irCcys) {
        string revStr = modelParameter("IrReversion_" + ccy, {resolvedProductTag_}, false, "");
        if (revStr.empty())
            revStr = modelParameter("IrReversion", {resolvedProductTag_}, false, "");
        QL_REQUIRE(!revStr.empty(), "Did not find reversion for "
                                        << ccy
                                        << ", need IrReversion_CCY or IrReversion parameter in pricing engine config.");
        irReversions_[ccy] = parseReal(revStr);
        DLOG("got Hull White reversion " << irReversions_[ccy] << " for " << ccy);
    }
}

namespace {
QuantLib::ext::shared_ptr<ZeroInflationIndex>
getInfMarketIndex(const std::string& name,
                  const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& indices) {
    for (auto const& i : indices) {
        if (i.first == name)
            return i.second;
    }
    QL_FAIL("ScriptedTradeEngineBuilder::compileSimulationAndAddDates(): did not find zero inflation index '"
            << name << "' in model indices, this is unexpected");
}
} // namespace

void ScriptedTradeEngineBuilder::compileSimulationAndAddDates() {
    DLOG("compile simulation and additional dates...");

    for (auto const& s : staticAnalyser_->indexEvalDates()) {
        IndexInfo info(s.first);
        // skip generic indices, for them we do not to add the obs date to sim or add dates
        if (info.isGeneric())
            continue;
        // need ir / inf index observation dates only for GaussianCam as simulation dates, for LocalVol, BS
        // we don't need to add them at all
        if ((!info.isIr() && !info.isInf()) || modelParam_ == "GaussianCam") {
            if (info.isInf()) {
                // inf needs special considerations
                QuantLib::ext::shared_ptr<ZeroInflationIndex> marketIndex = getInfMarketIndex(info.name(), modelInfIndices_);
                Size lag = getInflationSimulationLag(marketIndex);
                for (auto const& d : s.second) {
                    auto lim = inflationPeriod(d, info.inf()->frequency());
                    simulationDates_.insert(lim.first + lag);
                    QL_DEPRECATED_DISABLE_WARNING
                    // This will be removed in a later release and all inf indices are then flat
                    if (info.inf()->interpolated())
                        simulationDates_.insert(d + lag);
                    QL_DEPRECATED_ENABLE_WARNING
                }
            } else {
                // for all other indices we just take the original dates
                simulationDates_.insert(s.second.begin(), s.second.end());
            }
            DLOG("added " << s.second.size() << " simulation dates for '" << s.first << "' (from eval op obs dates)");
        }
    }

    for (auto const& s : staticAnalyser_->indexFwdDates()) {
        IndexInfo info(s.first);
        // do not need ir / inf index fwd dates (not for LocalVol, BS, but also not for GaussianCam)
        if (!info.isIr() && !info.isInf()) {
            addDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " additional dates for '" << s.first << "' (from eval op fwd dates)");
        }
    }

    for (auto const& s : staticAnalyser_->payObsDates()) {
        // need pay obs dates as simulation dates only for GaussianCam, for Local Vol, BS
        // add them as addDaes except the pay ccy is not base and there is an fx index with
        // the pay ccy as for ccy (then the simulated fx index will be used for ccy conversion)
        std::string payCcy = s.first;
        if (modelParam_ == "GaussianCam" ||
            (baseCcy_ != payCcy &&
             std::find_if(modelIndices_.begin(), modelIndices_.end(), [&payCcy](const std::string& s) {
                 IndexInfo ind(s);
                 return ind.isFx() && ind.fx()->sourceCurrency().code() == payCcy;
             }) != modelIndices_.end())) {
            simulationDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " simulation dates for '" << payCcy << "' (from pay() obs dates)");
        } else {
            addDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " additional dates for '" << payCcy << "' (from pay() obs dates)");
        }
    }

    simulationDates_.insert(staticAnalyser_->regressionDates().begin(), staticAnalyser_->regressionDates().end());
    DLOG("added " << staticAnalyser_->regressionDates().size() << " simulation dates (from npv() regression dates)");

    for (auto const& s : staticAnalyser_->payPayDates()) {
        addDates_.insert(s.second.begin(), s.second.end());
        DLOG("added " << s.second.size() << " additional dates for '" << s.first << "' (from pay() pay dates)");
    }

    for (auto const& s : staticAnalyser_->discountObsDates()) {
        // need disocunt obs dates only for GaussianCam as simulation dates, for Local Vol, BS
        // add them as addDates
        if (modelParam_ == "GaussianCam") {
            simulationDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " simulation dates for '" << s.first
                          << "' (from discount() obs dates)");
        } else {
            addDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " additional dates for '" << s.first
                          << "' (from discount() obs dates)");
        }
        DLOG("added " << s.second.size() << " additional dates for '" << s.first << "' (from discount() obs dates)");
    }

    for (auto const& s : staticAnalyser_->discountPayDates()) {
        addDates_.insert(s.second.begin(), s.second.end());
        DLOG("added " << s.second.size() << " additional dates for '" << s.first << "' (from discount() pay dates)");
    }

    for (auto const& s : staticAnalyser_->fwdCompAvgEvalDates()) {
        // need fwd comp eval dates only for GaussianCam as simulation dates, for Local Vol, BS
        // add them as addDates
        if (modelParam_ == "GaussianCam") {
            simulationDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " simulation dates for '" << s.first
                          << "' (from fwd[Comp|Avg]() obs dates)");
        } else {
            addDates_.insert(s.second.begin(), s.second.end());
            DLOG("added " << s.second.size() << " additional dates for '" << s.first
                          << "' (from fwd[Comp|Avg]() obs dates)");
        }
    }

    for (auto const& s : staticAnalyser_->fwdCompAvgStartEndDates()) {
        addDates_.insert(s.second.begin(), s.second.end());
        DLOG("added " << s.second.size() << " additional dates for '" << s.first
                      << "' (from fwd[Comp|Avg]() start/end dates)");
    }
}

namespace {

// filter out "dummy" strikes, such as up and out barriers set to 1E6 to indicate +inf
std::map<std::string, std::vector<Real>> filterBlackScholesCalibrationStrikes(
    const std::map<std::string, std::vector<Real>>& strikes, const std::vector<std::string>& modelIndices,
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes, const Real T) {
    QL_REQUIRE(modelIndices.size() == processes.size(), "filterBlackScholesCalibrationStrikes: processes size ("
                                                            << processes.size() << ") must match modelIndices size ("
                                                            << modelIndices.size() << ")");
    std::map<std::string, std::vector<Real>> result;
    if (T < 0.0 || close_enough(T, 0.0)) {
        DLOG("excluding all calibration strikes, because last relevant time is not positive (" << T << ")");
        return result;
    }
    const Real normInvEps = 2.0 * InverseCumulativeNormal()(1 - 1E-6); // hardcoded epsilon
    for (auto const& ks : strikes) {
        auto m = std::find(modelIndices.begin(), modelIndices.end(), ks.first);
        if (m != modelIndices.end()) {
            Size index = std::distance(modelIndices.begin(), m);
            Real atmf = processes[index]->x0() / processes[index]->riskFreeRate()->discount(T) *
                        processes[index]->dividendYield()->discount(T);
            Real sigmaSqrtT =
                std::max(processes[index]->blackVolatility()->blackVol(T, atmf), 0.1) * std::sqrt(std::max(T, 1.0));
            Real xmin = std::exp(std::log(atmf) - normInvEps * sigmaSqrtT);
            Real xmax = std::exp(std::log(atmf) + normInvEps * sigmaSqrtT);
            for (auto const k : ks.second) {
                if (k < xmin || k > xmax) {
                    DLOG("excluding calibration strike (" << k << ") for index '" << modelIndices[index]
                                                          << "', bounds = [" << xmin << "," << xmax << "]");
                } else {
                    result[ks.first].push_back(k);
                }
            }
        } else {
            result[ks.first] = ks.second;
        }
    }
    return result;
}

// build vector of calibration strikes per model index from map
std::vector<std::vector<Real>> getCalibrationStrikesVector(const std::map<std::string, std::vector<Real>>& strikes,
                                                           const std::vector<std::string>& modelIndices) {
    std::vector<std::vector<Real>> result;
    for (auto const& m : modelIndices) {
        auto s = strikes.find(m);
        if (s != strikes.end())
            result.push_back(s->second);
        else
            result.push_back({});
    }
    return result;
}

} // namespace

void ScriptedTradeEngineBuilder::buildBlackScholes(const std::string& id,
                                                   const IborFallbackConfig& iborFallbackConfig) {
    Real T = modelCurves_.front()->timeFromReference(lastRelevantDate_);
    auto filteredStrikes = filterBlackScholesCalibrationStrikes(calibrationStrikes_, modelIndices_, processes_, T);
    // ignore timeStepsPerYear if we have no correlations, i.e. we can take large timesteps without changing anything
    auto builder = QuantLib::ext::make_shared<BlackScholesModelBuilder>(
        modelCurves_, processes_, simulationDates_, addDates_, correlations_.empty() ? 0 : timeStepsPerYear_,
        calibration_, getCalibrationStrikesVector(filteredStrikes, modelIndices_));
    if (useCg_) {
        modelCG_ = QuantLib::ext::make_shared<BlackScholesCG>(
            modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_, modelIndices_,
            modelIndicesCurrencies_, builder->model(), correlations_, simulationDates_, iborFallbackConfig,
            calibration_, filteredStrikes);
    } else {
        model_ = QuantLib::ext::make_shared<BlackScholes>(modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_,
                                                  modelInfIndices_, modelIndices_, modelIndicesCurrencies_,
                                                  builder->model(), correlations_, mcParams_, simulationDates_,
                                                  iborFallbackConfig, calibration_, filteredStrikes);
    }
    modelBuilders_.insert(std::make_pair(id, builder));
}

void ScriptedTradeEngineBuilder::buildFdBlackScholes(const std::string& id,
                                                     const IborFallbackConfig& iborFallbackConfig) {
    Real T = modelCurves_.front()->timeFromReference(lastRelevantDate_);
    auto filteredStrikes = filterBlackScholesCalibrationStrikes(calibrationStrikes_, modelIndices_, processes_, T);
    auto builder = QuantLib::ext::make_shared<BlackScholesModelBuilder>(
        modelCurves_, processes_, simulationDates_, addDates_, timeStepsPerYear_, calibration_,
        getCalibrationStrikesVector(filteredStrikes, modelIndices_));
    model_ = QuantLib::ext::make_shared<FdBlackScholesBase>(
        modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_, modelIndices_,
        modelIndicesCurrencies_, payCcys_, builder->model(), correlations_, simulationDates_, iborFallbackConfig,
        calibration_, filteredStrikes, mesherEpsilon_, mesherScaling_, mesherConcentration_,
        mesherMaxConcentratingPoints_, mesherIsStatic_);
    modelBuilders_.insert(std::make_pair(id, builder));
}

void ScriptedTradeEngineBuilder::buildLocalVol(const std::string& id, const IborFallbackConfig& iborFallbackConfig) {
    LocalVolModelBuilder::Type lvType;
    if (modelParam_ == "LocalVolDupire")
        lvType = LocalVolModelBuilder::Type::Dupire;
    else if (modelParam_ == "LocalVolAndreasenHuge")
        lvType = LocalVolModelBuilder::Type::AndreasenHuge;
    else {
        QL_FAIL("local vol model type " << modelParam_ << " not recognised.");
    }
    auto builder = QuantLib::ext::make_shared<LocalVolModelBuilder>(modelCurves_, processes_, simulationDates_, addDates_,
                                                            timeStepsPerYear_, lvType, calibrationMoneyness_,
                                                            !calibrate_ || zeroVolatility_);
    model_ = QuantLib::ext::make_shared<LocalVol>(modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_,
                                          modelInfIndices_, modelIndices_, modelIndicesCurrencies_, builder->model(),
                                          correlations_, mcParams_, simulationDates_, iborFallbackConfig);
    modelBuilders_.insert(std::make_pair(id, builder));
}

namespace {
// return first ir ibor index in given set whose currency matches the parameter ccy, or ccy if no such index exists
std::string getFirstIrIndexOrCcy(const std::string& ccy, const std::set<IndexInfo> irIndices) {
    for (auto const& index : irIndices) {
        if (index.isIrSwap() && index.irSwap()->iborIndex()->currency().code() == ccy)
            return IndexNameTranslator::instance().oreName(index.irSwap()->iborIndex()->name());
        if (index.isIrIbor() && index.irIbor()->currency().code() == ccy)
            return index.name();
    }
    return ccy;
}
} // namespace

void ScriptedTradeEngineBuilder::buildGaussianCam(const std::string& id, const IborFallbackConfig& iborFallbackConfig,
                                                  const std::vector<std::string>& conditionalExpectationModelStates) {
    // compile cam correlation matrix
    // - we want to use the maximum tenor of an ir index in a correlation pair if several are given (to have
    //   a well defined rule how to derive the LGM IR correlations); to get there we store the correlations
    //   together with the index tenors (or null if not applicatble), so that we can decide if we overwrite
    //   an existing correlation with another candidate or not
    // - correlations are for index pair names and must be constant; if not given for a pair, we assume zero
    //   correlation;
    // - correlations for IR processes are taken from IR index correlations, if several indices exist
    //   for one ccy, the index with the longest tenor T is selected; we do not apply an LGM adjustment for this
    //   value due to time dependent volatility, since this is usually negligible
    // - for inf we do not apply an adjustment either => TODO is that suitable for both DK and JY approximately?
    // - for inf JY we have two driving factors (f1,f2) where f1 drives the real rate process and f2 drives the
    //   inflation index ("fx") process; we assume the correlation between f1 and any other factor (including f2)
    //   to be zero and set the correlation between f2 and any other factor to the correlation read from the
    //   market data for the inflation index, TODO is that assumption reasonable?
    std::map<std::pair<std::string, std::string>,
             std::tuple<Handle<QuantExt::CorrelationTermStructure>, Period, Period>>
        tmpCorrelations;
    for (auto const& c : correlations_) {
        std::pair<std::string, Period> firstEntry = convertIndexToCamCorrelationEntry(c.first.first);
        std::pair<std::string, Period> secondEntry = convertIndexToCamCorrelationEntry(c.first.second);
        // if we have identical CAM entries (e.g. IR:EUR, IR:EUR) we skip this pair, since we can't specify a
        // correlation in this case
        if (firstEntry.first == secondEntry.first)
            continue;
        auto e = tmpCorrelations.find(std::make_pair(firstEntry.first, secondEntry.first));
        if (e == tmpCorrelations.end() ||
            (firstEntry.second > std::get<1>(e->second) && secondEntry.second > std::get<2>(e->second))) {
            tmpCorrelations[std::make_pair(firstEntry.first, secondEntry.first)] =
                std::make_tuple(c.second, firstEntry.second, secondEntry.second);
        }
    }

    map<CorrelationKey, Handle<Quote>> camCorrelations;
    for (auto const& c : tmpCorrelations) {
        CorrelationFactor f_1 = parseCorrelationFactor(c.first.first, '#');
        CorrelationFactor f_2 = parseCorrelationFactor(c.first.second, '#');
        // update index for JY from 0 to 1 (i.e. to the factor driving the inf index ("fx") process)
        // in all other cases the index 0 is fine, since there is only one driving factor always
        if (infModelType_ == "JY") {
            if (f_1.type == CrossAssetModel::AssetType::INF)
                f_1.index = 1;
            if (f_2.type == CrossAssetModel::AssetType::INF)
                f_2.index = 1;
        }
        auto q = Handle<Quote>(QuantLib::ext::make_shared<CorrelationValue>(std::get<0>(c.second), 0.0));
        camCorrelations[std::make_pair(f_1, f_2)] = q;
        DLOG("added correlation for " << c.first.first << "/" << c.first.second << ": " << q->value());
    }

    // correlation overwrite from pricing engine parameters

    std::set<CorrelationFactor> allCorrRiskFactors;

    for (auto const& m : modelIndices_)
        allCorrRiskFactors.insert(parseCorrelationFactor(convertIndexToCamCorrelationEntry(m).first, '#'));
    for (auto const& m : modelIrIndices_)
        allCorrRiskFactors.insert(parseCorrelationFactor(convertIndexToCamCorrelationEntry(m.first).first, '#'));
    for (auto const& m : modelInfIndices_)
        allCorrRiskFactors.insert(parseCorrelationFactor(convertIndexToCamCorrelationEntry(m.first).first, '#'));
    for (auto const& ccy : modelCcys_)
        allCorrRiskFactors.insert({CrossAssetModel::AssetType::IR, ccy, 0});

    for (auto const& c1 : allCorrRiskFactors) {
        for (auto const& c2 : allCorrRiskFactors) {
            // determine the number of driving factors for f_1 and f_2
            Size nf_1 = c1.type == CrossAssetModel::AssetType::INF && infModelType_ == "JY" ? 2 : 1;
            Size nf_2 = c2.type == CrossAssetModel::AssetType::INF && infModelType_ == "JY" ? 2 : 1;
            for (Size k = 0; k < nf_1; ++k) {
                for (Size l = 0; l < nf_2; ++l) {
                    auto f_1 = c1;
                    auto f_2 = c2;
                    f_1.index = k;
                    f_2.index = l;
                    if (f_1 == f_2)
                        continue;
                    // lookup names are IR:GBP:0 and IR:GBP whenever the index is zero
                    auto s_1 = ore::data::to_string(f_1);
                    auto s_2 = ore::data::to_string(f_2);
                    std::set<std::string> lookupnames1, lookupnames2;
                    lookupnames1.insert(s_1);
                    lookupnames2.insert(s_2);
                    if (k == 0)
                        lookupnames1.insert(s_1.substr(0, s_1.size() - 2));
                    if (l == 0)
                        lookupnames2.insert(s_2.substr(0, s_2.size() - 2));
                    for (auto const& l1 : lookupnames1) {
                        for (auto const& l2 : lookupnames2) {
                            if (auto overwrite = modelParameter(
                                    "Correlation",
                                    {resolvedProductTag_ + "_" + l1 + "_" + l2, l1 + "_" + l2, resolvedProductTag_},
                                    false);
                                !overwrite.empty()) {
                                camCorrelations[std::make_pair(f_1, f_2)] =
                                    Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parseReal(overwrite)));
                            }
                        }
                    }
                }
            }
        }
    }

    // set up the cam and calibrate it using the cam builder
    // if for a non-base currency no fx index is given, we set up a zero vol FX process for this
    // if fullDynamicIr is false, we set up a zero vol IR process for currencies that are not irIndex currencies

    std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs;
    std::vector<QuantLib::ext::shared_ptr<InflationModelData>> infConfigs;
    std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;
    std::vector<QuantLib::ext::shared_ptr<EqBsData>> eqConfigs;
    std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>> comConfigs;

    // calibration expiries and terms for IR, INF, FX, EQ parametrisations (this will only work for a fixed reference
    // date, due to the way the cam builder and nested builders work, see ticket #940)
    Date referenceDate = modelCurves_.front()->referenceDate();
    std::vector<Date> calibrationDates;
    std::vector<std::string> calibrationExpiries, calibrationTerms;

    for (auto const& d : simulationDates_) {
        if (d > referenceDate) {
            calibrationDates.push_back(d);
            calibrationExpiries.push_back(ore::data::to_string(d));
            // make sure the underlying swap has at least 1M to run, QL will throw otherwise during calibration
            calibrationTerms.push_back(ore::data::to_string(std::max(d + 1 * Months, lastRelevantDate_)));
        }
    }

    // calibration times (need one less than calibration dates)
    std::vector<Real> calibrationTimes;
    for (Size i = 0; i + 1 < calibrationDates.size(); ++i) {
        calibrationTimes.push_back(modelCurves_.front()->timeFromReference(calibrationDates[i]));
    }

    // IR configs
    for (Size i = 0; i < modelCcys_.size(); ++i) {
        auto config = QuantLib::ext::make_shared<IrLgmData>();
        config->qualifier() = getFirstIrIndexOrCcy(modelCcys_[i], irIndices_);
        config->reversionType() = LgmData::ReversionType::HullWhite;
        config->volatilityType() = LgmData::VolatilityType::Hagan;
        config->calibrateH() = false;
        config->hParamType() = ParamType::Constant;
        config->hTimes() = std::vector<Real>();
        config->shiftHorizon() =
            modelCurves_.front()->timeFromReference(lastRelevantDate_) * 0.5; // TODO hardcode 0.5 here?
        config->scaling() = 1.0;
        std::string ccy = modelCcys_[i];
        // if we don't require fullDynamicIr and there is no model index in the currency, we set up a zero
        // vol IR component for this ccy; we also do this if zero vol is specified in the model params
        if (calibrationExpiries.empty() || zeroVolatility_ ||
            (!fullDynamicIr_ &&
             std::find_if(modelIrIndices_.begin(), modelIrIndices_.end(),
                          [&ccy](const std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>& index) {
                              return index.second->currency().code() == ccy;
                          }) == modelIrIndices_.end())) {

            DLOG("set up zero vol IrLgmData for currency '" << modelCcys_[i] << "'");
            // zero vol
            config->calibrationType() = CalibrationType::None;
            config->hValues() = {0.0};
            config->calibrateA() = false;
            config->aParamType() = ParamType::Constant;
            config->aTimes() = std::vector<Real>();
            config->aValues() = {0.0};
        } else {
            DLOG("set up IrLgmData for currency '" << modelCcys_[i] << "'");
            // bootstrapped on atm swaption vols
            auto rev = irReversions_.find(modelCcys_[i]);
            QL_REQUIRE(rev != irReversions_.end(), "reversion for ccy " << modelCcys_[i] << " not found");
            config->calibrationType() = CalibrationType::Bootstrap;
            config->hValues() = {rev->second};
            config->calibrateA() = true;
            config->aParamType() = ParamType::Piecewise;
            config->aTimes() = calibrationTimes;
            config->aValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030); // start value for optimiser
            config->optionExpiries() = calibrationExpiries;
            config->optionTerms() = calibrationTerms;
            config->optionStrikes() =
                std::vector<std::string>(calibrationExpiries.size(), "ATM"); // hardcoded ATM calibration strike
        }
        irConfigs.push_back(config);
    }

    // INF configs
    for (Size i = 0; i < modelInfIndices_.size(); ++i) {
        QuantLib::ext::shared_ptr<InflationModelData> config;
        if (zeroVolatility_) {
            // for both DK and JY we can just use a zero vol dk component
            config = QuantLib::ext::make_shared<InfDkData>(
                CalibrationType::None, std::vector<CalibrationBasket>(), modelInfIndices_[i].second->currency().code(),
                IndexInfo(modelInfIndices_[i].first).infName(),
                ReversionParameter(LgmData::ReversionType::Hagan, true, ParamType::Constant, {}, {0.60}),
                VolatilityParameter(LgmData::VolatilityType::Hagan, false, ParamType::Constant, {}, {0.00}),
                LgmReversionTransformation(), true);
        } else {
            // build calibration basket (CPI Floors at calibration strike or if that is not given, ATM strike)
            QuantLib::ext::shared_ptr<BaseStrike> calibrationStrike;
            if (auto k = calibrationStrikes_.find(modelInfIndices_[i].first);
                k != calibrationStrikes_.end() && !k->second.empty()) {
                calibrationStrike = QuantLib::ext::make_shared<AbsoluteStrike>(k->second.front());
            } else {
                calibrationStrike = QuantLib::ext::make_shared<AtmStrike>(QuantLib::DeltaVolQuote::AtmType::AtmFwd);
            }
            std::vector<QuantLib::ext::shared_ptr<CalibrationInstrument>> calInstr;
            for (auto const& d : calibrationDates)
                calInstr.push_back(
                    QuantLib::ext::make_shared<CpiCapFloor>(QuantLib::CapFloor::Type::Floor, d, calibrationStrike));
            std::vector<CalibrationBasket> calBaskets(1, CalibrationBasket(calInstr));
            if (infModelType_ == "DK") {
                // build DK config
                std::string infName = IndexInfo(modelInfIndices_[i].first).infName();
                Real vol = parseReal(modelParameter("InfDkVolatility",
                                                    {resolvedProductTag_ + "_" + infName, infName, resolvedProductTag_},
                                                    false, "0.0050"));
                config = QuantLib::ext::make_shared<InfDkData>(
                    CalibrationType::Bootstrap, calBaskets, modelInfIndices_[i].second->currency().code(),
                    IndexInfo(modelInfIndices_[i].first).infName(),
                    ReversionParameter(LgmData::ReversionType::Hagan, true, ParamType::Piecewise, {}, {0.60}),
                    VolatilityParameter(LgmData::VolatilityType::Hagan, false, ParamType::Piecewise, {}, {vol}),
                    LgmReversionTransformation(),
                    // ignore duplicate expiry times among calibration instruments
                    true);
            } else if (infModelType_ == "JY") {
                // build JY config
                // we calibrate the index ("fx") process to CPI cap/floors and set the real rate process reversion equal to
                // the nominal process reversion. The real rate vol is set to a fixed multiple of nominal rate vol, the
                // multiplier is taken from the pe config model parameter "InfJyRealToNominalVolRatio"
                std::string infName = IndexInfo(modelInfIndices_[i].first).infName();
                Size ccyIndex =
                    std::distance(modelCcys_.begin(), std::find(modelCcys_.begin(), modelCcys_.end(),
                                                                modelInfIndices_[i].second->currency().code()));
                ReversionParameter realRateRev = QuantLib::ext::static_pointer_cast<LgmData>(irConfigs[ccyIndex])->reversionParameter();
                VolatilityParameter realRateVol = QuantLib::ext::static_pointer_cast<LgmData>(irConfigs[ccyIndex])->volatilityParameter();
                realRateRev.setCalibrate(false);
                realRateVol.setCalibrate(false);
                Real realRateToNominalRateRatio = parseReal(
                    modelParameter("InfJyRealToNominalVolRatio",
                                   {resolvedProductTag_ + "_" + infName, infName, resolvedProductTag_}, false, "1.0"));
                QL_REQUIRE(ccyIndex < modelCcys_.size(),
                           "ScriptedTrade::buildGaussianCam(): internal error, inflation index currency "
                               << modelInfIndices_[i].second->currency().code() << " not found in model ccy list.");
                realRateVol.mult(realRateToNominalRateRatio);
                config = QuantLib::ext::make_shared<InfJyData>(
                    CalibrationType::Bootstrap, calBaskets, modelInfIndices_[i].second->currency().code(), infName,
                    // real rate reversion and vol
                    realRateRev, realRateVol,
                    // index ("fx") vol, start value 0.10 for calibration
                    VolatilityParameter(true, ParamType::Piecewise, {}, {0.10}),
                    // no parameter trafo, no optimisation constraints (TODO do we need boundaries?)
                    LgmReversionTransformation(), CalibrationConfiguration(),
                    // ignore duplicate expiry times among calibration instruments
                    true,
                    // link real to nominal rate params
                    true,
                    // real rate to nominal rate ratio
                    realRateToNominalRateRatio);
            } else {
                QL_FAIL("invalid infModelType '" << infModelType_ << "', expected DK or JY");
            }
        }
        infConfigs.push_back(config);
    }

    // FX configs
    for (Size i = 1; i < modelCcys_.size(); ++i) {
        auto config = QuantLib::ext::make_shared<FxBsData>();
        config->foreignCcy() = modelCcys_[i];
        config->domesticCcy() = modelCcys_[0];
        // if we do not have a FX index for the currency, we set up a zero vol process (FX indices are added above
        // for all non-base ccys if fullDynamicFx is specified)
        bool haveFxIndex = false;
        for (Size j = 0; j < modelIndices_.size(); ++j) {
            if (IndexInfo(modelIndices_[j]).isFx() && modelIndicesCurrencies_[j] == modelCcys_[i])
                haveFxIndex = true;
        }
        if (calibrationExpiries.empty() || !haveFxIndex || zeroVolatility_) {
            DLOG("set up zero vol FxBsData for currency '" << modelCcys_[i] << "'");
            // zero vols
            config->calibrationType() = CalibrationType::None;
            config->calibrateSigma() = false;
            config->sigmaParamType() = ParamType::Constant;
            config->sigmaTimes() = std::vector<Real>();
            config->sigmaValues() = {0.0};
        } else {
            DLOG("set up FxBsData for currency '" << modelCcys_[i] << "'");
            // bootstrapped on atm fx vols
            config->calibrationType() = CalibrationType::Bootstrap;
            config->calibrateSigma() = true;
            config->sigmaParamType() = ParamType::Piecewise;
            config->sigmaTimes() = calibrationTimes;
            config->sigmaValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.10); // start value for optimiser
            config->optionExpiries() = calibrationExpiries;
            config->optionStrikes() =
                std::vector<std::string>(calibrationExpiries.size(), "ATMF"); // hardcoded ATMF calibration strike
        }
        fxConfigs.push_back(config);
    }

    // EQ configs
    for (auto const& eq : eqIndices_) {
        auto config = QuantLib::ext::make_shared<EqBsData>();
        config->currency() = getEqCcy(eq);
        config->eqName() = eq.eq()->name();
        if (calibrationExpiries.empty() || zeroVolatility_) {
            DLOG("set up zero vol EqBsData for underlying " << eq.eq()->name());
            // zero vols
            config->calibrationType() = CalibrationType::None;
            config->calibrateSigma() = false;
            config->sigmaParamType() = ParamType::Constant;
            config->sigmaTimes() = std::vector<Real>();
            config->sigmaValues() = {0.0};
        } else {
            DLOG("set up EqBsData for underlying '" << eq.eq()->name() << "'");
            // bootstrapped on atm eq vols
            config->calibrationType() = CalibrationType::Bootstrap;
            config->calibrateSigma() = true;
            config->sigmaParamType() = ParamType::Piecewise;
            config->sigmaTimes() = calibrationTimes;
            config->sigmaValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.10); // start value for optimiser
            config->optionExpiries() = calibrationExpiries;
            config->optionStrikes() =
                std::vector<std::string>(calibrationExpiries.size(), "ATMF"); // hardcoded ATMF calibration strike
        }
        eqConfigs.push_back(config);
    }

    // TODO
    std::vector<QuantLib::ext::shared_ptr<CrLgmData>> crLgmConfigs;
    std::vector<QuantLib::ext::shared_ptr<CrCirData>> crCirConfigs;

    // COMM configs
    for (auto const& comm : commIndices_) {
        auto config = QuantLib::ext::make_shared<CommoditySchwartzData>();
        config->currency() = getCommCcy(comm);
        config->name() = comm.commName();
        if (calibrationExpiries.empty() || zeroVolatility_) {
            config->calibrationType() = CalibrationType::None;
            config->calibrateSigma() = false;
            config->sigmaParamType() = ParamType::Constant;
            config->sigmaValue() = 0.0;
        } else {
            config->calibrationType() = CalibrationType::BestFit;
            config->calibrateSigma() = true;
            config->sigmaParamType() = ParamType::Constant;
            config->sigmaValue() = 0.10; // start value for optimizer
            config->optionExpiries() = calibrationExpiries;
            config->optionStrikes() =
                std::vector<std::string>(calibrationExpiries.size(), "ATMF"); // hardcoded ATMF calibration strike
        }
        comConfigs.push_back(config);
    }

    std::string configurationInCcy = configuration(MarketContext::irCalibration);
    std::string configurationXois = configuration(MarketContext::pricing);
    auto discretization = useCg_ ? CrossAssetModel::Discretization::Euler : CrossAssetModel::Discretization::Exact;
    auto camBuilder = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        market_,
        QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, eqConfigs, infConfigs, crLgmConfigs, crCirConfigs,
                                                comConfigs, 0, camCorrelations, bootstrapTolerance_, "LGM",
                                                discretization),
        configurationInCcy, configurationXois, configurationXois, configurationInCcy, configurationInCcy,
        configurationXois, !calibrate_ || zeroVolatility_, continueOnCalibrationError_, referenceCalibrationGrid_,
        SalvagingAlgorithm::Spectral, id);

    // effective time steps per year: zero for exact evolution, otherwise the pricing engine parameter
    if (useCg_) {
        modelCG_ = QuantLib::ext::make_shared<GaussianCamCG>(
            camBuilder->model(), modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_,
            modelIndices_, modelIndicesCurrencies_, simulationDates_,
            camBuilder->model()->discretization() == CrossAssetModel::Discretization::Exact ? 0 : timeStepsPerYear_,
            iborFallbackConfig, std::vector<Size>(), conditionalExpectationModelStates);
    } else {
        model_ = QuantLib::ext::make_shared<GaussianCam>(
            camBuilder->model(), modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_,
            modelIndices_, modelIndicesCurrencies_, simulationDates_, mcParams_,
            camBuilder->model()->discretization() == CrossAssetModel::Discretization::Exact ? 0 : timeStepsPerYear_,
            iborFallbackConfig, std::vector<Size>(), conditionalExpectationModelStates);
    }

    modelBuilders_.insert(std::make_pair(id, camBuilder));
}

void ScriptedTradeEngineBuilder::buildFdGaussianCam(const std::string& id,
                                                    const IborFallbackConfig& iborFallbackConfig) {

    Date referenceDate = modelCurves_.front()->referenceDate();
    std::vector<Date> calibrationDates;
    std::vector<std::string> calibrationExpiries, calibrationTerms;

    for (auto const& d : simulationDates_) {
        if (d > referenceDate) {
            calibrationDates.push_back(d);
            calibrationExpiries.push_back(ore::data::to_string(d));
            // make sure the underlying swap has at least 1M to run, QL will throw otherwise during calibration
            calibrationTerms.push_back(ore::data::to_string(std::max(d + 1 * Months, lastRelevantDate_)));
        }
    }

    // calibration times (need one less than calibration dates)
    std::vector<Real> calibrationTimes;
    for (Size i = 0; i + 1 < calibrationDates.size(); ++i) {
        calibrationTimes.push_back(modelCurves_.front()->timeFromReference(calibrationDates[i]));
    }

    // determine calibration strike
    std::string calibrationStrike = "ATM";
    if(calibration_ == "Deal") {
        // first model index found in calibration strike spec and first specified strike therein is used
        for (auto const& m : modelIrIndices_) {
            if (auto f = calibrationStrikes_.find(m.first); f != calibrationStrikes_.end()) {
                if(!f->second.empty()) {
                    calibrationStrike = boost::lexical_cast<std::string>(f->second.front());
                }
            }
        }
    }

    // IR config
    QL_REQUIRE(modelCcys_.size() == 1,
               "ScriptedTradeEngineBuilder::buildFdGaussianCam(): only one ccy is supported, got "
                   << modelCcys_.size());

    auto config = QuantLib::ext::make_shared<IrLgmData>();
    config->qualifier() = getFirstIrIndexOrCcy(modelCcys_.front(), irIndices_);
    config->reversionType() = LgmData::ReversionType::HullWhite;
    config->volatilityType() = LgmData::VolatilityType::Hagan;
    config->calibrateH() = false;
    config->hParamType() = ParamType::Constant;
    config->hTimes() = std::vector<Real>();
    config->shiftHorizon() =
        modelCurves_.front()->timeFromReference(lastRelevantDate_) * 0.5; // TODO hardcode 0.5 here?
    config->scaling() = 1.0;
    std::string ccy = modelCcys_.front();
    if (zeroVolatility_ ) {
        DLOG("set up zero vol IrLgmData for currency '" << modelCcys_.front() << "'");
        // zero vol
        config->calibrationType() = CalibrationType::None;
        config->hValues() = {0.0};
        config->calibrateA() = false;
        config->aParamType() = ParamType::Constant;
        config->aTimes() = std::vector<Real>();
        config->aValues() = {0.0};
    } else {
        DLOG("set up IrLgmData for currency '" << modelCcys_.front() << "'");
        auto rev = irReversions_.find(modelCcys_.front());
        QL_REQUIRE(rev != irReversions_.end(), "reversion for ccy " << modelCcys_.front() << " not found");
        config->calibrationType() = CalibrationType::Bootstrap;
        config->hValues() = {rev->second};
        config->calibrateA() = true;
        config->aParamType() = ParamType::Piecewise;
        config->aTimes() = calibrationTimes;
        config->aValues() = std::vector<Real>(calibrationTimes.size() + 1, 0.0030); // start value for optimiser
        config->optionExpiries() = calibrationExpiries;
        config->optionTerms() = calibrationTerms;
        config->optionStrikes() = std::vector<std::string>(calibrationExpiries.size(), calibrationStrike);
    }

    std::string configurationInCcy = configuration(MarketContext::irCalibration);
    std::string configurationXois = configuration(MarketContext::pricing);

    auto camBuilder = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        market_,
        QuantLib::ext::make_shared<CrossAssetModelData>(
            std::vector<QuantLib::ext::shared_ptr<IrModelData>>{config}, std::vector<QuantLib::ext::shared_ptr<FxBsData>>{},
            std::vector<QuantLib::ext::shared_ptr<EqBsData>>{}, std::vector<QuantLib::ext::shared_ptr<InflationModelData>>{},
            std::vector<QuantLib::ext::shared_ptr<CrLgmData>>{}, std::vector<QuantLib::ext::shared_ptr<CrCirData>>{},
            std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>>{}, 0,
            std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>{}, bootstrapTolerance_, "LGM",
            CrossAssetModel::Discretization::Exact),
        configurationInCcy, configurationXois, configurationXois, configurationInCcy, configurationInCcy,
        configurationXois, !calibrate_ || zeroVolatility_, continueOnCalibrationError_, referenceCalibrationGrid_,
        SalvagingAlgorithm::Spectral, id);

    model_ = QuantLib::ext::make_shared<FdGaussianCam>(camBuilder->model(), modelCcys_.front(), modelCurves_.front(),
                                               modelIrIndices_, simulationDates_, modelSize_, timeStepsPerYear_,
                                               mesherEpsilon_, iborFallbackConfig);

    modelBuilders_.insert(std::make_pair(id, camBuilder));
}

void ScriptedTradeEngineBuilder::buildAMCCGModel(const std::string& id, const IborFallbackConfig& iborFallbackConfig,
                                                 const std::vector<std::string>& conditionalExpectationModelStates) {
    // nothing to build really, the resulting model is exactly the input model
    QL_REQUIRE(useCg_, "building gaussian cam from external amc cg model, useCg must be set to true in this case.");
    modelCG_ = amcCgModel_;
}

void ScriptedTradeEngineBuilder::buildGaussianCamAMC(
    const std::string& id, const IborFallbackConfig& iborFallbackConfig,
    const std::vector<std::string>& conditionalExpectationModelStates) {

    QL_REQUIRE(!useCg_, "building gaussian cam from external amc cam, useCg must be set to false in this case.");

    std::vector<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;

    // IR configs
    for (Size i = 0; i < modelCcys_.size(); ++i) {
        selectedComponents.push_back(
            std::make_pair(CrossAssetModel::AssetType::IR, amcCam_->ccyIndex(parseCurrency(modelCcys_[i]))));
    }

    // INF configs
    for (Size i = 0; i < modelInfIndices_.size(); ++i) {
        selectedComponents.push_back(std::make_pair(CrossAssetModel::AssetType::INF,
                                                    amcCam_->infIndex(IndexInfo(modelInfIndices_[i].first).infName())));
    }

    // FX configs
    for (Size i = 1; i < modelCcys_.size(); ++i) {
        selectedComponents.push_back(
            std::make_pair(CrossAssetModel::AssetType::FX, amcCam_->ccyIndex(parseCurrency(modelCcys_[i])) - 1));
    }

    // EQ configs
    for (auto const& eq : eqIndices_) {
        selectedComponents.push_back(std::make_pair(CrossAssetModel::AssetType::EQ, amcCam_->eqIndex(eq.eq()->name())));
    }

    // COMM configs, not supported at this point
    QL_REQUIRE(commIndices_.empty(), "GaussianCam model does not support commodity underlyings currently");

    std::vector<Size> projectedStateProcessIndices;
    Handle<CrossAssetModel> projectedModel(
        getProjectedCrossAssetModel(amcCam_, selectedComponents, projectedStateProcessIndices));

    // effective time steps per year: zero for exact evolution, otherwise the pricing engine parameter
    if (useCg_) {
        modelCG_ = QuantLib::ext::make_shared<GaussianCamCG>(
            projectedModel, modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_,
            modelIndices_, modelIndicesCurrencies_, simulationDates_,
            projectedModel->discretization() == CrossAssetModel::Discretization::Exact ? 0 : timeStepsPerYear_,
            iborFallbackConfig, projectedStateProcessIndices, conditionalExpectationModelStates);
    } else {
        model_ = QuantLib::ext::make_shared<GaussianCam>(
            projectedModel, modelSize_, modelCcys_, modelCurves_, modelFxSpots_, modelIrIndices_, modelInfIndices_,
            modelIndices_, modelIndicesCurrencies_, simulationDates_, mcParams_,
            projectedModel->discretization() == CrossAssetModel::Discretization::Exact ? 0 : timeStepsPerYear_,
            iborFallbackConfig, projectedStateProcessIndices, conditionalExpectationModelStates);
    }

    DLOG("built GuassianCam model as projection of xva evolution model");
    for (auto const& p : projectedStateProcessIndices)
        DLOG("  got projected state process index: " << p);
}

void ScriptedTradeEngineBuilder::addAmcGridToContext(QuantLib::ext::shared_ptr<Context>& context) const {
    // the amc grid might be empty, but we add the _AMC_SimDates variable to the context anyway, since
    // a script might rely on its existence
    DLOG("adding amc date grid (" << amcGrid_.size() << ") to context as _AMC_SimDates");
    std::vector<ValueType> tmp;
    for (auto const& d : amcGrid_)
        tmp.push_back(EventVec{modelSize_, d});
    context->arrays["_AMC_SimDates"] = tmp;
}

void ScriptedTradeEngineBuilder::setupCalibrationStrikes(const ScriptedTradeScriptData& script,
                                                         const QuantLib::ext::shared_ptr<Context>& context) {
    calibrationStrikes_ = getCalibrationStrikes(script.calibrationSpec(), context);
}

} // namespace data
} // namespace ore
