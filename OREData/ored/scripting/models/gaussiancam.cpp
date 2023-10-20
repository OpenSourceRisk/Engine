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

#include <ored/scripting/models/gaussiancam.hpp>
#include <qle/models/infdkvectorised.hpp>

#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/methods/multipathvariategenerator.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

GaussianCam::GaussianCam(const Handle<CrossAssetModel>& cam, const Size paths,
                         const std::vector<std::string>& currencies,
                         const std::vector<Handle<YieldTermStructure>>& curves,
                         const std::vector<Handle<Quote>>& fxSpots,
                         const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>>& irIndices,
                         const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>>& infIndices,
                         const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                         const std::set<Date>& simulationDates, const McParams& mcParams, const Size timeStepsPerYear,
                         const IborFallbackConfig& iborFallbackConfig,
                         const std::vector<Size>& projectedStateProcessIndices,
                         const std::vector<std::string>& conditionalExpectationModelStates)
    : ModelImpl(curves.front()->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                simulationDates, iborFallbackConfig),
      cam_(cam), curves_(curves), fxSpots_(fxSpots), mcParams_(mcParams), timeStepsPerYear_(timeStepsPerYear),
      projectedStateProcessIndices_(projectedStateProcessIndices) {

    QL_REQUIRE(!cam_.empty(), "model is empty");

    // register with observables

    for (auto const& o : curves_)
        registerWith(o);
    for (auto const& o : fxSpots_)
        registerWith(o);

    registerWith(cam_);

    // set conditional expectation model states

    conditionalExpectationUseIr_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "IR") !=
            conditionalExpectationModelStates.end();
    conditionalExpectationUseInf_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "INF") !=
            conditionalExpectationModelStates.end();
    conditionalExpectationUseAsset_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "Asset") !=
            conditionalExpectationModelStates.end();

} // GaussianCam ctor

Size GaussianCam::size() const {
    if (injectedPathTimes_ == nullptr)
        if (inTrainingPhase_)
            return mcParams_.trainingSamples;
        else
            return Model::size();
    else {
        return overwriteModelSize_;
    }
}

void GaussianCam::releaseMemory() {
    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();
    irStates_.clear();
    infStates_.clear();
    irStatesTraining_.clear();
    infStatesTraining_.clear();
    irIndexValueCache_.clear();
}

void GaussianCam::resetNPVMem() { storedRegressionCoeff_.clear(); }

const Date& GaussianCam::referenceDate() const {
    calculate();
    return referenceDate_;
}

void GaussianCam::performCalculations() const {

    QL_REQUIRE(!inTrainingPhase_, "GaussianCam::performCalculations(): state inTrainingPhase should be false, this was "
                                  "not resetted appropriately.");

    referenceDate_ = curves_.front()->referenceDate();

    // set up time grid

    effectiveSimulationDates_.clear();
    effectiveSimulationDates_.insert(referenceDate());
    for (auto const& d : simulationDates_) {
        if (d >= referenceDate())
            effectiveSimulationDates_.insert(d);
    }

    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        times.push_back(curves_.front()->timeFromReference(d));
    }

    Size steps = std::max(std::lround(timeStepsPerYear_ * times.back() + 0.5), 1l);
    timeGrid_ = TimeGrid(times.begin(), times.end(), steps);
    positionInTimeGrid_.resize(times.size());
    for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
        positionInTimeGrid_[i] = timeGrid_.index(times[i]);

    // clear underlying paths

    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();

    // init times vector and underlying path where we map a date to a randomvariable representing the path values

    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<RandomVariable>(indices_.size(), RandomVariable(size(), 0.0));
        irStates_[d] = std::vector<RandomVariable>(currencies_.size(), RandomVariable(size(), 0.0));
        infStates_[d] = std::vector<std::pair<RandomVariable, RandomVariable>>(
            infIndices_.size(), std::make_pair(RandomVariable(size(), 0.0), RandomVariable(size(), 0.0)));

        if(trainingSamples() != Null<Size>() && injectedPathTimes_ == nullptr) {
            underlyingPathsTraining_[d] =
                std::vector<RandomVariable>(indices_.size(), RandomVariable(trainingSamples(), 0.0));
            irStatesTraining_[d] =
                std::vector<RandomVariable>(currencies_.size(), RandomVariable(trainingSamples(), 0.0));
            infStatesTraining_[d] = std::vector<std::pair<RandomVariable, RandomVariable>>(
                infIndices_.size(),
                std::make_pair(RandomVariable(trainingSamples(), 0.0), RandomVariable(trainingSamples(), 0.0)));
        }

    }

    // populate index mappings

    currencyPositionInProcess_.clear();
    currencyPositionInCam_.clear();
    for (Size i = 0; i < currencies_.size(); ++i) {
        currencyPositionInProcess_.push_back(
            cam_->pIdx(CrossAssetModel::AssetType::IR, cam_->ccyIndex(parseCurrency(currencies_[i]))));
        currencyPositionInCam_.push_back(
            cam_->idx(CrossAssetModel::AssetType::IR, cam_->ccyIndex(parseCurrency(currencies_[i]))));
    }

    irIndexPositionInCam_.clear();
    for (Size i = 0; i < irIndices_.size(); ++i) {
        irIndexPositionInCam_.push_back(cam_->ccyIndex(irIndices_[i].second->currency()));
    }

    infIndexPositionInProcess_.clear();
    infIndexPositionInCam_.clear();
    for (Size i = 0; i < infIndices_.size(); ++i) {
        Size infIdx = cam_->infIndex(infIndices_[i].first.infName());
        infIndexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::INF, infIdx));
        infIndexPositionInCam_.push_back(infIdx);
    }

    indexPositionInProcess_.clear();
    for (Size i = 0; i < indices_.size(); ++i) {
        if (indices_[i].isFx()) {
            // FX
            Size ccyIdx = cam_->ccyIndex(parseCurrency(indexCurrencies_[i]));
            QL_REQUIRE(ccyIdx > 0, "fx index '" << indices_[i] << "' has unexpected for ccy = base ccy");
            indexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::FX, ccyIdx - 1));
            eqIndexInCam_.push_back(Null<Size>());
        } else if (indices_[i].isEq()) {
            // EQ
            Size eqIdx = cam_->eqIndex(indices_[i].eq()->name());
            indexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::EQ, eqIdx));
            eqIndexInCam_.push_back(eqIdx);
        } else {
            QL_FAIL("index '" << indices_[i].name() << "' expected to be FX or EQ");
        }
    }

    // populate path values

    populatePathValues(size(), underlyingPaths_, irStates_, infStates_, times, false);
    if (trainingSamples() != Null<Size>() && injectedPathTimes_ == nullptr) {
        populatePathValues(trainingSamples(), underlyingPathsTraining_, irStatesTraining_, infStatesTraining_, times,
                           true);
    }
}

void GaussianCam::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                     std::map<Date, std::vector<RandomVariable>>& irStates,
                                     std::map<Date, std::vector<std::pair<RandomVariable, RandomVariable>>>& infStates,
                                     const std::vector<Real>& times, const bool isTraining) const {
    // get state process

    auto process = cam_->stateProcess();

    // set reference date values, if there are no future simulation dates, we are done

    // FX and EQ indcies
    for (Size k = 0; k < indices_.size(); ++k) {
        paths[referenceDate_][k].setAll(process->initialValues().at(indexPositionInProcess_[k]));
    }

    // IR states per currency (they are all just 0)
    for (Size k = 0; k < currencies_.size(); ++k) {
        irStates[referenceDate()][k].setAll(0.0);
    }

    // INF DK or JY state, we happen to have two components (x,y) for each, so no case distinction needed
    for (Size k = 0; k < infIndices_.size(); ++k) {
        infStates[referenceDate()].push_back(
            std::make_pair(RandomVariable(nSamples, process->initialValues().at(infIndexPositionInProcess_[k])),
                           RandomVariable(nSamples, process->initialValues().at(infIndexPositionInProcess_[k] + 1))));
    }

    if (effectiveSimulationDates_.size() == 1)
        return;

    // populate path values

    if (process->size() == 1 && injectedPathTimes_ == nullptr) {

        // we treat the case of a one dimensional process separately for optimisation reasons; we know that in
        // this case we have a single, driftless LGM process for currency 0

        auto lgmProcess = boost::dynamic_pointer_cast<StochasticProcess1D>(cam_->lgm(0)->stateProcess());

        std::vector<Real> stdDevs(times.size() - 1);
        for (Size i = 0; i < times.size() - 1; ++i) {
            stdDevs[i] = lgmProcess->stdDeviation(times[i], 0.0, times[i + 1] - times[i]);
        }

        // generate paths using own variate generator
        auto gen =
            makeMultiPathVariateGenerator(isTraining ? mcParams_.trainingSequenceType : mcParams_.sequenceType, 1,
                                          times.size() - 1, isTraining ? mcParams_.trainingSeed : mcParams_.seed,
                                          mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers);

        for (auto s = std::next(irStates.begin(), 1); s != irStates.end(); ++s)
            for (auto& r : s->second)
                r.expand();

        for (Size path = 0; path < nSamples; ++path) {
            auto p = gen->next();
            Real state = 0.0;
            auto date = effectiveSimulationDates_.begin();
            for (Size i = 0; i < times.size() - 1; ++i) {
                state += stdDevs[i] * p.value[i][0];
                ++date;
                irStates[*date][0].data()[path] = state;
            }
        }

    } else {

        // case process size > 1 or we have injected paths, we use the normal process interface to evolve the process

        boost::shared_ptr<MultiPathGeneratorBase> pathGen;

        // build a temporary repository of the state prcess values, since we want to access them not path by path
        // below - for efficiency reasons the loop over the paths should be the innermost loop there!

        std::vector<std::vector<std::vector<Real>>> pathValues(
            effectiveSimulationDates_.size() - 1,
            std::vector<std::vector<Real>>(process->size(), std::vector<Real>(nSamples)));

        if (injectedPathTimes_ == nullptr) {

            // the usual path generator

            if (auto tmp = boost::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
                tmp->resetCache(timeGrid_.size() - 1);
            }

            auto pathGen =
                makeMultiPathGenerator(isTraining ? mcParams_.trainingSequenceType : mcParams_.sequenceType, process,
                                       timeGrid_, isTraining ? mcParams_.trainingSeed : mcParams_.seed,
                                       mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers);
            for (Size i = 0; i < nSamples; ++i) {
                MultiPath path = pathGen->next().value;
                for (Size j = 0; j < effectiveSimulationDates_.size() - 1; ++j) {
                    for (Size k = 0; k < process->size(); ++k) {
                        pathValues[j][k][i] = path[k][positionInTimeGrid_[j + 1]];
                    }
                }
            }
        } else {

            // simple linear interpolation of injected paths, TODO explore the usage of Brownian Bridges here

            std::vector<Real> relevantPathTimes;
            std::vector<Size> relevantPathIndices;
            QL_REQUIRE(!injectedPathStickyCloseOutRun_ || !((*injectedPathIsRelevantTime_)[0]),
                       "internal error: first injected time is relevant and stickyCloseOutRun is true");
            for (Size j = 0; j < injectedPathTimes_->size(); ++j) {
                if ((*injectedPathIsRelevantTime_)[j]) {
                    relevantPathTimes.push_back((*injectedPathTimes_)[injectedPathStickyCloseOutRun_ ? j - 1 : j]);
                    relevantPathIndices.push_back(j);
                }
            }
            Array y(relevantPathTimes.size());
            auto pathInterpolator =
                LinearFlat().interpolate(relevantPathTimes.begin(), relevantPathTimes.end(), y.begin());
            pathInterpolator.enableExtrapolation();
            for (Size i = 0; i < nSamples; ++i) {
                for (Size k = 0; k < process->size(); ++k) {
                    for (Size j = 0; j < relevantPathTimes.size(); ++j) {
                        y[j] = (*injectedPaths_)[relevantPathIndices[j]][projectedStateProcessIndices_[k]][i];
                    }
                    pathInterpolator.update();
                    for (Size j = 0; j < times.size() - 1; ++j) {
                        pathValues[j][k][i] = pathInterpolator(times[j + 1]);
                    }
                }
            }
        }

        // FX and EQ indcies
        std::vector<std::vector<RandomVariable*>> rvs(
            indices_.size(), std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
        auto date = effectiveSimulationDates_.begin();
        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
            ++date;
            for (Size j = 0; j < indices_.size(); ++j) {
                rvs[j][i] = &paths[*date][j];
                rvs[j][i]->expand();
            }
        }
        for (Size k = 0; k < indices_.size(); ++k) {
            for (Size j = 1; j < effectiveSimulationDates_.size(); ++j) {
                for (Size i = 0; i < nSamples; ++i) {
                    rvs[k][j - 1]->data()[i] = std::exp(pathValues[j - 1][indexPositionInProcess_[k]][i]);
                }
            }
        }

        // IR states per currency
        std::vector<std::vector<RandomVariable*>> rvs2(
            currencies_.size(), std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
        auto date2 = effectiveSimulationDates_.begin();
        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
            ++date2;
            for (Size j = 0; j < currencies_.size(); ++j) {
                rvs2[j][i] = &irStates[*date2][j];
                rvs2[j][i]->expand();
            }
        }
        for (Size k = 0; k < currencies_.size(); ++k) {
            for (Size j = 1; j < effectiveSimulationDates_.size(); ++j) {
                for (Size i = 0; i < nSamples; ++i) {
                    rvs2[k][j - 1]->data()[i] = pathValues[j - 1][currencyPositionInProcess_[k]][i];
                }
            }
        }

        // INF states per index, again we do not need to distinguish DK and JY here
        std::vector<std::vector<RandomVariable*>> rvs3a(
            infIndices_.size(), std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
        std::vector<std::vector<RandomVariable*>> rvs3b(
            infIndices_.size(), std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
        auto date3 = effectiveSimulationDates_.begin();
        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
            ++date3;
            for (Size j = 0; j < infIndices_.size(); ++j) {
                rvs3a[j][i] = &infStates[*date3][j].first;
                rvs3b[j][i] = &infStates[*date3][j].second;
                rvs3a[j][i]->expand();
                rvs3b[j][i]->expand();
            }
        }
        for (Size k = 0; k < infIndices_.size(); ++k) {
            for (Size j = 1; j < effectiveSimulationDates_.size(); ++j) {
                for (Size i = 0; i < nSamples; ++i) {
                    rvs3a[k][j - 1]->data()[i] = pathValues[j - 1][infIndexPositionInProcess_[k]][i];
                    rvs3b[k][j - 1]->data()[i] = pathValues[j - 1][infIndexPositionInProcess_[k] + 1][i];
                }
            }
        }
    }
}

RandomVariable GaussianCam::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    auto res = underlyingPaths_.at(d).at(indexNo);
    // compute forwarding factor
    if (fwd != Null<Date>()) {
        auto ccy = std::find(currencies_.begin(), currencies_.end(), indexCurrencies_[indexNo]);
        QL_REQUIRE(ccy != currencies_.end(), "GaussianCam::getIndexValue(): can not get currency for index #"
                                                 << indexNo << "(" << indices_.at(indexNo) << ")");
        if (indices_[indexNo].isFx()) {
            res *= getDiscount(std::distance(currencies_.begin(), ccy), d, fwd) / getDiscount(0, d, fwd);
        } else if (eqIndexInCam_[indexNo] != Null<Size>()) {
            // the CAM assumes a deterministic dividend curve for EQ
            auto div = cam_->eqbs(eqIndexInCam_[indexNo])->equityDivYieldCurveToday();
            res *= RandomVariable(size(), div->discount(fwd) / div->discount(d)) /
                   getDiscount(std::distance(currencies_.begin(), ccy), d, fwd,
                               cam_->eqbs(eqIndexInCam_[indexNo])->equityIrCurveToday());
        } else {
            QL_FAIL("GaussianGam::getIndexValue(): did not recognise  index #" << indexNo << "(" << indices_.at(indexNo)
                                                                               << ")");
        }
    }
    return res;
}

RandomVariable GaussianCam::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date fixingDate = d;
    if (fwd != Null<Date>())
        fixingDate = fwd;
    // ensure a valid fixing date
    fixingDate = irIndices_[indexNo].second->fixingCalendar().adjust(fixingDate);
    // look up required fixing in cache and return it if found
    auto cacheValue = irIndexValueCache_.find(std::make_tuple(indexNo, d, fixingDate));
    if (cacheValue != irIndexValueCache_.end())
        return cacheValue->second;
    // compute value, add to cache and return it
    Size currencyIdx = irIndexPositionInCam_[indexNo];
    LgmVectorised lgmv(cam_->irlgm1f(currencyIdx));
    auto result = lgmv.fixing(irIndices_[indexNo].second, fixingDate, curves_.front()->timeFromReference(d),
                              irStates_.at(d).at(currencyIdx));
    irIndexValueCache_[std::make_tuple(indexNo, d, fixingDate)] = result;
    return result;
}

RandomVariable GaussianCam::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date fixingDate = d;
    Date obsDate = d;
    if (fwd != Null<Date>())
        fixingDate = fwd;
    Size lag = getInflationSimulationLag(infIndices_[indexNo].second);
    auto const& state = infStates_.at(obsDate + lag).at(indexNo);
    Size camIndex = infIndexPositionInCam_[indexNo];
    Real t = infIndices_[indexNo].second->zeroInflationTermStructure()->timeFromReference(obsDate + lag);
    Real T = infIndices_[indexNo].second->zeroInflationTermStructure()->timeFromReference(fixingDate + lag);
    QL_DEPRECATED_DISABLE_WARNING
    bool isInterpolated = infIndices_[indexNo].second->interpolated();
    QL_DEPRECATED_ENABLE_WARNING
    Real baseFixing =
        infIndices_[indexNo].second->fixing(infIndices_[indexNo].second->zeroInflationTermStructure()->baseDate());
    RandomVariable result(size());

    if (cam_->modelType(CrossAssetModel::AssetType::INF, camIndex) == CrossAssetModel::ModelType::DK) {
        InfDkVectorised infdkv(*cam_);
        RandomVariable baseFixingVec(size(), baseFixing);
        QL_REQUIRE(t < T || close_enough(t, T), "infdkI: t (" << t << ") <= T (" << T << ") required");
        auto dk = infdkv.infdkI(camIndex, t, T, state.first, state.second, isInterpolated);
        result = baseFixingVec * dk.first * (fixingDate != obsDate ? dk.second : RandomVariable(size(), 1.0));
    } else if (cam_->modelType(CrossAssetModel::AssetType::INF, camIndex) == CrossAssetModel::ModelType::JY) {
        result = exp(state.second);
        if (fixingDate != obsDate) {
            // we need a forward cpi, TODO vectorise this computation
            RandomVariable growthFactor(size());
            growthFactor.expand();
            for (Size p = 0; p < size(); ++p) {
                growthFactor.data()[p] =
                    inflationGrowth(*cam_, indexNo, t, T, state.first[p], state.second[p], isInterpolated);
            }
            result *= growthFactor;
        }
    } else {
        QL_FAIL("GaussianCam::getInfIndexValue(): unknown model type for inflation index "
                << infIndices_[indexNo].first.name());
    }
    return result;
}

RandomVariable GaussianCam::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                       const Date& start, const Date& end, const Real spread, const Real gearing,
                                       const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                       const bool includeSpread, const Real cap, const Real floor,
                                       const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto ir = std::find_if(irIndices_.begin(), irIndices_.end(),
                           [&indexInput](const std::pair<IndexInfo, boost::shared_ptr<InterestRateIndex>>& p) {
                               return p.first.name() == indexInput;
                           });
    QL_REQUIRE(ir != irIndices_.end(),
               "ModelImpl::fwdComp() ir index " << indexInput << " not found, this is unexpected");
    Size irIndexPos = irIndexPositionInCam_[std::distance(irIndices_.begin(), ir)];
    LgmVectorised lgmv(cam_->lgm(irIndexPos)->parametrization());
    auto on = boost::dynamic_pointer_cast<OvernightIndex>(ir->second);
    QL_REQUIRE(on, "GaussianCam::fwdCompAvg(): expected on index for " << indexInput);
    // only used to extract fixing and value dates
    auto coupon = boost::make_shared<QuantExt::OvernightIndexedCoupon>(
        end, 1.0, start, end, on, gearing, spread, Date(), Date(), DayCounter(), false, includeSpread, lookback * Days,
        rateCutoff, fixingDays);
    // get model time and state
    Date effobsdate = std::max(referenceDate(), obsdate);
    const auto& modelState = irStates_.at(effobsdate)[irIndexPos];
    const auto& modelTime = curves_.front()->timeFromReference(effobsdate);
    if (isAvg) {
        return lgmv.averagedOnRate(on, coupon->fixingDates(), coupon->valueDates(), coupon->dt(), rateCutoff,
                                   includeSpread, spread, gearing, lookback * Days, cap, floor, localCapFloor,
                                   nakedOption, modelTime, modelState);
    } else {
        return lgmv.compoundedOnRate(on, coupon->fixingDates(), coupon->valueDates(), coupon->dt(), rateCutoff,
                                     includeSpread, spread, gearing, lookback * Days, cap, floor, localCapFloor,
                                     nakedOption, modelTime, modelState);
    }
}

RandomVariable GaussianCam::getDiscount(const Size idx, const Date& s, const Date& t) const {
    return getDiscount(idx, s, t, Handle<YieldTermStructure>());
}

RandomVariable GaussianCam::getDiscount(const Size idx, const Date& s, const Date& t,
                                        const Handle<YieldTermStructure>& targetCurve) const {
    LgmVectorised lgmv(cam_->lgm(currencyPositionInCam_[idx])->parametrization());
    return lgmv.discountBond(curves_.front()->timeFromReference(s), curves_.front()->timeFromReference(t),
                             irStates_.at(s)[idx], targetCurve);
}

RandomVariable GaussianCam::getNumeraire(const Date& s) const {
    LgmVectorised lgmv(cam_->lgm(currencyPositionInCam_[0])->parametrization());
    return lgmv.numeraire(curves_.front()->timeFromReference(s), irStates_.at(s)[0]);
}

Real GaussianCam::getFxSpot(const Size idx) const { return fxSpots_.at(idx)->value(); }

RandomVariable GaussianCam::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                                const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                                const RandomVariable& addRegressor2) const {

    calculate();

    // short cut, if amount is deterministic and no memslot is given

    if (amount.deterministic() && !memSlot)
        return amount;

    // if obsdate is today, take a plain expectation

    if (obsdate == referenceDate())
        return expectation(amount);

    // build the state

    std::vector<const RandomVariable*> state;

    if (conditionalExpectationUseAsset_ && !underlyingPaths_.empty()) {
        for (auto const& r : underlyingPaths_.at(obsdate))
            state.push_back(&r);
    }

    // TODO we include zero vol ir states here, we could exclude them
    if (conditionalExpectationUseIr_) {
        for (auto const& r : irStates_.at(obsdate))
            state.push_back(&r);
    }

    // valid for both DK and JY
    if (conditionalExpectationUseInf_) {
        for (auto const& r : infStates_.at(obsdate)) {
            state.push_back(&r.first);
            state.push_back(&r.second);
        }
    }

    Size nModelStates = state.size();

    // if memSlot is given we have to make sure the state always has the same size
    if (addRegressor1.initialised() && (memSlot || !addRegressor1.deterministic()))
        state.push_back(&addRegressor1);
    if (addRegressor2.initialised() && (memSlot || !addRegressor2.deterministic()))
        state.push_back(&addRegressor2);

    Size nAddReg = state.size() - nModelStates;

    // if the state is empty, return the plain expectation (no conditioning)

    if (state.empty()) {
        return expectation(amount);
    }

    // generate basis if not yet done

    if (basisFns_.find(state.size()) == basisFns_.end())
        basisFns_[state.size()] = multiPathBasisSystem(state.size(), mcParams_.regressionOrder, mcParams_.polynomType,
                                                       std::min(size(), trainingSamples()));

    // if a memSlot is given and coefficients are stored, we use them

    Array coeff;

    if (memSlot) {
        auto it = storedRegressionCoeff_.find(*memSlot);
        if (it != storedRegressionCoeff_.end()) {
            coeff = it->second.first;
            QL_REQUIRE(it->second.second == state.size(),
                       "GaussianCam::npv(): stored regression coefficients at mem slot "
                           << *memSlot << " are for state size " << it->second.second << ", actual state size is "
                           << state.size());
        }
    }

    // otherwise compute coefficients and store them if a memSlot is given

    if (coeff.size() == 0) {
        coeff = regressionCoefficients(amount, state, basisFns_.at(state.size()), filter,
                                       RandomVariableRegressionMethod::QR);
        DLOG("GaussianCam::npv(" << ore::data::to_string(obsdate) << "): regression coefficients are " << coeff
                                 << " (got model state size " << nModelStates << " and " << nAddReg
                                 << " additional regressors)");
        if (memSlot) {
            storedRegressionCoeff_[*memSlot] = std::make_pair(coeff, state.size());
        }
    }

    // compute conditional expectation and return the result

    return conditionalExpectation(state, basisFns_.at(state.size()), coeff);
}

void GaussianCam::toggleTrainingPaths() const {
    std::swap(underlyingPaths_, underlyingPathsTraining_);
    std::swap(irStates_, irStatesTraining_);
    std::swap(infStates_, infStatesTraining_);
    inTrainingPhase_ = !inTrainingPhase_;
    irIndexValueCache_.clear();
}

Size GaussianCam::trainingSamples() const { return mcParams_.trainingSamples; }

void GaussianCam::injectPaths(const std::vector<QuantLib::Real>* pathTimes,
                              const std::vector<std::vector<QuantExt::RandomVariable>>* paths,
                              const std::vector<bool>* isRelevantTime, const bool stickyCloseOutRun) {

    if (pathTimes == nullptr) {
        // reset injected path data
        injectedPathTimes_ = nullptr;
        injectedPaths_ = nullptr;
        injectedPathIsRelevantTime_ = nullptr;
        injectedPathStickyCloseOutRun_ = false; // arbitrary
        return;
    }

    QL_REQUIRE(!pathTimes->empty(), "GaussianCam::injectPaths(): injected path times empty");

    QL_REQUIRE(pathTimes->size() == paths->size(), "GaussianCam::injectPaths(): path times ("
                                                       << pathTimes->size() << ") must match path size ("
                                                       << paths->size() << ")");

    QL_REQUIRE(pathTimes->size() == isRelevantTime->size(),
               "GaussianCam::injectPaths(): path times (" << pathTimes->size() << ") must match isRelevanTime size ("
                                                          << isRelevantTime->size() << ")");

    QL_REQUIRE(projectedStateProcessIndices_.size() == cam_->dimension(),
               "GaussianCam::injectPaths(): number of projected state process indices ("
                   << projectedStateProcessIndices_.size() << ") must match model dimension (" << cam_->dimension());

    Size maxProjectedStateProcessIndex =
        *std::max_element(projectedStateProcessIndices_.begin(), projectedStateProcessIndices_.end());

    for (auto const& v : *paths) {
        QL_REQUIRE(v.size() > maxProjectedStateProcessIndex, "GaussianCam::injectPaths(): dimension of variates ("
                                                                 << v.size()
                                                                 << ") must cover max projected state process index ("
                                                                 << maxProjectedStateProcessIndex << ")");
        overwriteModelSize_ = v.front().size();
    }

    injectedPathTimes_ = pathTimes;
    injectedPaths_ = paths;
    injectedPathIsRelevantTime_ = isRelevantTime;
    injectedPathStickyCloseOutRun_ = stickyCloseOutRun;
    update();
}

} // namespace data
} // namespace ore
