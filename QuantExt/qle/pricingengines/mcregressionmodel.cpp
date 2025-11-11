/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/mcregressionmodel.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>

namespace QuantExt {

RegressionModel::RegressionModel(const Real observationTime,
                                                       const std::vector<CashflowInfo>& cashflowInfo,
                                                       const std::function<bool(std::size_t)>& cashflowRelevant,
                                                       const CrossAssetModel& model,
                                                       const RegressionModel::RegressorModel regressorModel,
                                                       const Real regressionVarianceCutoff,
                                                       const Size regressionMaxSimTimesIr,
                                                       const Size regressionMaxSimTimesFx,
                                                       const Size regressionMaxSimTimesEq,
                                                       const RegressionModel::VarGroupMode regressionVarGroupMode)
    : observationTime_(observationTime), regressionVarianceCutoff_(regressionVarianceCutoff) {

    // we always include the full model state as of the observation time

    for (Size m = 0; m < model.dimension(); ++m) {
        regressorTimesModelIndices_.insert(std::make_pair(observationTime, m));
    }

    std::set<Size> modelIrIndices;
    std::set<Size> modelFxIndices;
    std::set<Size> modelEqIndices;

    // for Lagged and LaggedIR we add past ir states

    if (regressorModel == RegressionModel::RegressorModel::Lagged ||
        regressorModel == RegressionModel::RegressorModel::LaggedIR) {
        for (Size i = 0; i < model.components(CrossAssetModel::AssetType::IR); ++i)
            modelIrIndices.insert(model.pIdx(CrossAssetModel::AssetType::IR, i));
    }

    // for Lagged and LaggedFX we add past fx states

    if (regressorModel == RegressionModel::RegressorModel::Lagged ||
        regressorModel == RegressionModel::RegressorModel::LaggedFX) {
        for (Size i = 1; i < model.components(CrossAssetModel::AssetType::IR); ++i)
            for (Size j = 0; j < model.stateVariables(CrossAssetModel::AssetType::FX, i - 1); ++j)
                modelFxIndices.insert(model.pIdx(CrossAssetModel::AssetType::FX, i - 1, j));
    }

    // for Lagged and LaggedEQ we add past eq states

    if (regressorModel == RegressionModel::RegressorModel::Lagged ||
        regressorModel == RegressionModel::RegressorModel::LaggedEQ) {
        for (Size i = 0; i < model.components(CrossAssetModel::AssetType::EQ); ++i)
            modelEqIndices.insert(model.pIdx(CrossAssetModel::AssetType::EQ, i));
    }

    auto fillRegressorTimesModelIndices = [this, &cashflowInfo, &cashflowRelevant] (
        const std::set<Size>& modelIndices, Size maxSimTimes) {
        if (modelIndices.empty()) {
            return;
        }
        std::map<Size, std::vector<Time>> relevantTime;
        for (Size i = 0; i < cashflowInfo.size(); ++i) {
            if (!cashflowRelevant(i))
                continue;
            // add the cashflow simulation indices
            for (Size j = 0; j < cashflowInfo[i].simulationTimes.size(); ++j) {
                Real t = std::min(observationTime_, cashflowInfo[i].simulationTimes[j]);
                // the simulation time might be zero, but then we want to skip the factors
                if (QuantLib::close_enough(t, 0.0))
                    continue;
                for (auto& m : cashflowInfo[i].modelIndices[j]) {
                    if (modelIndices.find(m) != modelIndices.end()) {
                        if (relevantTime.find(m) == relevantTime.end()) {
                            relevantTime[m] = {t};
                        } else {
                            relevantTime[m].push_back(t);
                        }
                    }
                }
            }
        }
        for (const auto& [m, time] : relevantTime) {
            Size maxSimTimesLocal = maxSimTimes == 0 ? time.size() : maxSimTimes;
            Real step = std::max(time.size() / static_cast<Real>(maxSimTimesLocal), 1.0);
            for (Size j = 0; j < maxSimTimesLocal - 1; ++j) {
                Size idx = static_cast<Size>(j * step);
                if (idx >= time.size())
                    break;
                regressorTimesModelIndices_.insert(std::make_pair(time[idx], m));
            }
        }
    };

    fillRegressorTimesModelIndices(modelIrIndices, regressionMaxSimTimesIr);
    fillRegressorTimesModelIndices(modelFxIndices, regressionMaxSimTimesFx);
    fillRegressorTimesModelIndices(modelEqIndices, regressionMaxSimTimesEq);

    if (regressionVarGroupMode == RegressionModel::VarGroupMode::Global) {
        varGroups_ = {};
    } else if (regressionVarGroupMode == RegressionModel::VarGroupMode::Trivial) {
        for (Size i = 0; i < regressorTimesModelIndices_.size(); ++i) {
            varGroups_.insert({i});
        }
    } else {
        QL_FAIL("RegressionModelRegressionModel(): unknown regressionVarGroupMode");
    }
}

void RegressionModel::train(const Size polynomOrder,
                                                  const LsmBasisSystem::PolynomialType polynomType,
                                                  const RandomVariable& regressand,
                                                  const std::vector<std::vector<const RandomVariable*>>& paths,
                                                  const std::set<Real>& pathTimes, const Filter& filter) {

    // check if the model is in the correct state

    QL_REQUIRE(!isTrained_, "RegressionModeltrain(): internal error: model is already trained, "
                            "train() should not be called twice on the same model instance.");

    /* build the regressor - if the regressand is identically zero we leave it empty which optimizes
    out unnecessary calculations below */

    std::vector<const RandomVariable*> regressor;
    if (!regressand.deterministic() || !QuantLib::close_enough(regressand.at(0), 0.0)) {
        for (auto const& [t, modelIdx] : regressorTimesModelIndices_) {
            auto pt = pathTimes.find(t);
            QL_REQUIRE(pt != pathTimes.end(),
                       "RegressionModeltrain(): internal error: did not find regressor time "
                           << t << " in pathTimes.");
            regressor.push_back(paths[std::distance(pathTimes.begin(), pt)][modelIdx]);
        }
    }

    // factor reduction to reduce dimensionalitty and handle collinearity

    std::vector<RandomVariable> transformedRegressor;
    if (regressionVarianceCutoff_ != Null<Real>()) {
        coordinateTransform_ = pcaCoordinateTransform(regressor, regressionVarianceCutoff_);
        transformedRegressor = applyCoordinateTransform(regressor, coordinateTransform_);
        regressor = vec2vecptr(transformedRegressor);
    }

    // compute regression coefficients

    if (!regressor.empty()) {

        // get the basis functions
        basisDim_ = regressor.size();
        basisOrder_ = polynomOrder;
        basisType_ = polynomType;
        basisSystemSizeBound_ = Null<Size>();

        basisFns_ = multiPathBasisSystem(regressor.size(), polynomOrder, polynomType, varGroups_, Null<Size>());

        // compute the regression coefficients

        regressionCoeffs_ =
            regressionCoefficients(regressand, regressor, basisFns_, filter, RandomVariableRegressionMethod::QR);

    } else {

        /* an empty regressor is possible if there are no relevant cashflows, but then the regressand
           has to be zero too */

        QL_REQUIRE(close_enough_all(regressand, RandomVariable(regressand.size(), 0.0)),
                   "RegressionModeltrain(): internal error: regressand is not identically "
                   "zero, but no regressor was built.");
    }

    // update state of model

    isTrained_ = true;
}

RandomVariable
RegressionModel::apply(const Array& initialState,
                                             const std::vector<std::vector<const RandomVariable*>>& paths,
                                             const std::set<Real>& pathTimes) const {

    // check if model is trained

    QL_REQUIRE(isTrained_, "McMultiLegBaseEngine::RegressionMdeol::apply(): internal error: model is not trained.");

    // determine sample size

    QL_REQUIRE(!paths.empty() && !paths.front().empty(),
               "McMultiLegBaseEngine::RegressionMdeol::apply(): paths are empty or have empty first component");
    Size samples = paths.front().front()->size();

    // if we do not have regression coefficients, the regressand was zero

    if (regressionCoeffs_.empty())
        return RandomVariable(samples, 0.0);

    // build initial state pointer

    std::vector<RandomVariable> initialStateValues(initialState.size());
    std::vector<const RandomVariable*> initialStatePointer(initialState.size());
    for (Size j = 0; j < initialState.size(); ++j) {
        initialStateValues[j] = RandomVariable(samples, initialState[j]);
        initialStatePointer[j] = &initialStateValues[j];
    }

    // build the regressor

    std::vector<const RandomVariable*> regressor(regressorTimesModelIndices_.size());
    std::vector<RandomVariable> tmp(regressorTimesModelIndices_.size());

    Size i = 0;
    for (auto const& [t, modelIdx] : regressorTimesModelIndices_) {
        auto pt = pathTimes.find(t);
        if (pt != pathTimes.end()) {

            // the time is a path time, no need to interpolate the path

            regressor[i] = paths[std::distance(pathTimes.begin(), pt)][modelIdx];

        } else {

            // the time is not a path time, we need to interpolate:
            // find the sim times and model states before and after the exercise time

            auto t2 = std::lower_bound(pathTimes.begin(), pathTimes.end(), t);

            // t is after last path time => flat extrapolation

            if (t2 == pathTimes.end()) {
                regressor[i] = paths[pathTimes.size() - 1][modelIdx];
                ++i;
                continue;
            }

            // t is before last path time

            Real time2 = *t2;
            const RandomVariable* s2 = paths[std::distance(pathTimes.begin(), t2)][modelIdx];

            Real time1;
            const RandomVariable* s1;
            if (t2 == pathTimes.begin()) {
                time1 = 0.0;
                s1 = initialStatePointer[modelIdx];
            } else {
                time1 = *std::next(t2, -1);
                s1 = paths[std::distance(pathTimes.begin(), std::next(t2, -1))][modelIdx];
            }

            // compute the interpolated state

            RandomVariable alpha1(samples, (time2 - t) / (time2 - time1));
            RandomVariable alpha2(samples, (t - time1) / (time2 - time1));
            tmp[i] = alpha1 * *s1 + alpha2 * *s2;
            regressor[i] = &tmp[i];
        }
        ++i;
    }

    // transform regressor if necessary

    std::vector<RandomVariable> transformedRegressor;
    if (!coordinateTransform_.empty()) {
        transformedRegressor = applyCoordinateTransform(regressor, coordinateTransform_);
        regressor = vec2vecptr(transformedRegressor);
    }

    // compute result and return it

    return conditionalExpectation(regressor, basisFns_, regressionCoeffs_);
}

template <class Archive> void RegressionModel::serialize(Archive& ar, const unsigned int version) {
    ar& observationTime_;
    ar& regressionVarianceCutoff_;
    ar& isTrained_;
    ar& regressorTimesModelIndices_;
    ar& coordinateTransform_;
    ar& regressionCoeffs_;
    ar& varGroups_;

    // serialise the function by serialising the paramters needed
    ar& basisDim_;
    ar& basisOrder_;
    ar& basisType_;
    ar& basisSystemSizeBound_;

    // if deserialising, recreate the basisFns_ by passing the individual parameters to the function
    if (Archive::is_loading::value) {
        if (basisDim_ > 0)
            basisFns_ = multiPathBasisSystem(basisDim_, basisOrder_, basisType_, {}, basisSystemSizeBound_);
    }
}

template void QuantExt::RegressionModel::serialize(boost::archive::binary_iarchive& ar,
                                                                         const unsigned int version);
template void QuantExt::RegressionModel::serialize(boost::archive::binary_oarchive& ar,
                                                                         const unsigned int version);
} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::RegressionModel);