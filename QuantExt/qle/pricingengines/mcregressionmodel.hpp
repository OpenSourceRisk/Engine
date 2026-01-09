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

#pragma once

#include <ql/types.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/pricingengines/mccashflowinfo.hpp>

#include <set>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/export.hpp>

namespace QuantExt {

//! Regression helper built on a observation time (e.g. XVA or exercise date). Trains a set of
//! basis functions on simulated state variables to approximate continuation values.
//! Used in American Monte Carlo (AMC) pricing engines for regression based valuation of
//! early exercise options or XVA valuation.
class McRegressionModel {
public:
    enum class RegressorModel { Simple, Lagged, LaggedIR, LaggedFX, LaggedEQ };
    enum class VarGroupMode { Global, Trivial };

    McRegressionModel() = default;
    McRegressionModel(const Real observationTime, const std::vector<McCashflowInfo>& cashflowInfo,
                    const std::function<bool(std::size_t)>& cashflowRelevant, const CrossAssetModel& model,
                    const RegressorModel regressorModel, const Real regressionVarianceCutoff = Null<Real>(),
                    const Size regressionMaxSimTimesIr = 0, const Size regressionMaxSimTimesFx = 0,
                    const Size regressionMaxSimTimesEq = 0,
                    const VarGroupMode regressionVarGroupMode = VarGroupMode::Global);
    // pathTimes must contain the observation time and the relevant cashflow simulation times
    void train(const Size polynomOrder, const LsmBasisSystem::PolynomialType polynomType,
               const RandomVariable& regressand, const std::vector<std::vector<const RandomVariable*>>& paths,
               const std::set<Real>& pathTimes, const Filter& filter = Filter());
    // pathTimes do not need to contain the observation time or the relevant cashflow simulation times
    RandomVariable apply(const Array& initialState, const std::vector<std::vector<const RandomVariable*>>& paths,
                         const std::set<Real>& pathTimes) const;
    // is this model initialized and trained?
    bool isTrained() const { return isTrained_; }

private:
    Real observationTime_ = Null<Real>();
    Real regressionVarianceCutoff_ = Null<Real>();
    bool isTrained_ = false;
    std::set<std::pair<Real, Size>> regressorTimesModelIndices_;
    Matrix coordinateTransform_;
    std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>> basisFns_ =
        std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>{};
    Array regressionCoeffs_;

    Size basisDim_ = 0;
    Size basisOrder_ = 0;
    LsmBasisSystem::PolynomialType basisType_ = LsmBasisSystem::PolynomialType::Monomial;
    Size basisSystemSizeBound_ = Null<Size>();
    std::set<std::set<Size>> varGroups_ = {};

    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};
} // namespace QuantExt