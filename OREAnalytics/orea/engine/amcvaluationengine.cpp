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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/observationmode.hpp>

#include <ored/marketdata/clonedloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/methods/multipathvariategenerator.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <ql/instruments/compositeinstrument.hpp>

#include <ored/portfolio/optionwrapper.hpp>

#include <boost/timer/timer.hpp>

#include <future>

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

Real numRatio(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
              const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex,
              const Size timeIndex, const Real time, const Size sample) {
    if (ccyIndex == 0)
        return 1.0;
    Real state_base = state(irStateBuffer, 0, timeIndex, sample);
    Real state_curr = state(irStateBuffer, ccyIndex, timeIndex, sample);
    return model->numeraire(ccyIndex, time, state_curr) / model->numeraire(0, time, state_base);
}

Real num(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
         const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex, const Size timeIndex,
         const Real time, const Size sample) {
    Real state_curr = state(irStateBuffer, ccyIndex, timeIndex, sample);
    return model->numeraire(ccyIndex, time, state_curr);
}

Real discount(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
              const std::vector<std::vector<std::vector<Real>>>& irStateBuffer, const Size ccyIndex,
              const Size timeIndex, const Real t, const Real T, const Size sample) {
    Real state_curr = state(irStateBuffer, ccyIndex, timeIndex, sample);
    return model->discountBond(ccyIndex, t, T, state_curr);
}

std::vector<QuantExt::RandomVariable>
simulatePathInterface2(const QuantLib::ext::shared_ptr<AmcCalculator>& amcCalc, const std::vector<Real>& pathTimes,
                       std::vector<std::vector<RandomVariable>>& paths, const std::vector<size_t>& pathIdx,
                       const std::vector<size_t>& timeIdx,
                       const std::string& tradeLabel,
                       const std::string& tradeType) {
    QL_REQUIRE(pathIdx.size() == timeIdx.size(),
               "internal error, mismatch between relevant path idx and timegrid idx, please contact dev");
    try {
        return amcCalc->simulatePath(pathTimes, paths, pathIdx, timeIdx);
    } catch (const std::exception& e) {
        StructuredTradeErrorMessage(tradeLabel, tradeType, "error during amc path simulation for trade.", e.what())
            .log();
        return std::vector<QuantExt::RandomVariable>(pathIdx.size() + 1,
                                                     RandomVariable(paths.front().front().size()));
    }
}

std::vector<QuantExt::RandomVariable>
feeContributions(const Size j, const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& sgd, const Date& asof,
                 const Size samples, const std::vector<std::vector<std::tuple<Size, Real, QuantLib::Date>>>& tradeFees,
                 const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                 const std::vector<std::vector<std::vector<Real>>>& fxBuffer,
                 const std::vector<std::vector<std::vector<Real>>>& irStateBuffer) {
    std::vector<QuantExt::RandomVariable> result;
    for (Size k = 0; k < sgd->getGrid()->timeGrid().size(); ++k) {
        Date simDate = k == 0 ? asof : sgd->getGrid()->dates()[k - 1];
        // slight approximation: we treat premiums as seen from the closeout date the same as if priced from the
        // the valuation date in sticky date mode with mpor grid
        if (k == 0 || !sgd->withCloseOutLag() || !sgd->withMporStickyDate() ||
            sgd->getGrid()->isValuationDate()[k - 1]) {
            result.push_back(RandomVariable(samples, 0.0));
            if (tradeFees[j].empty())
                continue;
            for (Size i = 0; i < samples; ++i) {
                Real tmp = 0.0;
                for (Size f = 0; f < tradeFees[j].size(); ++f) {
                    if (std::get<2>(tradeFees[j][f]) > simDate) {
                        Real t = sgd->getGrid()->timeGrid()[k];
                        Real T = model->irModel(0)->termStructure()->timeFromReference(std::get<2>(tradeFees[j][f]));
                        tmp += std::get<1>(tradeFees[j][f]) * fx(fxBuffer, std::get<0>(tradeFees[j][f]), k, i) *
                               discount(model, irStateBuffer, std::get<0>(tradeFees[j][f]), k, t, T, i) *
                               num(model, irStateBuffer, 0, k, t, i);
                    }
                }
                result.back().set(i, tmp);
            }
        }
    }
    return result;
}

void runCoreEngine(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& model,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                   const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGeneratorData>& sgd,
                   const std::vector<string>& aggDataIndices, const std::vector<string>& aggDataCurrencies,
                   const Size aggDataNumberCreditStates, QuantLib::ext::shared_ptr<ore::analytics::AggregationScenarioData> asd,
                   QuantLib::ext::shared_ptr<NPVCube> outputCube, QuantLib::ext::shared_ptr<ProgressIndicator> progressIndicator) {

    std::ostringstream detail;
    detail << portfolio->size() << " trade" << (portfolio->size() == 1 ? "" : "s");
    progressIndicator->updateProgress(0, portfolio->size(), detail.str());

    // base currency is the base currency of the cam

    Currency baseCurrency = model->irlgm1f(0)->currency();

    // timings

    boost::timer::cpu_timer timer, timerTotal;
    Real calibrationTime = 0.0, valuationTime = 0.0, asdTime = 0.0, bufferTime = 0.0, pathGenTime = 0.0, residualTime,
         totalTime;
    timerTotal.start();

    // prepare for asd writing

    std::vector<Size> asdCurrencyIndex; // FX Spots
    std::vector<string> asdCurrencyCode;
    std::vector<QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected>> asdIndexCurve; // Ibor Indices
    std::vector<QuantLib::ext::shared_ptr<Index>> asdIndex;
    std::vector<Size> asdIndexIndex;
    std::vector<string> asdIndexName;
    if (asd != nullptr) {
        LOG("Collect information for aggregation scenario data...");
        // fx spots
        for (auto const& c : aggDataCurrencies) {
            Currency cur = parseCurrency(c);
            if (cur == baseCurrency)
                continue;
            Size ccyIndex = model->ccyIndex(cur);
            asdCurrencyIndex.push_back(ccyIndex);
            asdCurrencyCode.push_back(c);
        }
        // ibor indices
        for (auto const& i : aggDataIndices) {
            QuantLib::ext::shared_ptr<IborIndex> tmp;
            try {
                tmp = *market->iborIndex(i);
            } catch (const std::exception& e) {
                ALOG("index \"" << i << "\" not found in market, skipping. (" << e.what() << ")");
            }
            Size ccyIndex = model->ccyIndex(tmp->currency());
            asdIndexCurve.push_back(
                QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(model->lgm(ccyIndex), tmp->forwardingTermStructure()));
            asdIndex.push_back(tmp->clone(Handle<YieldTermStructure>(asdIndexCurve.back())));
            asdIndexIndex.push_back(ccyIndex);
            asdIndexName.push_back(i);
        }
    } else {
        LOG("No asd object set, won't write aggregation scenario data...");
    }

    // extract AMC calculators, fees and some other infos we need from the ore wrapper

    LOG("Extract AMC Calculators...");
    std::vector<QuantLib::ext::shared_ptr<AmcCalculator>> amcCalculators;
    std::vector<Size> tradeId;
    std::vector<std::string> tradeLabel, tradeType;
    std::vector<Real> effectiveMultiplier;
    std::vector<Size> currencyIndex;
    std::vector<std::vector<std::tuple<Size, Real, QuantLib::Date>>> tradeFees;
    timer.start();
    Size progressCounter = 0;

    // reset timing stats
    RandomVariableStats::instance().enabled = true;
    RandomVariableStats::instance().data_ops = 0;
    RandomVariableStats::instance().calc_ops = 0;
    RandomVariableStats::instance().data_timer.start();
    RandomVariableStats::instance().data_timer.stop();
    RandomVariableStats::instance().calc_timer.start();
    RandomVariableStats::instance().calc_timer.stop();
    McEngineStats::instance().other_timer.start();
    McEngineStats::instance().other_timer.stop();
    McEngineStats::instance().path_timer.start();
    McEngineStats::instance().path_timer.stop();
    McEngineStats::instance().calc_timer.start();
    McEngineStats::instance().calc_timer.stop();

    auto extractAmcCalculator = [&amcCalculators, &tradeId, &tradeLabel, &tradeType, &effectiveMultiplier,
                                 &currencyIndex, &tradeFees, &model,
                                 &outputCube](const std::pair<std::string, QuantLib::ext::shared_ptr<Trade>>& trade,
                                              QuantLib::ext::shared_ptr<AmcCalculator> amcCalc, Real multiplier, bool addFees) {
        LOG("AMCCalculator extracted for \"" << trade.first << "\"");
        amcCalculators.push_back(amcCalc);
        effectiveMultiplier.push_back(multiplier);
        currencyIndex.push_back(model->ccyIndex(amcCalc->npvCurrency()));
        if (auto id = outputCube->idsAndIndexes().find(trade.first); id != outputCube->idsAndIndexes().end()) {
            tradeId.push_back(id->second);
        } else {
            QL_FAIL("AMCValuationEngine: trade id '" << trade.first
                                                     << "' is not present in output cube - internal error.");
        }
        tradeLabel.push_back(trade.first);
        tradeType.push_back(trade.second->tradeType());
        tradeFees.push_back({});
        if (addFees) {
            for (Size i = 0; i < trade.second->instrument()->additionalInstruments().size(); ++i) {
                if (auto p = QuantLib::ext::dynamic_pointer_cast<QuantExt::Payment>(
                        trade.second->instrument()->additionalInstruments()[i])) {
                    tradeFees.back().push_back(std::make_tuple(model->ccyIndex(p->currency()), p->cashFlow()->amount(),
                                                               p->cashFlow()->date()));
                } else {
                    StructuredTradeErrorMessage(trade.second, "Additional instrument is ignored in AMC simulation",
                                                "only QuantExt::Payment is handled as additional instrument.")
                        .log();
                }
            }
        }
    };

    for (auto const& trade : portfolio->trades()) {
        QuantLib::ext::shared_ptr<AmcCalculator> amcCalc;
        try {
            auto inst = trade.second->instrument()->qlInstrument(true);
            QL_REQUIRE(inst != nullptr,
                       "instrument has no ql instrument, this is not supported by the amc valuation engine.");
            Real multiplier = trade.second->instrument()->multiplier() * trade.second->instrument()->multiplier2();

            // handle composite trades
            if (auto cInst = QuantLib::ext::dynamic_pointer_cast<CompositeInstrument>(inst)) {
                auto addResults = cInst->additionalResults();
                std::vector<Real> multipliers;
                while (true) {
                    std::stringstream ss;
                    ss << multipliers.size() + 1 << "_multiplier";
                    if (addResults.find(ss.str()) == addResults.end())
                        break;
                    multipliers.push_back(inst->result<Real>(ss.str()));
                }
                std::vector<QuantLib::ext::shared_ptr<AmcCalculator>> amcCalcs;
                for (Size cmpIdx = 0; cmpIdx < multipliers.size(); ++cmpIdx) {
                    std::stringstream ss;
                    ss << cmpIdx + 1 << "_amcCalculator";
                    if (addResults.find(ss.str()) != addResults.end()) {
                        amcCalcs.push_back(inst->result<QuantLib::ext::shared_ptr<AmcCalculator>>(ss.str()));
                    }
                }
                QL_REQUIRE(amcCalcs.size() == multipliers.size(),
                           "Did not find amc calculators for all components of composite trade.");
                for (Size cmpIdx = 0; cmpIdx < multipliers.size(); ++cmpIdx) {
                    extractAmcCalculator(trade, amcCalc, multiplier * multipliers[cmpIdx], cmpIdx == 0);
                }
                continue;
            }

            // handle non-composite trades
            amcCalc = inst->result<QuantLib::ext::shared_ptr<AmcCalculator>>("amcCalculator");
            extractAmcCalculator(trade, amcCalc, multiplier, true);

        } catch (const std::exception& e) {
            StructuredTradeErrorMessage(trade.second, "Error building trade for AMC simulation", e.what()).log();
        }
    }

    timer.stop();
    calibrationTime += timer.elapsed().wall * 1e-9;
    LOG("Extracted " << amcCalculators.size() << " AMCCalculators for " << portfolio->size() << " source trades");

    // set up buffers for fx rates and ir states that we need below for the runs against interface 1 and 2
    // we set these buffers up on the full grid (i.e. valuation + close-out dates, also including the T0 date)

    std::vector<std::vector<std::vector<Real>>> fxBuffer(
        model->components(CrossAssetModel::AssetType::FX),
        std::vector<std::vector<Real>>(sgd->getGrid()->dates().size() + 1, std::vector<Real>(outputCube->samples())));
    std::vector<std::vector<std::vector<Real>>> irStateBuffer(
        model->components(CrossAssetModel::AssetType::IR),
        std::vector<std::vector<Real>>(sgd->getGrid()->dates().size() + 1, std::vector<Real>(outputCube->samples())));

    // set up cache for paths

    auto process = model->stateProcess();
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        tmp->resetCache(sgd->getGrid()->timeGrid().size() - 1);
    }
    Size nStates = process->size();
    QL_REQUIRE(sgd->getGrid()->timeGrid().size() > 0, "AMCValuationEngine: empty time grid given");
    std::vector<Real> pathTimes(std::next(sgd->getGrid()->timeGrid().begin(), 1), sgd->getGrid()->timeGrid().end());
    std::vector<std::vector<RandomVariable>> paths(
        pathTimes.size(), std::vector<RandomVariable>(nStates, RandomVariable(outputCube->samples())));

    // fill fx buffer, ir state buffer and write ASD

    auto pathGenerator = makeMultiPathGenerator(sgd->sequenceType(), process, sgd->getGrid()->timeGrid(), sgd->seed(),
                                                sgd->ordering(), sgd->directionIntegers());

    LOG("Write ASD, fill internal fx and irState buffers...");

    for (Size i = 0; i < outputCube->samples(); ++i) {
        timer.start();
        const auto& path = pathGenerator->next().value;
        timer.stop();
        pathGenTime += timer.elapsed().wall * 1e-9;

        // populate fx and ir state buffers, populate cached paths for interface 2

        timer.start();
        for (Size k = 0; k < fxBuffer.size(); ++k) {
            for (Size j = 0; j < sgd->getGrid()->timeGrid().size(); ++j) {
                fxBuffer[k][j][i] = std::exp(path[model->pIdx(CrossAssetModel::AssetType::FX, k)][j]);
            }
        }
        for (Size k = 0; k < irStateBuffer.size(); ++k) {
            for (Size j = 0; j < sgd->getGrid()->timeGrid().size(); ++j) {
                irStateBuffer[k][j][i] = path[model->pIdx(CrossAssetModel::AssetType::IR, k)][j];
            }
        }

        for (Size k = 0; k < nStates; ++k) {
            for (Size j = 0; j < pathTimes.size(); ++j) {
                paths[j][k].set(i, path[k][j + 1]);
            }
        }
        timer.stop();
        bufferTime += timer.elapsed().wall * 1e-9;

        // write aggregation scenario data, TODO this seems relatively slow, can we speed it up using LgmVectorised

        if (asd != nullptr) {
            timer.start();
            Size dateIndex = 0;
            for (Size k = 1; k < sgd->getGrid()->timeGrid().size(); ++k) {
                // only write asd on valuation dates
                if (!sgd->getGrid()->isValuationDate()[k - 1])
                    continue;
                // set numeraire
                asd->set(dateIndex, i, model->numeraire(0, path[0].time(k), path[0][k]),
                         AggregationScenarioDataType::Numeraire);
                // set fx spots
                for (Size j = 0; j < asdCurrencyIndex.size(); ++j) {
                    asd->set(dateIndex, i, fx(fxBuffer, asdCurrencyIndex[j], k, i), AggregationScenarioDataType::FXSpot,
                             asdCurrencyCode[j]);
                }
                // set index fixings
                Date d = sgd->getGrid()->dates()[k - 1];
                for (Size j = 0; j < asdIndex.size(); ++j) {
                    asdIndexCurve[j]->move(d, state(irStateBuffer, asdIndexIndex[j], k, i));
                    auto index = asdIndex[j];
                    if (auto fb = QuantLib::ext::dynamic_pointer_cast<FallbackIborIndex>(asdIndex[j])) {
                        // proxy fallback ibor index by its rfr index's fixing
                        index = fb->rfrIndex();
                    }
                    asd->set(dateIndex, i, index->fixing(index->fixingCalendar().adjust(d)),
                             AggregationScenarioDataType::IndexFixing, asdIndexName[j]);
                }
                // set credit states
                for (Size j = 0; j < aggDataNumberCreditStates; ++j) {
                    asd->set(dateIndex, i, path[model->pIdx(CrossAssetModel::AssetType::CrState, j)][k],
                             AggregationScenarioDataType::CreditState, std::to_string(j));
                }
                ++dateIndex;
            }
            timer.stop();
            asdTime += timer.elapsed().wall * 1e-9;
        }
    }

    // Run AmcCalculators

    LOG("Run simulation...");
    // set up vectors indicating valuation times, close-out times and all times

    std::vector<size_t> allTimes;
    std::vector<size_t> valuationTimeIdx, closeOutTimeIdx;
    const auto& grid = sgd->getGrid();
    const auto& dates = grid->dates();
    size_t j = 0;
    for (size_t i = 0; i < pathTimes.size(); ++i) {
        allTimes.push_back(i);
        if (sgd->withCloseOutLag()) {
            auto& d = dates[i];
            if (grid->isValuationDate()[i]) {
                valuationTimeIdx.push_back(i);
                auto closeOutDate = grid->closeOutDateFromValuationDate(d);
                while (j < pathTimes.size() && dates[j] != closeOutDate) {
                    ++j;
                }
                QL_REQUIRE(j < pathTimes.size(),
                           "AmcValuationEngine:: couldnt find close out date" << to_string(closeOutDate));
                closeOutTimeIdx.push_back(j);
            }
        }
    }
    // loop over amc calculators, get result and populate cube

    timer.start();
    for (Size j = 0; j < amcCalculators.size(); ++j) {
        auto resFee = feeContributions(j, sgd, model->irModel(0)->termStructure()->referenceDate(),
                                       outputCube->samples(), tradeFees, model, fxBuffer, irStateBuffer);

        if (!sgd->withCloseOutLag()) {
            // no close-out lag, fill depth 0 with npv on path
            auto res = simulatePathInterface2(amcCalculators[j], pathTimes, paths, allTimes, allTimes, tradeLabel[j],
                                              tradeType[j]);
            Real v = outputCube->getT0(tradeId[j], 0);
            outputCube->setT0(v +
                                  res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                      numRatio(model, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                      effectiveMultiplier[j] +
                                  resFee[0][0],
                              tradeId[j], 0);
            for (Size k = 1; k < res.size(); ++k) {
                Real t = sgd->getGrid()->timeGrid()[k];
                for (Size i = 0; i < outputCube->samples(); ++i) {
                    Real v = outputCube->get(tradeId[j], k - 1, i, 0);
                    outputCube->set(v +
                                        res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                            numRatio(model, irStateBuffer, currencyIndex[j], k, t, i) *
                                            effectiveMultiplier[j] +
                                        resFee[k][i],
                                    tradeId[j], k - 1, i, 0);
                }
            }
        } else {
            // with close-out lag, fill depth 0 with valuation date npvs, depth 1 with (inflated) close-out npvs
            if (sgd->withMporStickyDate()) {
                // sticky date mpor mode. simulate the valuation times...
                auto res = simulatePathInterface2(amcCalculators[j], pathTimes, paths, valuationTimeIdx,
                                                  valuationTimeIdx, tradeLabel[j], tradeType[j]);
                // ... and then the close-out times, but times moved to the valuation times
                auto resLag = simulatePathInterface2(amcCalculators[j], pathTimes, paths, closeOutTimeIdx,
                                                     valuationTimeIdx, tradeLabel[j], tradeType[j]);
                Real v = outputCube->getT0(tradeId[j], 0);
                outputCube->setT0(v +
                                      res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                          numRatio(model, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                          effectiveMultiplier[j] +
                                      resFee[0][0],
                                  tradeId[j], 0);
                int dateIndex = -1;
                std::map<QuantLib::Date, std::vector<std::tuple<QuantLib::Date, double, size_t>>> closeOutDateToValuationDate;
                for (Size k = 0; k < sgd->getGrid()->dates().size(); ++k) {
                     
                    Real t = sgd->getGrid()->timeGrid()[k + 1];
                    if (sgd->getGrid()->isCloseOutDate()[k]) {
                        Date closeOutDate = sgd->getGrid()->dates()[k];
                        auto dateIndexIt = closeOutDateToValuationDate.find(closeOutDate);
                        QL_REQUIRE(dateIndexIt != closeOutDateToValuationDate.end() && !dateIndexIt->second.empty(), 
                                    "The valuation date needs to before the corresponding close out date");
                        for (const auto& [valuationDate, valuationTime, valuationIndex] : dateIndexIt->second){
                            for (Size i = 0; i < outputCube->samples(); ++i) {
                                Real v = outputCube->get(tradeId[j], valuationIndex, i, 1);
                                outputCube->set(
                                    v +
                                        resLag[valuationIndex + 1][i] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                            num(model, irStateBuffer, currencyIndex[j], k + 1, valuationTime, i) *
                                            effectiveMultiplier[j] +
                                        resFee[valuationIndex + 1][i],
                                    tradeId[j], valuationIndex, i, 1);
                            }
                        }
                        
                    }
                    if (sgd->getGrid()->isValuationDate()[k]) {
                        Date valuationDate = sgd->getGrid()->dates()[k];
                        Date closeOutDate = sgd->getGrid()->closeOutDateFromValuationDate(valuationDate);
                        closeOutDateToValuationDate[closeOutDate].push_back(std::make_tuple(valuationDate, t, ++dateIndex));
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            Real v = outputCube->get(tradeId[j], dateIndex, i, 0);
                            outputCube->set(v +
                                                res[dateIndex + 1][i] * fx(fxBuffer, currencyIndex[j], k + 1, i) *
                                                    numRatio(model, irStateBuffer, currencyIndex[j], k + 1, t, i) *
                                                    effectiveMultiplier[j] +
                                                resFee[dateIndex + 1][i],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                }
            } else {
                // actual date mpor mode: simulate all times in one go
                auto res = simulatePathInterface2(amcCalculators[j], pathTimes, paths, allTimes, allTimes, 
                                                  tradeLabel[j], tradeType[j]);
                Real v = outputCube->getT0(tradeId[j], 0);
                outputCube->setT0(v + res[0].at(0) * fx(fxBuffer, currencyIndex[j], 0, 0) *
                                          numRatio(model, irStateBuffer, currencyIndex[j], 0, 0.0, 0) *
                                          effectiveMultiplier[j],
                                  tradeId[j], 0);
                std::map<QuantLib::Date, std::vector<std::tuple<QuantLib::Date, double, size_t>>>
                    closeOutDateToValuationDate;
                int dateIndex = -1;
                for (Size k = 1; k < res.size(); ++k) {
                    Real t = sgd->getGrid()->timeGrid()[k];
                    if (sgd->getGrid()->isCloseOutDate()[k - 1]) {
                        Date closeOutDate = sgd->getGrid()->dates()[k - 1];
                        auto dateIndexIt = closeOutDateToValuationDate.find(closeOutDate);
                        QL_REQUIRE(dateIndexIt != closeOutDateToValuationDate.end() && !dateIndexIt->second.empty(),
                                   "The valuation date needs to before the corresponding close out date");
                        for (const auto& [valuationDate, valuationTime, valuationIndex] : dateIndexIt->second) {
                            for (Size i = 0; i < outputCube->samples(); ++i) {
                                Real v = outputCube->get(tradeId[j], valuationIndex, i, 1);
                                outputCube->set(v +
                                                    res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                        num(model, irStateBuffer, currencyIndex[j], k, t, i) *
                                                        effectiveMultiplier[j] +
                                                    resFee[k][i],
                                                tradeId[j], valuationIndex, i, 1);
                            }
                        }
                    }
                    if (sgd->getGrid()->isValuationDate()[k - 1]) {
                        Date valuationDate = sgd->getGrid()->dates()[k - 1];
                        Date closeOutDate = sgd->getGrid()->closeOutDateFromValuationDate(valuationDate);
                        closeOutDateToValuationDate[closeOutDate].push_back(std::make_tuple(valuationDate, t, ++dateIndex));
                        for (Size i = 0; i < outputCube->samples(); ++i) {
                            Real v = outputCube->get(tradeId[j], dateIndex, i, 0);
                            outputCube->set(v +
                                                res[k][i] * fx(fxBuffer, currencyIndex[j], k, i) *
                                                    numRatio(model, irStateBuffer, currencyIndex[j], k, t, i) *
                                                    effectiveMultiplier[j] +
                                                resFee[k][i],
                                            tradeId[j], dateIndex, i, 0);
                        }
                    }
                }
            }
        }
        std::ostringstream detail;
        detail << portfolio->size() << " trade" << (portfolio->size() == 1 ? "" : "s");
        progressIndicator->updateProgress(++progressCounter, portfolio->size(), detail.str());
    }
    timer.stop();
    valuationTime += timer.elapsed().wall * 1e-9;

    totalTime = timerTotal.elapsed().wall * 1e-9;
    residualTime = totalTime - (calibrationTime + pathGenTime + valuationTime + asdTime + bufferTime);
    LOG("calibration time     : " << calibrationTime << " sec");
    LOG("asd time             : " << asdTime << " sec");
    LOG("buffer time          : " << bufferTime << " sec");
    LOG("path generation time : " << pathGenTime << " sec");
    LOG("valuation time       : " << valuationTime << " sec");
    LOG("residual time        : " << residualTime << " sec");
    LOG("total time           : " << totalTime << " sec");
    LOG("AMCValuationEngine finished for one of possibly multiple threads.");
    LOG("RandomVariableStats  : ");
    LOG("Data Ops             : " << RandomVariableStats::instance().data_ops / 1E6 << " MOPS");
    LOG("Calc Ops             : " << RandomVariableStats::instance().calc_ops / 1E6 << " MOPS");
    LOG("Data Timer           : " << RandomVariableStats::instance().data_timer.elapsed().wall / 1E9 << " sec");
    LOG("Calc Timer           : " << RandomVariableStats::instance().calc_timer.elapsed().wall / 1E9 << " sec");
    LOG("Data Performace      : " << RandomVariableStats::instance().data_ops * 1E3 /
                                         RandomVariableStats::instance().data_timer.elapsed().wall
                                  << " MFLOPS");
    LOG("Calc Performace      : " << RandomVariableStats::instance().calc_ops * 1E3 /
                                         RandomVariableStats::instance().calc_timer.elapsed().wall
                                  << " MFLOPS");
    LOG("MC Other Timer       : " << McEngineStats::instance().other_timer.elapsed().wall / 1E9 << " sec");
    LOG("MC Path Timer        : " << McEngineStats::instance().path_timer.elapsed().wall / 1E9 << " sec");
    LOG("MC Calc Timer        : " << McEngineStats::instance().calc_timer.elapsed().wall / 1E9 << " sec");

} // runCoreEngine()

} // namespace

AMCValuationEngine::AMCValuationEngine(
    const QuantLib::Size nThreads, const QuantLib::Date& today, const QuantLib::Size nSamples,
    const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
    const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
    const std::vector<string>& aggDataIndices, const std::vector<string>& aggDataCurrencies,
    const Size aggDataNumberCreditStates, const QuantLib::ext::shared_ptr<CrossAssetModelData>& crossAssetModelData,
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
    const std::string& configurationLgmCalibration, const std::string& configurationFxCalibration,
    const std::string& configurationEqCalibration, const std::string& configurationInfCalibration,
    const std::string& configurationCrCalibration, const std::string& configurationFinalModel,
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
    const ore::data::IborFallbackConfig& iborFallbackConfig, const bool handlePseudoCurrenciesTodaysMarket,
    const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(
        const QuantLib::Date&, const std::set<std::string>&, const std::vector<QuantLib::Date>&, const QuantLib::Size)>&
        cubeFactory,
    const QuantLib::ext::shared_ptr<Scenario>& offSetScenario,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams)
    : useMultithreading_(true), aggDataIndices_(aggDataIndices), aggDataCurrencies_(aggDataCurrencies),
      aggDataNumberCreditStates_(aggDataNumberCreditStates), scenarioGeneratorData_(scenarioGeneratorData),
      nThreads_(nThreads), today_(today), nSamples_(nSamples), loader_(loader),
      crossAssetModelData_(crossAssetModelData), engineData_(engineData), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), configurationLgmCalibration_(configurationLgmCalibration),
      configurationFxCalibration_(configurationFxCalibration), configurationEqCalibration_(configurationEqCalibration),
      configurationInfCalibration_(configurationInfCalibration),
      configurationCrCalibration_(configurationCrCalibration), configurationFinalModel_(configurationFinalModel),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig),
      handlePseudoCurrenciesTodaysMarket_(handlePseudoCurrenciesTodaysMarket), cubeFactory_(cubeFactory),
      offsetScenario_(offSetScenario), simMarketParams_(simMarketParams) {
#ifndef QL_ENABLE_SESSIONS
    QL_FAIL(
        "AMCValuationEngine requires a build with QL_ENABLE_SESSIONS = ON when ctor multi-threaded runs is called.");
#endif
    QL_REQUIRE(scenarioGeneratorData_->seed() != 0,
               "AMCValuationEngine: path generation uses seed 0 - this might lead to inconsistent results to a classic "
               "simulation run, if both are combined. Consider using a non-zero seed.");
    if (!cubeFactory_)
        cubeFactory_ = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                          const std::vector<QuantLib::Date>& dates, const Size samples) {
            return QuantLib::ext::make_shared<ore::analytics::DoublePrecisionInMemoryCube>(asof, ids, dates, samples);
        };
}

AMCValuationEngine::AMCValuationEngine(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& model,
                                       const QuantLib::ext::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
                                       const QuantLib::ext::shared_ptr<Market>& market,
                                       const std::vector<string>& aggDataIndices,
                                       const std::vector<string>& aggDataCurrencies,
                                       const Size aggDataNumberCreditStates)
    : useMultithreading_(false), aggDataIndices_(aggDataIndices), aggDataCurrencies_(aggDataCurrencies),
      aggDataNumberCreditStates_(aggDataNumberCreditStates), scenarioGeneratorData_(scenarioGeneratorData),
      model_(model), market_(market) {

    QL_REQUIRE((aggDataIndices.empty() && aggDataCurrencies.empty()) || market != nullptr,
               "AMCValuationEngine: market is required for asd generation");
    QL_REQUIRE(scenarioGeneratorData_->seed() != 0,
               "AMCValuationEngine: path generation uses seed 0 - this might lead to inconsistent results to a classic "
               "simulation run, if both are combined. Consider using a non-zero seed.");
    QL_REQUIRE(
        model->irlgm1f(0)->termStructure()->dayCounter() == scenarioGeneratorData_->getGrid()->dayCounter(),
        "AMCValuationEngine: day counter in simulation parameters ("
            << scenarioGeneratorData_->getGrid()->dayCounter() << ") is different from model day counter ("
            << model->irlgm1f(0)->termStructure()->dayCounter()
            << "), align these e.g. by setting the day counter in the simulation parameters to the model day counter");
}

void AMCValuationEngine::buildCube(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                   QuantLib::ext::shared_ptr<NPVCube>& outputCube) {

    LOG("Starting single-threaded AMCValuationEngine for " << portfolio->size() << " trades, " << outputCube->samples()
                                                           << " samples and "
                                                           << scenarioGeneratorData_->getGrid()->size() << " dates.");

    QL_REQUIRE(!useMultithreading_, "AMCValuationEngine::buildCube() method was called with signature for "
                                    "single-threaded run, but engine was constructed for multi-threaded runs");

    QL_REQUIRE(portfolio->size() > 0, "AMCValuationEngine::buildCube: empty portfolio");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == scenarioGeneratorData_->getGrid()->valuationDates().size(),
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "different from number of valuation dates ("
                                    << scenarioGeneratorData_->getGrid()->valuationDates().size() << ")");

    try {
        // we can use the mt progress indicator here although we are running on a single thread
        runCoreEngine(portfolio, model_, market_, scenarioGeneratorData_, aggDataIndices_, aggDataCurrencies_,
                      aggDataNumberCreditStates_, asd_, outputCube,
                      QuantLib::ext::make_shared<ore::analytics::MultiThreadedProgressIndicator>(this->progressIndicators()));
    } catch (const std::exception& e) {
        QL_FAIL("Error during amc val engine run: " << e.what());
    }

    LOG("Finished single-threaded AMCValuationEngine run.");
}

void AMCValuationEngine::buildCube(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) {
    LOG("Starting multi-threaded AMCValuationEngine for "
        << portfolio->size() << " trades, " << nSamples_ << " samples and " << scenarioGeneratorData_->getGrid()->size()
        << " dates.");

    QL_REQUIRE(useMultithreading_, "AMCValuationEngine::buildCube() method was called with signature for "
                                   "multi-threaded run, but engine was constructed for single-threaded runs");

    QL_REQUIRE(portfolio->size() > 0, "AMCValuationEngine::buildCube: empty portfolio");

    // split portfolio into nThreads parts (just distribute the trades assuming all are approximately expensive)

    LOG("Splitting portfolio.");

    Size eff_nThreads = std::min(portfolio->size(), nThreads_);

    LOG("portfolio size = " << portfolio->size());
    LOG("nThreads       = " << nThreads_);
    LOG("eff nThreads   = " << eff_nThreads);

    QL_REQUIRE(eff_nThreads > 0, "effective threads are zero, this is not allowed.");

    std::vector<QuantLib::ext::shared_ptr<ore::data::Portfolio>> portfolios;
    for (Size i = 0; i < eff_nThreads; ++i)
        portfolios.push_back(QuantLib::ext::make_shared<ore::data::Portfolio>());

    Size portfolioIndex = 0;
    for (auto const& t : portfolio->trades()) {
        portfolios[portfolioIndex]->add(t.second);
        if (++portfolioIndex >= eff_nThreads)
            portfolioIndex = 0;
    }

    // output the portfolios into strings so that the worker threads can load them from there

    std::vector<std::string> portfoliosAsString;
    for (auto const& p : portfolios) {
        portfoliosAsString.emplace_back(p->toXMLString());
    }

    // log info on the portfolio split

    for (Size i = 0; i < eff_nThreads; ++i) {
        LOG("Portfolio #" << i << " number of trades       : " << portfolios[i]->size());
    }

    // build loaders for each thread as clones of the original one

    LOG("Cloning loaders for " << eff_nThreads << " threads...");
    std::vector<QuantLib::ext::shared_ptr<ore::data::ClonedLoader>> loaders;
    for (Size i = 0; i < eff_nThreads; ++i)
        loaders.push_back(QuantLib::ext::make_shared<ore::data::ClonedLoader>(today_, loader_));

    // build nThreads mini-cubes to which each thread writes its results

    LOG("Build " << eff_nThreads << " mini result cubes...");
    miniCubes_.clear();
    for (Size i = 0; i < eff_nThreads; ++i) {
        miniCubes_.push_back(
            cubeFactory_(today_, portfolios[i]->ids(), scenarioGeneratorData_->getGrid()->valuationDates(), nSamples_));
    }

    // precompute sim dates

    std::vector<Date> simDates =
        scenarioGeneratorData_->withCloseOutLag() && !scenarioGeneratorData_->withMporStickyDate()
            ? scenarioGeneratorData_->getGrid()->dates()
            : scenarioGeneratorData_->getGrid()->valuationDates();

    // build progress indicator consolidating the results from the threads

    auto progressIndicator =
        QuantLib::ext::make_shared<ore::analytics::MultiThreadedProgressIndicator>(this->progressIndicators());

    // create the thread pool with eff_nThreads and queue size = eff_nThreads as well

    // LOG("Create thread pool with " << eff_nThreads);
    // ctpl::thread_pool threadPool(eff_nThreads);

    // create the jobs and push them to the pool

    using resultType = int;
    std::vector<std::future<resultType>> results(eff_nThreads);

    std::vector<std::thread> jobs; // not needed if thread pool is used

    // get obs mode of main thread, so that we can set this mode in the worker threads below

    ore::analytics::ObservationMode::Mode obsMode = ore::analytics::ObservationMode::instance().mode();

    for (Size i = 0; i < eff_nThreads; ++i) {

        auto job = [this, obsMode, &portfoliosAsString, &loaders, &simDates, &progressIndicator](int id) -> resultType {
            // set thread local singletons

            QuantLib::Settings::instance().evaluationDate() = today_;
            ore::analytics::ObservationMode::instance().setMode(obsMode);

            LOG("Start thread " << id);

            int rc;

            try {

                // build todays market using cloned market data

                QuantLib::ext::shared_ptr<ore::data::Market> initMarket = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
                    today_, todaysMarketParams_, loaders[id], curveConfigs_, true, true, true, referenceData_, false,
                    iborFallbackConfig_, false, handlePseudoCurrenciesTodaysMarket_);

                QuantLib::ext::shared_ptr<ore::data::Market> market = initMarket;

                if(offsetScenario_ != nullptr){
                    QL_REQUIRE(simMarketParams_ != nullptr,
                               "AMC Valuation Engine can not build simMarket without simMarketParam");
                    bool continueOnError = true;
                    std::string configuration = configurationFinalModel_;
                    market = QuantLib::ext::make_shared<ScenarioSimMarket>(
                        initMarket, simMarketParams_, QuantLib::ext::make_shared<FixingManager>(today_), configuration,
                        *curveConfigs_, *todaysMarketParams_, continueOnError, true, true, false, iborFallbackConfig_,
                        false, offsetScenario_);
                }

                // build cam
                ore::data::CrossAssetModelBuilder modelBuilder(
                    market, crossAssetModelData_, configurationLgmCalibration_, configurationFxCalibration_,
                    configurationEqCalibration_, configurationInfCalibration_, configurationCrCalibration_,
                    configurationFinalModel_, false, true, "", SalvagingAlgorithm::None, "xva/amc cam building");

                auto cam = *modelBuilder.model();

                // build portfolio against init market

                auto portfolio = QuantLib::ext::make_shared<ore::data::Portfolio>();
                portfolio->fromXMLString(portfoliosAsString[id]);

                QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*engineData_);
                edCopy->globalParameters()["GenerateAdditionalResults"] = "false";
                edCopy->globalParameters()["RunType"] = "NPV";
                map<MarketContext, string> configurations{{MarketContext::irCalibration, configurationLgmCalibration_},
                                                          {MarketContext::fxCalibration, configurationFxCalibration_},
                                                          {MarketContext::pricing, configurationFinalModel_}};

                auto engineFactory = QuantLib::ext::make_shared<EngineFactory>(
                    edCopy, market, configurations, referenceData_, iborFallbackConfig_,
                    EngineBuilderFactory::instance().generateAmcEngineBuilders(cam, simDates), true);

                portfolio->build(engineFactory, "amc-val-engine", true);

                // run core engine code (asd is written for thread id 0 only)

                runCoreEngine(portfolio, cam, market, scenarioGeneratorData_, aggDataIndices_, aggDataCurrencies_,
                              aggDataNumberCreditStates_, id == 0 ? asd_ : nullptr, miniCubes_[id], progressIndicator);

                // return code 0 = ok

                LOG("Thread " << id << " successfully finished.");

                rc = 0;

            } catch (const std::exception& e) {

                // log error and return code 1 = not ok

                ore::analytics::StructuredAnalyticsErrorMessage("AMC Valuation Engine (multithreaded mode)", "",
                                                                e.what())
                    .log();
                rc = 1;
            }

            // exit

            return rc;
        };

        // results[i] = threadPool.push(job);

        // not needed if thread pool is used
        std::packaged_task<resultType(int)> task(job);
        results[i] = task.get_future();
        std::thread thread(std::move(task), i);
        jobs.emplace_back(std::move(thread));
    }

    // not needed if thread pool is used
    for (auto& t : jobs)
        t.join();

    for (Size i = 0; i < results.size(); ++i) {
        results[i].wait();
    }

    for (Size i = 0; i < results.size(); ++i) {
        QL_REQUIRE(results[i].valid(), "internal error: did not get a valid result");
        int rc = results[i].get();
        QL_REQUIRE(rc == 0, "error: thread " << i << " exited with return code " << rc
                                             << ". Check for structured errors from 'AMCValuationEngine'.");
    }

    // stop the thread pool, wait for unfinished jobs

    // LOG("Stop thread pool");
    // threadPool.stop(true);

    LOG("Finished multi-threaded AMCValuationEngine run.");
}

} // namespace analytics
} // namespace ore
