/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/engine/amcvaluationengine.hpp>

#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/methods/multipathvariategenerator.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <ored/portfolio/optionwrapper.hpp>

#include <boost/timer/timer.hpp>

using namespace ore::data;
using namespace ore::analytics;

namespace ore {
namespace analytics {

namespace {

Real fx(const std::vector<std::vector<std::vector<Real>>>& fxBuffer, const Size ccyIndex, const Size timeIndex,
        const Size sample) {
    if (ccyIndex == 0)
        return 1.0;
    return fxBuffer[ccyIndex - 1][timeIndex][sample];
}

Real state(const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex, const Size timeIndex,
           const Size sample) {
    return irStateBuffer[ccyIndex][timeIndex][sample];
}

Real numRatio(const boost::shared_ptr<CrossAssetModel>& model,
              const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex,
              const Size timeIndex, const Real time, const Size sample) {
    if (ccyIndex == 0)
        return 1.0;
    Real state_base = state(irStateBuffer, 0, timeIndex, sample);
    Real state_curr = state(irStateBuffer, ccyIndex, timeIndex, sample);
    return model->numeraire(ccyIndex, time, state_curr) / model->numeraire(0, time, state_base);
}

Real num(const boost::shared_ptr<CrossAssetModel>& model,
         const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex, const Size timeIndex,
         const Real time, const Size sample) {
    Real state_curr = state(irStateBuffer, ccyIndex, timeIndex, sample);
    return model->numeraire(ccyIndex, time, state_curr);
}

Array simulatePathInterface1(const boost::shared_ptr<AmcCalculatorSinglePath>& amcCalc, const MultiPath& path,
                             const bool reuseLastEvents, const std::string& tradeLabel, const Size sample) {
    try {
        return amcCalc->simulatePath(path, reuseLastEvents);
    } catch (const std::exception& e) {
        ALOG("error for trade '" << tradeLabel << "' sample #" << sample << ": " << e.what());
        return Array(path.pathSize(), 0.0);
    }
}

std::vector<QuantExt::RandomVariable>
simulatePathInterface2(const boost::shared_ptr<AmcCalculatorMultiVariates>& amcCalc, const std::vector<Real>& pathTimes,
                       std::vector<std::vector<RandomVariable>>& paths, const std::vector<bool>& isRelevantTime,
                       const bool moveStateToPreviousTime, const std::string& tradeLabel) {
    try {
        return amcCalc->simulatePath(pathTimes, paths, isRelevantTime, moveStateToPreviousTime);
    } catch (const std::exception& e) {
        ALOG("error for trade '" << tradeLabel << "': " << e.what());
        return std::vector<QuantExt::RandomVariable>(pathTimes.size() + 1,
                                                     RandomVariable(paths.front().front().size()));
    }
}

} // namespace

AMCValuationEngine::AMCValuationEngine(const boost::shared_ptr<QuantExt::CrossAssetModel>& model,
                                       const boost::shared_ptr<ScenarioGeneratorData>& sgd,
                                       const boost::shared_ptr<Market>& market,
                                       const std::vector<string>& aggDataIndices,
                                       const std::vector<string>& aggDataCurrencies)
    : model_(model), sgd_(sgd), market_(market), aggDataIndices_(aggDataIndices),
      aggDataCurrencies_(aggDataCurrencies) {

    QL_REQUIRE((aggDataIndices.empty() && aggDataCurrencies.empty()) || market != nullptr,
               "AMCValuationEngine: market is required for asd generation");
    QL_REQUIRE(sgd->seed() != 0,
               "AMCValuationEngine: path generation uses seed 0 - this might lead to inconsistent results to a classic "
               "simulation run, if both are combined. Consider using a non-zero seed.");
    QL_REQUIRE(
        model->irlgm1f(0)->termStructure()->dayCounter() == sgd->getGrid()->dayCounter(),
        "AMCValuationEngine: day counter in simulation parameters ("
            << sgd->getGrid()->dayCounter() << ") is different from model day counter ("
            << model->irlgm1f(0)->termStructure()->dayCounter()
            << "), align these e.g. by setting the day counter in the simulation parameters to the model day counter");
}

MultiPath AMCValuationEngine::effectiveSimulationPath(const MultiPath& p, const bool processCloseOutDates) const {
    QL_REQUIRE(sgd_->withCloseOutLag() && sgd_->withMporStickyDate(),
               "effectiveSimulationPath(): expected grid with close-out lag and sticky date mpor mode");
    MultiPath filteredPath(p.assetNumber(), sgd_->getGrid()->valuationTimeGrid());
    Size filteredTimeIndex = 0;
    for (Size i = 0; i < p.pathSize(); ++i) {
        if (i == 0 || (i > 0 && (processCloseOutDates ? sgd_->getGrid()->isCloseOutDate()[i - 1]
                                                      : sgd_->getGrid()->isValuationDate()[i - 1]))) {
            for (Size j = 0; j < p.assetNumber(); ++j) {
                filteredPath[j][filteredTimeIndex] = p[j][i];
            }
            ++filteredTimeIndex;
        }
    }
    return filteredPath;
}

void AMCValuationEngine::buildCube(const boost::shared_ptr<Portfolio>& portfolio,
                                   boost::shared_ptr<NPVCube>& outputCube) {

    LOG("Starting AMCValuationEngine for " << portfolio->size() << " trades, " << outputCube->samples()
                                           << " samples and " << sgd_->getGrid()->size() << " dates.");

    QL_REQUIRE(portfolio->size() > 0, "AMCValuationEngine::buildCube: empty portfolio");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == sgd_->getGrid()->valuationDates().size(),
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "different from number of valuation dates ("
                                    << sgd_->getGrid()->valuationDates().size() << ")");

    // base currency is the base currency of the cam

    Currency baseCurrency = model_->irlgm1f(0)->currency();

    // timings

    boost::timer::cpu_timer timer, timerTotal;
    Real calibrationTime = 0.0, valuationTime = 0.0, asdTime = 0.0, pathGenTime = 0.0, residualTime, totalTime;
    timerTotal.start();

    // prepare for asd writing

    std::vector<Size> asdCurrencyIndex; // FX Spots
    std::vector<string> asdCurrencyCode;
    std::vector<boost::shared_ptr<LgmImpliedYtsFwdFwdCorrected>> asdIndexCurve; // Ibor Indices
    std::vector<boost::shared_ptr<Index>> asdIndex;
    std::vector<Size> asdIndexIndex;
    std::vector<string> asdIndexName;
    if (asd_ != nullptr) {
        LOG("Collect information for aggregation scenario data...");
        // fx spots
        for (auto const& c : aggDataCurrencies_) {
            Currency cur = parseCurrency(c);
            if (cur == baseCurrency)
                continue;
            Size ccyIndex = model_->ccyIndex(cur);
            asdCurrencyIndex.push_back(ccyIndex);
            asdCurrencyCode.push_back(c);
        }
        // ibor indices
        for (auto const& i : aggDataIndices_) {
            boost::shared_ptr<IborIndex> tmp;
            try {
                tmp = *market_->iborIndex(i);
            } catch (const std::exception& e) {
                ALOG("index \"" << i << "\" not found in market, skipping. (" << e.what() << ")");
            }
            Size ccyIndex = model_->ccyIndex(tmp->currency());
            asdIndexCurve.push_back(boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_->lgm(ccyIndex),
                                                                                     tmp->forwardingTermStructure()));
            asdIndex.push_back(tmp->clone(Handle<YieldTermStructure>(asdIndexCurve.back())));
            asdIndexIndex.push_back(ccyIndex);
            asdIndexName.push_back(i);
        }
    } else {
        LOG("No asd object set, won't write aggregation scenario data...");
    }

    // extract AMC calculators and additional things we need from the ore wrapper

    LOG("Extract AMC Calculators...");
    std::vector<boost::shared_ptr<AmcCalculator>> amcCalculators;
    std::vector<Size> tradeId;
    std::vector<std::string> tradeLabel;
    std::vector<Real> effectiveMultiplier;
    std::vector<Size> currencyIndex;
    timer.start();
    for (Size i = 0; i < portfolio->size(); ++i) {
        boost::shared_ptr<AmcCalculator> amcCalc;
        const boost::shared_ptr<Trade>& t = portfolio->trades()[i];
        try {
            amcCalc = t->instrument()->qlInstrument(true)->result<boost::shared_ptr<AmcCalculator>>("amcCalculator");
            LOG("AMCCalculator extracted for \"" << t->id() << "\"");
        } catch (const std::exception& e) {
            WLOG("could not extract AMCCalculator for trade \"" << t->id() << "\": " << e.what());
        }
        if (amcCalc != nullptr) {
            amcCalculators.push_back(amcCalc);
            Real effMult = t->instrument()->multiplier();
            auto ow = boost::dynamic_pointer_cast<OptionWrapper>(t->instrument());
            if (ow != nullptr) {
                effMult *= ow->isLong() ? 1.0 : -1.0;
                // we can ignore the underlying multiplier, since this is not involved in the AMC engine
            }
            effectiveMultiplier.push_back(effMult);
            currencyIndex.push_back(model_->ccyIndex(amcCalc->npvCurrency()));
            tradeId.push_back(i);
            tradeLabel.push_back(t->id());
            // TODO additional instruments (fees)
        }
        updateProgress(i, portfolio->size());
    }
    timer.stop();
    calibrationTime += timer.elapsed().wall * 1e-9;
    LOG("Extracted " << amcCalculators.size() << " AMCCalculators for " << portfolio->size() << " source trades");

    // run the simulation and populate the cube with NPVs, and write aggregation scenario data

    auto process = model_->stateProcess();

    /* set up buffers for fx rates and ir states that we need below for the runs against interface 1 and 2
       we set these buffers up on the full grid (i.e. valuation + close-out dates, also including the T0 date)
     */

    std::vector<std::vector<std::vector<Real>>> fxBuffer(
        model_->components(CrossAssetModel::AssetType::FX),
        std::vector<std::vector<Real>>(sgd_->getGrid()->dates().size() + 1, std::vector<Real>(outputCube->samples())));
    std::vector<std::vector<std::vector<Real>>> irStateBuffer(
        model_->components(CrossAssetModel::AssetType::IR),
        std::vector<std::vector<Real>>(sgd_->getGrid()->dates().size() + 1, std::vector<Real>(outputCube->samples())));

    /* set up cache for paths, this is used in interface 2 below */

    Size nStates = process->size();
    QL_REQUIRE(sgd_->getGrid()->timeGrid().size() > 0, "AMCValuationEngine: empty time grid given");
    std::vector<Real> pathTimes(std::next(sgd_->getGrid()->timeGrid().begin(), 1), sgd_->getGrid()->timeGrid().end());
    std::vector<std::vector<RandomVariable>> paths(
        pathTimes.size(), std::vector<RandomVariable>(nStates, RandomVariable(outputCube->samples())));

    // Run AmcCalculators implementing interface 1, write ASD and fill fxBuffer / irStateBuffer

    // FIXME hardcoded ordering and directionIntegers here...
    auto pathGenerator =
        makeMultiPathGenerator(sgd_->sequenceType(), process, sgd_->getGrid()->timeGrid(), sgd_->seed());
    LOG("Run simulation (amc calculators implementing interface 1, write ASD, fill internal fx and irState "
        "buffers)...");
    resetProgress();
    for (Size i = 0; i < outputCube->samples(); ++i) {
        timer.start();
        const MultiPath& path = pathGenerator->next().value;
        timer.stop();
        pathGenTime += timer.elapsed().wall * 1e-9;

        // populate fx and ir state buffers, populate cached paths for interface 2

        for (Size k = 0; k < fxBuffer.size(); ++k) {
            for (Size j = 0; j < sgd_->getGrid()->timeGrid().size(); ++j) {
                fxBuffer[k][j][i] = std::exp(path[model_->pIdx(CrossAssetModel::AssetType::FX, k)][j]);
            }
        }
        for (Size k = 0; k < irStateBuffer.size(); ++k) {
            for (Size j = 0; j < sgd_->getGrid()->timeGrid().size(); ++j) {
                irStateBuffer[k][j][i] = path[model_->pIdx(CrossAssetModel::AssetType::IR, k)][j];
            }
        }

        // TODO we do not need this if there are no amc calculators implementing interface 2
        for (Size k = 0; k < nStates; ++k) {
            for (Size j = 0; j < pathTimes.size(); ++j) {
                paths[j][k].set(i, path[k][j]);
            }
        }

        // amc valuation and output to cube

        timer.start();
        for (Size j = 0; j < amcCalculators.size(); ++j) {
            auto amcCalc = boost::dynamic_pointer_cast<AmcCalculatorSinglePath>(amcCalculators[j]);
            if (amcCalc == nullptr)
                continue;

            if (!sgd_->withCloseOutLag()) {
                // no close-out lag, fill depth 0 of cube with npvs on path
                Array res = simulatePathInterface1(amcCalc, path, false, tradeLabel[j], i);
                outputCube->setT0(res[0] * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                      numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                      effectiveMultiplier[j],
                                  tradeId[j], 0);
                int dateIndex = -1;
                for (Size k = 1; k < res.size(); ++k) {
                    ++dateIndex;
                    Real t = sgd_->getGrid()->timeGrid()[k];
                    outputCube->set(res[k] * fx(fxBuffer, currencyIndex[j], k, i) *
                                        numRatio(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                        effectiveMultiplier[j],
                                    tradeId[j], dateIndex, i, 0);
                }
            } else {
                // with close-out lag, fille depth 0 with valuation date npvs, depth 1 with (inflated) close-out npvs
                if (sgd_->withMporStickyDate()) {
                    // stikcy date mpor mode, simulate the valuation times and close out times separately
                    Array res =
                        simulatePathInterface1(amcCalc, effectiveSimulationPath(path, false), false, tradeLabel[j], i);
                    Array resCout =
                        simulatePathInterface1(amcCalc, effectiveSimulationPath(path, true), true, tradeLabel[j], i);
                    outputCube->setT0(res[0] * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                          numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                          effectiveMultiplier[j],
                                      tradeId[j], 0);
                    int dateIndex = -1;
                    for (Size k = 0; k < sgd_->getGrid()->dates().size(); ++k) {
                        Real t = sgd_->getGrid()->timeGrid()[k + 1];
                        Real tm = sgd_->getGrid()->timeGrid()[k];
                        if (sgd_->getGrid()->isCloseOutDate()[k]) {
                            QL_REQUIRE(dateIndex >= 0, "first date in grid must be a valuation date");
                            outputCube->set(resCout[dateIndex + 1] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                                num(model_, irStateBuffer, currencyIndex[j], k + 1, tm, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 1);
                        }
                        if (sgd_->getGrid()->isValuationDate()[k]) {
                            dateIndex++;
                            outputCube->set(res[dateIndex + 1] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                                numRatio(model_, irStateBuffer, currencyIndex[j], k + 1, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                } else {
                    // actual date mport mode: simulate all times in one go
                    Array res = simulatePathInterface1(amcCalc, path, false, tradeLabel[j], i);
                    outputCube->setT0(res[0] * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                          numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                          effectiveMultiplier[j],
                                      tradeId[j], 0);
                    int dateIndex = -1;
                    for (Size k = 1; k < res.size(); ++k) {
                        Real t = sgd_->getGrid()->timeGrid()[k];
                        if (sgd_->getGrid()->isCloseOutDate()[k - 1]) {
                            QL_REQUIRE(dateIndex >= 0, "first date in grid must be a valuation date");
                            outputCube->set(res[k] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                num(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 1);
                        }
                        if (sgd_->getGrid()->isValuationDate()[k - 1]) {
                            dateIndex++;
                            outputCube->set(res[k] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                numRatio(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                }
            }
        }
        timer.stop();
        valuationTime += timer.elapsed().wall * 1e-9;

        // write aggregation scenario data, TODO this seems relatively slow, can we speed it up using LgmVectorised
        // etc.?

        if (asd_ != nullptr) {
            timer.start();
            Size dateIndex = 0;
            for (Size k = 1; k < sgd_->getGrid()->timeGrid().size(); ++k) {
                // only write asd on valuation dates
                if (!sgd_->getGrid()->isValuationDate()[k - 1])
                    continue;
                // set numeraire
                asd_->set(dateIndex, i, model_->numeraire(0, path[0].time(k), path[0][k]),
                          AggregationScenarioDataType::Numeraire);
                // set fx spots
                for (Size j = 0; j < asdCurrencyIndex.size(); ++j) {
                    asd_->set(dateIndex, i, fx(fxBuffer, asdCurrencyIndex[j], k, i),
                              AggregationScenarioDataType::FXSpot, asdCurrencyCode[j]);
                }
                // set index fixings
                Date d = sgd_->getGrid()->dates()[k - 1];
                for (Size j = 0; j < asdIndex.size(); ++j) {
                    asdIndexCurve[j]->move(d, state(irStateBuffer, asdIndexIndex[j], k, i));
                    asd_->set(dateIndex, i, asdIndex[j]->fixing(d), AggregationScenarioDataType::IndexFixing,
                              asdIndexName[j]);
                }
                ++dateIndex;
            }
            timer.stop();
            asdTime += timer.elapsed().wall * 1e-9;
        }
        updateProgress(i + 1, outputCube->samples());
    }

    // Run AmcCalculators implementing interface 2

    LOG("Run simulation (amc calculators implementing interface 2)...");
    resetProgress();

    // set up vectors indicating valuation times, close-out times and all times

    std::vector<bool> allTimes(pathTimes.size(), true);
    std::vector<bool> valuationTimes(pathTimes.size()), closeOutTimes(pathTimes.size());
    for (Size i = 0; i < pathTimes.size(); ++i) {
        valuationTimes[i] = sgd_->getGrid()->isValuationDate()[i];
        closeOutTimes[i] = sgd_->getGrid()->isCloseOutDate()[i];
    }

    // loop over amc calculators, get result and populate cube

    timer.start();
    for (Size j = 0; j < amcCalculators.size(); ++j) {
        auto amcCalc = boost::dynamic_pointer_cast<AmcCalculatorMultiVariates>(amcCalculators[j]);
        if (amcCalc == nullptr)
            continue;
        if (!sgd_->withCloseOutLag()) {
            // no close-out lag, fill depth 0 with npv on path
            auto res = simulatePathInterface2(amcCalc, pathTimes, paths, allTimes, false, tradeLabel[j]);
            outputCube->setT0(res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                  numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) * effectiveMultiplier[j],
                              tradeId[j], 0);
            for (Size k = 1; k < res.size(); ++k) {
                Real t = sgd_->getGrid()->timeGrid()[k];
                for (Size i = 0; i < outputCube->samples(); ++i) {
                    outputCube->set(res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                        numRatio(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                        effectiveMultiplier[j],
                                    tradeId[j], k - 1, i, 0);
                }
            }
        } else {
            // with close-out lag, fill depth 0 with valuation date npvs, depth 1 with (inflated) close-out npvs
            if (sgd_->withMporStickyDate()) {
                // sticky date mpor mode. simulate the valuation times...
                auto res = simulatePathInterface2(amcCalc, pathTimes, paths, valuationTimes, false, tradeLabel[j]);
                // ... and then the close-out times, but times moved to the valuation times
                auto resLag = simulatePathInterface2(amcCalc, pathTimes, paths, closeOutTimes, true, tradeLabel[j]);
                outputCube->setT0(res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                      numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                      effectiveMultiplier[j],
                                  tradeId[j], 0);
                int dateIndex = -1;
                for (Size k = 0; k < sgd_->getGrid()->dates().size(); ++k) {
                    Real t = sgd_->getGrid()->timeGrid()[k + 1];
                    Real tm = sgd_->getGrid()->timeGrid()[k];
                    if (sgd_->getGrid()->isCloseOutDate()[k]) {
                        QL_REQUIRE(dateIndex >= 0, "first date in grid must be a valuation date");
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            outputCube->set(resLag[dateIndex + 1][i] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                                num(model_, irStateBuffer, currencyIndex[j], k + 1, tm, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 1);
                        }
                    }
                    if (sgd_->getGrid()->isValuationDate()[k]) {
                        ++dateIndex;
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            outputCube->set(res[dateIndex + 1][i] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                                numRatio(model_, irStateBuffer, currencyIndex[j], k + 1, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                }
            } else {
                // actual date mpor mode: simulate all times in one go
                auto res = simulatePathInterface2(amcCalc, pathTimes, paths, allTimes, false, tradeLabel[j]);
                outputCube->setT0(res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                      numRatio(model_, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                      effectiveMultiplier[j],
                                  tradeId[j], 0);
                int dateIndex = -1;
                for (Size k = 1; k < res.size(); ++k) {
                    Real t = sgd_->getGrid()->timeGrid()[k];
                    if (sgd_->getGrid()->isCloseOutDate()[k - 1]) {
                        QL_REQUIRE(dateIndex >= 0, "first date in grid must be a valuation date");
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            outputCube->set(res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                num(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 1);
                        }
                    }
                    if (sgd_->getGrid()->isValuationDate()[k - 1]) {
                        ++dateIndex;
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            outputCube->set(res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                numRatio(model_, irStateBuffer, currencyIndex[j], k, t, i) *
                                                effectiveMultiplier[j],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                }
            }
        }
        updateProgress(j + 1, amcCalculators.size());
    }
    timer.stop();
    valuationTime += timer.elapsed().wall * 1e-9;

    totalTime = timerTotal.elapsed().wall * 1e-9;
    residualTime = totalTime - (calibrationTime + pathGenTime + valuationTime + asdTime);
    LOG("calibration time     : " << calibrationTime << " sec");
    LOG("path generation time : " << pathGenTime << " sec");
    LOG("valuation time       : " << valuationTime << " sec");
    LOG("asd time             : " << asdTime << " sec");
    LOG("residual time        : " << residualTime << " sec");
    LOG("total time           : " << totalTime << " sec");
    LOG("AMCValuationEngine finished");
}

} // namespace analytics
} // namespace ore
