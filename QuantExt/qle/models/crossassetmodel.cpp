/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/pseudoparameter.hpp>

#include <ql/experimental/math/piecewiseintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/processes/eulerdiscretization.hpp>

namespace QuantExt {

CrossAssetModel::CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization> >& parametrizations,
                                 const Matrix& correlation, SalvagingAlgorithm::Type salvaging)
    : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation), salvaging_(salvaging) {
    initialize();
}

CrossAssetModel::CrossAssetModel(const std::vector<boost::shared_ptr<LinearGaussMarkovModel> >& currencyModels,
                                 const std::vector<boost::shared_ptr<FxBsParametrization> >& fxParametrizations,
                                 const Matrix& correlation, SalvagingAlgorithm::Type salvaging)
    : LinkableCalibratedModel(), lgm_(currencyModels), rho_(correlation), salvaging_(salvaging) {
    for (Size i = 0; i < currencyModels.size(); ++i) {
        p_.push_back(currencyModels[i]->parametrization());
    }
    for (Size i = 0; i < fxParametrizations.size(); ++i) {
        p_.push_back(fxParametrizations[i]);
    }
    initialize();
}

Size CrossAssetModel::components(const AssetType t) const {
    switch (t) {
    case IR:
        return nIrLgm1f_;
        break;
    case FX:
        return nFxBs_;
        break;
    default:
        QL_FAIL("asset class " << t << " not known.");
    }
}

Size CrossAssetModel::ccyIndex(const Currency& ccy) const {
    Size i = 0;
    // FIXME: remove try/catch
    try {
        // irlgm1f() will throw if out of bounds
        while (irlgm1f(i)->currency() != ccy)
            ++i;
        return i;
    } catch (...) {
        QL_FAIL("currency " << ccy.code() << " not present in cross asset model");
    }
}

void CrossAssetModel::update() {
    for (Size i = 0; i < p_.size(); ++i) {
        p_[i]->update();
    }
    stateProcessExact_->flushCache();
    stateProcessEuler_->flushCache();
    notifyObservers();
}

void CrossAssetModel::generateArguments() { update(); }

Size CrossAssetModel::brownians(const AssetType t, const Size) const {
    switch (t) {
    case IR:
        return 1;
    case FX:
        return 1;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::stateVariables(const AssetType t, const Size) const {
    switch (t) {
    case IR:
        return 1;
    case FX:
        return 1;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::arguments(const AssetType t, const Size) const {
    switch (t) {
    case IR:
        return 2;
    case FX:
        return 1;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::idx(const AssetType t, const Size i) const {
    switch (t) {
    case IR:
        QL_REQUIRE(i < nIrLgm1f_, "ir index (" << i << ") must be in 0..." << (nIrLgm1f_ - 1));
        return i;
    case FX:
        QL_REQUIRE(nFxBs_ > 0, "fx index (" << i << ") invalid, no fx components");
        QL_REQUIRE(i < nFxBs_, "fx index (" << i << ") must be in 0..." << (nFxBs_ - 1));
        return nIrLgm1f_ + i;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::cIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < brownians(t, i), "c-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << brownians(t, i) - 1);
    // the return values below assume specific models and have to be
    // generalized when other model types are added
    switch (t) {
    case IR:
        QL_REQUIRE(i < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..." << (nIrLgm1f_ - 1));
        return i;
    case FX:
        QL_REQUIRE(nFxBs_ > 0, "fx index (" << i << ") invalid, no fx components");
        QL_REQUIRE(i < nFxBs_, "fxbs index (" << i << ") must be in 0..." << (nFxBs_ - 1));
        return nIrLgm1f_ + i;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::pIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < stateVariables(t, i), "p-offset (" << offset << ") for asset class " << t << " and index " << i
                                                           << " must be in 0..." << stateVariables(t, i) - 1);
    // the return values below assume specific models and have to be
    // generalized when other model types are added
    switch (t) {
    case IR:
        QL_REQUIRE(i < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..." << (nIrLgm1f_ - 1));
        return i;
    case FX:
        QL_REQUIRE(nFxBs_ > 0, "fx index (" << i << ") invalid, no fx components");
        QL_REQUIRE(i < nFxBs_, "fxbs index (" << i << ") must be in 0..." << (nFxBs_ - 1));
        return nIrLgm1f_ + i;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

Size CrossAssetModel::aIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < arguments(t, i), "a-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << arguments(t, i) - 1);
    // the return values below assume specific models and have to be
    // generalized when other model types are added
    switch (t) {
    case IR:
        QL_REQUIRE(i < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..." << (nIrLgm1f_ - 1));
        return 2 * i + offset;
    case FX:
        QL_REQUIRE(nFxBs_ > 0, "fx index (" << i << ") invalid, no fx components");
        QL_REQUIRE(i < nFxBs_, "fxbs index (" << i << ") must be in 0..." << (nFxBs_ - 1));
        return 2 * nIrLgm1f_ + i;
    default:
        QL_FAIL("EQ, COM not yet supported or type (" << t << ") unknown");
    }
}

const Real& CrossAssetModel::correlation(const AssetType s, const Size i, const AssetType t, const Size j,
                                         const Size iOffset, const Size jOffset) const {
    return rho_(cIdx(s, i, iOffset), cIdx(t, j, jOffset));
}

void CrossAssetModel::correlation(const AssetType s, const Size i, const AssetType t, const Size j, const Real value,
                                  const Size iOffset, const Size jOffset) {
    Size row = cIdx(s, i, iOffset);
    Size column = cIdx(t, j, jOffset);
    QL_REQUIRE(row != column || close_enough(value, 1.0), "correlation must be 1 at (" << row << "," << column << ")");
    QL_REQUIRE(value >= -1.0 && value <= 1.0, "correlation must be in [-1,1] at (" << row << "," << column << ")");
    // we can not check for non-negative eigenvalues, since we do not
    // know when the correlation matrix setup is finished, but this
    // is effectively one in the state process later on anyway and
    // the user can also use checkCorrelationMatrix() to verify this
    rho_(row, column) = rho_(column, row) = value;
    update();
}

void CrossAssetModel::initialize() {
    initializeParametrizations();
    initializeCorrelation();
    initializeArguments();
    finalizeArguments();
    checkModelConsistency();
    initDefaultIntegrator();
    initStateProcess();
}

void CrossAssetModel::initDefaultIntegrator() {
    setIntegrationPolicy(boost::make_shared<SimpsonIntegral>(1.0E-8, 100), true);
}

void CrossAssetModel::initStateProcess() {
    stateProcessEuler_ = boost::make_shared<CrossAssetStateProcess>(this, CrossAssetStateProcess::euler, salvaging_);
    stateProcessExact_ = boost::make_shared<CrossAssetStateProcess>(this, CrossAssetStateProcess::exact, salvaging_);
}

void CrossAssetModel::setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
                                           const bool usePiecewiseIntegration) const {

    if (!usePiecewiseIntegration) {
        integrator_ = integrator;
        return;
    }

    // collect relevant times from parametrizations
    // we don't have to sort them or make them unique,
    // this is all done in PiecewiseIntegral for us

    std::vector<Time> allTimes;
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        allTimes.insert(allTimes.end(), p_[idx(IR, i)]->parameterTimes(0).begin(),
                        p_[idx(IR, i)]->parameterTimes(0).end());
        allTimes.insert(allTimes.end(), p_[idx(IR, i)]->parameterTimes(1).begin(),
                        p_[idx(IR, i)]->parameterTimes(1).end());
    }
    for (Size i = 0; i < nFxBs_; ++i) {
        allTimes.insert(allTimes.end(), p_[idx(FX, i)]->parameterTimes(0).begin(),
                        p_[idx(FX, i)]->parameterTimes(0).end());
    }

    // use piecewise integrator avoiding the step points
    integrator_ = boost::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

void CrossAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    nIrLgm1f_ = 0;
    nFxBs_ = 0;

    Size i = 0;

    bool genericCtor = lgm_.empty();
    while (i < p_.size() && boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]) != NULL) {
        // initialize model, if generic constructor was used
        if (genericCtor) {
            lgm_.push_back(
                boost::make_shared<LinearGaussMarkovModel>(boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])));
        }
        // count things
        ++nIrLgm1f_;
        ++i;
    }

    while (i < p_.size() && boost::dynamic_pointer_cast<FxBsParametrization>(p_[i]) != NULL) {
        ++nFxBs_;
        ++i;
    }

    QL_REQUIRE(nIrLgm1f_ > 0, "at least one ir parametrization must be given");

    QL_REQUIRE(nFxBs_ == nIrLgm1f_ - 1, "there must be n-1 fx "
                                        "for n ir parametrizations, found "
                                            << nIrLgm1f_ << " ir and " << nFxBs_ << " fx parametrizations");

    // check currencies

    // without an order or a hash function on Currency this seems hard
    // to do in a simpler way ...
    Size uniqueCurrencies = 0;
    std::vector<Currency> currencies;
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        Size tmp = 1;
        for (Size j = 0; j < i; ++j) {
            if (irlgm1f(i)->currency() == currencies[j])
                tmp = 0;
        }
        uniqueCurrencies += tmp;
        currencies.push_back(irlgm1f(i)->currency());
    }
    QL_REQUIRE(uniqueCurrencies == nIrLgm1f_, "there are duplicate currencies "
                                              "in the set of irlgm1f "
                                              "parametrizations");
    for (Size i = 0; i < nFxBs_; ++i) {
        QL_REQUIRE(fxbs(i)->currency() == irlgm1f(i + 1)->currency(),
                   "fx parametrization #" << i << " must be for currency of ir parametrization #" << (i + 1)
                                          << ", but they are " << fxbs(i)->currency() << " and "
                                          << irlgm1f(i + 1)->currency() << " respectively");
    }
}

void CrossAssetModel::initializeCorrelation() {
    Size n = nIrLgm1f_ + nFxBs_;
    if (rho_.empty()) {
        rho_ = Matrix(n, n, 0.0);
        for (Size i = 0; i < n; ++i)
            rho_(i, i) = 1.0;
        return;
    }
    QL_REQUIRE(rho_.rows() == n && rho_.columns() == n, "correlation matrix is " << rho_.rows() << " x "
                                                                                 << rho_.columns() << " but should be "
                                                                                 << n << " x " << n);
    checkCorrelationMatrix();
}

void CrossAssetModel::checkCorrelationMatrix() const {
    Size n = rho_.rows();
    Size m = rho_.columns();
    QL_REQUIRE(rho_.columns() == n, "correlation matrix (" << n << " x " << m << " must be square");
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            QL_REQUIRE(close_enough(rho_[i][j], rho_[j][i]), "correlation matrix is no symmetric, for (i,j)=("
                                                                 << i << "," << j << ") rho(i,j)=" << rho_[i][j]
                                                                 << " but rho(j,i)=" << rho_[j][i]);
            QL_REQUIRE(rho_[i][j] >= -1.0 && rho_[i][j] <= 1.0, "correlation matrix has invalid entry at (i,j)=("
                                                                    << i << "," << j << ") equal to " << rho_[i][j]);
        }
        QL_REQUIRE(close_enough(rho_[i][i], 1.0), "correlation matrix must have unit diagonal elements, "
                                                  "but rho(i,i)="
                                                      << rho_[i][i] << " for i=" << i);
    }

    SymmetricSchurDecomposition ssd(rho_);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0, "correlation matrix has negative eigenvalue at "
                                                    << i << " (" << ssd.eigenvalues()[i] << ")");
    }
}

void CrossAssetModel::initializeArguments() {

    arguments_.resize(2 * nIrLgm1f_ + nFxBs_);
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        // volatility
        arguments_[aIdx(IR, i, 0)] = irlgm1f(i)->parameter(0);
        // reversion
        arguments_[aIdx(IR, i, 1)] = irlgm1f(i)->parameter(1);
    }
    for (Size i = 0; i < nFxBs_; ++i) {
        // volatility
        arguments_[aIdx(FX, i, 0)] = fxbs(i)->parameter(0);
    }
}

void CrossAssetModel::finalizeArguments() {

    totalNumberOfParameters_ = 0;
    for (Size i = 0; i < arguments_.size(); ++i) {
        QL_REQUIRE(arguments_[i] != NULL, "unexpected error: argument " << i << " is null");
        totalNumberOfParameters_ += arguments_[i]->size();
    }
}

void CrossAssetModel::checkModelConsistency() const {
    QL_REQUIRE(nIrLgm1f_ > 0, "at least one IR component must be given");
    QL_REQUIRE(nIrLgm1f_ + nFxBs_ == p_.size(), "the parametrizations must be given in the following order: ir, "
                                                "fx (others not supported by this class), found "
                                                    << nIrLgm1f_ << " ir and " << nFxBs_
                                                    << " bs parametrizations, but there are " << p_.size()
                                                    << " parametrizations given in total");
}

void CrossAssetModel::calibrateIrLgm1fVolatilitiesIterative(
    const Size ccy, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateVolatilitiesIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fReversionsIterative(
    const Size ccy, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateReversionsIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fGlobal(const Size ccy,
                                             const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrate(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateFxBsVolatilitiesIterative(
    const Size ccy, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveFxBsVolatility(ccy, i));
    }
    update();
}

} // namespace QuantExt
