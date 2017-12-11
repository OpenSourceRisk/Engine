/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/pseudoparameter.hpp>

#include <ql/experimental/math/piecewiseintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/processes/eulerdiscretization.hpp>

using namespace QuantExt::CrossAssetAnalytics;

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
    case INF:
        return nInfDk_;
        break;
    case CR:
        return nCrLgm1f_;
        break;
    case EQ:
        return nEqBs_;
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

Size CrossAssetModel::eqIndex(const std::string& name) const {
    Size i = 0;
    // FIXME: remove try/catch
    try {
        while (eqbs(i)->eqName() != name)
            ++i;
        return i;
    } catch (...) {
        QL_FAIL("equity name " << name << " not present in cross asset model");
    }
}

Size CrossAssetModel::infIndex(const std::string& index) const {
    Size i = 0;
    // FIXME: remove try/catch
    try {
        while (infdk(i)->name() != index)
            ++i;
        return i;
    } catch (...) {
        QL_FAIL("inflation index " << index << " not present in cross asset model");
    }
}

void CrossAssetModel::update() {
    cache_crlgm1fS_.clear();
    cache_infdkI_.clear();
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
    case INF:
        return 1;
    case CR:
        return 1;
    case EQ:
        return 1;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
    }
}

Size CrossAssetModel::stateVariables(const AssetType t, const Size) const {
    switch (t) {
    case IR:
        return 1;
    case FX:
        return 1;
    case INF:
        return 2;
    case CR:
        return 2;
    case EQ:
        return 1;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
    }
}

Size CrossAssetModel::arguments(const AssetType t, const Size) const {
    switch (t) {
    case IR:
        return 2;
    case FX:
        return 1;
    case INF:
        return 2;
    case CR:
        return 2;
    case EQ:
        return 1;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
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
    case INF:
        QL_REQUIRE(nInfDk_ > 0, "inf index (" << i << ") invalid, no inf components");
        QL_REQUIRE(i < nInfDk_, "inf index (" << i << ") must be in 0..." << (nInfDk_ - 1));
        return nIrLgm1f_ + nFxBs_ + i;
    case CR:
        QL_REQUIRE(nCrLgm1f_ > 0, "cr index (" << i << ") invalid, no cr components");
        QL_REQUIRE(i < nCrLgm1f_, "crlgm1f index (" << i << ") must be in 0..." << (nCrLgm1f_ - 1));
        return nIrLgm1f_ + nFxBs_ + nInfDk_ + i;
    case EQ:
        QL_REQUIRE(nEqBs_ > 0, "eq index (" << i << ") invalid, no eq components");
        QL_REQUIRE(i < nEqBs_, "eq index (" << i << ") must be in 0..." << (nEqBs_ - 1));
        return nIrLgm1f_ + nFxBs_ + nInfDk_ + nCrLgm1f_ + i;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
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
    case INF:
        QL_REQUIRE(nInfDk_ > 0, "inf index (" << i << ") invalid, no inf components");
        QL_REQUIRE(i < nInfDk_, "infdk index (" << i << ") must be in 0..." << (nInfDk_ - 1));
        return nIrLgm1f_ + nFxBs_ + i;
    case CR:
        QL_REQUIRE(nCrLgm1f_ > 0, "cr index (" << i << ") invalid, no cr components");
        QL_REQUIRE(i < nCrLgm1f_, "crlgm1f index (" << i << ") must be in 0..." << (nCrLgm1f_ - 1));
        return nIrLgm1f_ + nFxBs_ + nInfDk_ + i;
    case EQ:
        QL_REQUIRE(nEqBs_ > 0, "eq index (" << i << ") invalid, no eq components");
        QL_REQUIRE(i < nEqBs_, "eqbs index (" << i << ") must be in 0..." << (nEqBs_ - 1));
        return nIrLgm1f_ + nFxBs_ + nInfDk_ + nCrLgm1f_ + i;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
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
    case INF:
        QL_REQUIRE(nInfDk_ > 0, "inf index (" << i << ") invalid, no inf components");
        QL_REQUIRE(i < nInfDk_, "infdk index (" << i << ") must be in 0..." << (nInfDk_ - 1));
        return nIrLgm1f_ + nFxBs_ + 2 * i + offset;
    case CR:
        QL_REQUIRE(nCrLgm1f_ > 0, "cr index (" << i << ") invalid, no cr components");
        QL_REQUIRE(i < nCrLgm1f_, "crlgm1f index (" << i << ") must be in 0..." << (nCrLgm1f_ - 1));
        return nIrLgm1f_ + nFxBs_ + 2 * nInfDk_ + 2 * i + offset;
    case EQ:
        QL_REQUIRE(nEqBs_ > 0, "eq index (" << i << ") invalid, no eq components");
        QL_REQUIRE(i < nEqBs_, "eqbs index (" << i << ") must be in 0..." << (nEqBs_ - 1));
        return nIrLgm1f_ + nFxBs_ + 2 * nInfDk_ + 2 * nCrLgm1f_ + i;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
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
    case INF:
        QL_REQUIRE(nInfDk_ > 0, "inf index (" << i << ") invalid, no inf components");
        QL_REQUIRE(i < nInfDk_, "infdk index (" << i << ") must be in 0..." << (nInfDk_ - 1));
        return 2 * nIrLgm1f_ + nFxBs_ + 2 * i + offset;
    case CR:
        QL_REQUIRE(nCrLgm1f_ > 0, "cr index (" << i << ") invalid, no cr components");
        QL_REQUIRE(i < nCrLgm1f_, "crlgm1f idnex (" << i << ") must be in 0...1" << (nCrLgm1f_ - 1));
        return 2 * nIrLgm1f_ + nFxBs_ + 2 * nInfDk_ + 2 * i + offset;
    case EQ:
        QL_REQUIRE(nEqBs_ > 0, "eq index (" << i << ") invalid, no eq components");
        QL_REQUIRE(i < nEqBs_, "crlgm1f idnex (" << i << ") must be in 0...1" << (nEqBs_ - 1));
        return 2 * nIrLgm1f_ + nFxBs_ + 2 * nInfDk_ + 2 * nCrLgm1f_ + i;
    default:
        QL_FAIL("asset type (" << t << ") unknown");
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

    // collect relevant times from parametrizations, we don't have to sort them or make them unique,
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
    for (Size i = 0; i < nInfDk_; ++i) {
        allTimes.insert(allTimes.end(), p_[idx(INF, i)]->parameterTimes(0).begin(),
                        p_[idx(INF, i)]->parameterTimes(0).end());
        allTimes.insert(allTimes.end(), p_[idx(INF, i)]->parameterTimes(1).begin(),
                        p_[idx(INF, i)]->parameterTimes(1).end());
    }
    for (Size i = 0; i < nCrLgm1f_; ++i) {
        allTimes.insert(allTimes.end(), p_[idx(CR, i)]->parameterTimes(0).begin(),
                        p_[idx(CR, i)]->parameterTimes(0).end());
        allTimes.insert(allTimes.end(), p_[idx(CR, i)]->parameterTimes(1).begin(),
                        p_[idx(CR, i)]->parameterTimes(1).end());
    }
    for (Size i = 0; i < nEqBs_; ++i) {
        allTimes.insert(allTimes.end(), p_[idx(EQ, i)]->parameterTimes(0).begin(),
                        p_[idx(EQ, i)]->parameterTimes(0).end());
    }

    // use piecewise integrator avoiding the step points
    integrator_ = boost::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

void CrossAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    nIrLgm1f_ = 0;
    nFxBs_ = 0;
    nInfDk_ = 0;
    nCrLgm1f_ = 0;
    nEqBs_ = 0;

    Size i = 0;

    // IR parametrizations

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

    // FX parametrizations

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

    // Inf parametrizations

    while (i < p_.size() && boost::dynamic_pointer_cast<InfDkParametrization>(p_[i]) != NULL) {
        ++nInfDk_;
        ++i;
        // we do not check the currency, if not present among the model's
        // currencies, it will throw below
    }

    // Cr parametrizations

    boost::shared_ptr<CrLgm1fParametrization> p;
    while (i < p_.size() && (p = boost::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i])) != NULL) {
        // we really don't mind the currency, don't need a check for this,
        // because it is in the end specified in crlgm1fS() and only this
        // is used
        // QL_REQUIRE(p->currency() == irlgm1f(0)->currency(),
        //            "credit currency at " << nCrLgm1f_ << " (" << p->currency()
        //                                  << ") is not domestic ("
        //                                  << irlgm1f(0)->currency() << ")");
        ++nCrLgm1f_;
        ++i;
    }

    // Eq parametrizations

    while (i < p_.size() && boost::dynamic_pointer_cast<EqBsParametrization>(p_[i]) != NULL) {
        ++nEqBs_;
        ++i;
    }
    // check the equity currencies to ensure they are covered by CrossAssetModel
    for (Size i = 0; i < nEqBs_; ++i) {
        Currency eqCcy = eqbs(i)->currency();
        try {
            Size eqCcyIdx = ccyIndex(eqCcy);
            QL_REQUIRE(eqCcyIdx < nIrLgm1f_, "Invalid currency for equity " << eqbs(i)->eqName());
        } catch (...) {
            QL_FAIL("Invalid currency (" << eqCcy.code() << ") for equity " << eqbs(i)->eqName());
        }
    }

} // initParametrizations

void CrossAssetModel::initializeCorrelation() {
    Size n = nIrLgm1f_ + nFxBs_ + nInfDk_ + nCrLgm1f_ + nEqBs_;
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
            QL_REQUIRE(close_enough(rho_[i][j], rho_[j][i]), "correlation matrix is not symmetric, for (i,j)=("
                                                                 << i << "," << j << ") rho(i,j)=" << rho_[i][j]
                                                                 << " but rho(j,i)=" << rho_[j][i]);
            QL_REQUIRE(close_enough(std::abs(rho_[i][j]), 1.0) || (rho_[i][j] > -1.0 && rho_[i][j] < 1.0),
                       "correlation matrix has invalid entry at (i,j)=(" << i << "," << j << ") equal to "
                                                                         << rho_[i][j]);
        }
        QL_REQUIRE(close_enough(rho_[i][i], 1.0), "correlation matrix must have unit diagonal elements, "
                                                  "but rho(i,i)="
                                                      << rho_[i][i] << " for i=" << i);
    }

    SymmetricSchurDecomposition ssd(rho_);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0,
                   "correlation matrix has negative eigenvalue at " << i << " (" << ssd.eigenvalues()[i] << ")");
    }
}

void CrossAssetModel::initializeArguments() {

    arguments_.resize(2 * nIrLgm1f_ + nFxBs_ + 2 * nInfDk_ + 2 * nCrLgm1f_ + nEqBs_);

    // irlgm1f
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        // volatility
        arguments_[aIdx(IR, i, 0)] = irlgm1f(i)->parameter(0);
        // reversion
        arguments_[aIdx(IR, i, 1)] = irlgm1f(i)->parameter(1);
    }

    // fx bs
    for (Size i = 0; i < nFxBs_; ++i) {
        // volatility
        arguments_[aIdx(FX, i, 0)] = fxbs(i)->parameter(0);
    }

    // inflation dk
    for (Size i = 0; i < nInfDk_; ++i) {
        // volatility
        arguments_[aIdx(INF, i, 0)] = infdk(i)->parameter(0);
        // reversion
        arguments_[aIdx(INF, i, 1)] = infdk(i)->parameter(1);
    }

    // credit lgm1f
    for (Size i = 0; i < nCrLgm1f_; ++i) {
        // volatility
        arguments_[aIdx(CR, i, 0)] = crlgm1f(i)->parameter(0);
        // reversion
        arguments_[aIdx(CR, i, 1)] = crlgm1f(i)->parameter(1);
    }

    // eq bs
    for (Size i = 0; i < nEqBs_; ++i) {
        // volatility
        arguments_[aIdx(EQ, i, 0)] = eqbs(i)->parameter(0);
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
    QL_REQUIRE(nIrLgm1f_ + nFxBs_ + nInfDk_ + nCrLgm1f_ + nEqBs_ == p_.size(),
               "the parametrizations must be given in the following order: ir, "
               "fx, inf, cr, eq, found "
                   << nIrLgm1f_ << " ir, " << nFxBs_ << " bs, " << nInfDk_ << " inf, " << nCrLgm1f_ << " cr, " << nEqBs_
                   << " eq parametrizations, but there are " << p_.size() << " parametrizations given in total");
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

void CrossAssetModel::calibrateBsVolatilitiesIterative(
    const AssetType& assetType, const Size idx, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
    OptimizationMethod& method, const EndCriteria& endCriteria, const Constraint& constraint,
    const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == FX || assetType == EQ, "Unsupported AssetType for BS calibration");
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, idx, i));
    }
    update();
}

void CrossAssetModel::calibrateBsVolatilitiesGlobal(const AssetType& assetType, const Size aIdx,
                                                    const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                                    OptimizationMethod& method, const EndCriteria& endCriteria,
                                                    const Constraint& constraint, const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == FX || assetType == EQ, "Unsupported AssetType for BS calibration");
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, aIdx, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesIterative(
    const Size index, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(INF, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkReversionsIterative(
    const Size index, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(INF, 1, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesGlobal(
    const Size index, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(INF, 0, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkReversionsGlobal(const Size index,
                                                     const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                                     OptimizationMethod& method, const EndCriteria& endCriteria,
                                                     const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(INF, 1, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateCrLgm1fVolatilitiesIterative(
    const Size index, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(CR, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateCrLgm1fReversionsIterative(
    const Size index, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(CR, 1, index, i));
    }
    update();
}

std::pair<Real, Real> CrossAssetModel::infdkV(const Size i, const Time t, const Time T) {
    Size ccy = ccyIndex(infdk(i)->currency());
    cache_key k = { i, ccy, t, T };
    boost::unordered_map<cache_key, std::pair<Real, Real> >::const_iterator it = cache_infdkI_.find(k);
    Real V0, V_tilde;

    if (it == cache_infdkI_.end()) {
        V0 = infV(i, ccy, 0, t);
        V_tilde = infV(i, ccy, t, T) - infV(i, ccy, 0, T) + infV(i, ccy, 0, t);
        cache_infdkI_.insert(std::make_pair(k, std::make_pair(V0, V_tilde)));
    } else {
        // take V0 and V_tilde from cache
        V0 = it->second.first;
        V_tilde = it->second.second;
    }
    return std::make_pair(V0, V_tilde);
}

std::pair<Real, Real> CrossAssetModel::infdkI(const Size i, const Time t, const Time T, const Real z, const Real y) {
    QL_REQUIRE(t < T || close_enough(t, T), "infdkI: t (" << t << ") <= T (" << T << ") required");
    Real V0, V_tilde;
    std::pair<Real, Real> Vs = infdkV(i, t, T);
    V0 = Vs.first;
    V_tilde = Vs.second;
    Real Hyt = Hy(i).eval(this, t);
    Real HyT = Hy(i).eval(this, T);

    // lag computation
    Date baseDate = infdk(i)->termStructure()->baseDate();
    Frequency freq = infdk(i)->termStructure()->frequency();
    Real lag = inflationYearFraction(freq, infdk(i)->termStructure()->indexIsInterpolated(),
                                     irlgm1f(0)->termStructure()->dayCounter(), baseDate,
                                     infdk(i)->termStructure()->referenceDate());

    //    Period lag = infdk(i)->termStructure()->observationLag();

    // TODO account for seasonality ...
    // compute final results depending on z and y
    Real It = std::pow(1.0 + infdk(i)->termStructure()->zeroRate(t - lag), t) * std::exp(Hyt * z - y - V0);
    Real Itilde_t_T = std::pow(1.0 + infdk(i)->termStructure()->zeroRate(T - lag), T) /
                      std::pow(1.0 + infdk(i)->termStructure()->zeroRate(t - lag), t) *
                      std::exp((HyT - Hyt) * z + V_tilde);
    // concerning interpolation there is an inaccuracy here: if the index
    // is not interpolated, we still simulate the index value as of t
    // (and T), although we should go back to t, T which corresponds to
    // the last actual publication time of the index => is the approximation
    // here in this sense good enough that we can tolerate this?
    return std::make_pair(It, Itilde_t_T);
}

Real CrossAssetModel::infdkYY(const Size i, const Time t, const Time S, const Time T, const Real z, const Real y,
                              const Real irz) {
    Size ccy = ccyIndex(infdk(i)->currency());

    // Set Convexity adjustment set to 1.
    // TODO: Add calculation for DK convexity adjustment
    Real C_tilde = 1;

    Real I_tildeS = infdkI(i, t, S, z, y).second;
    Real I_tildeT = infdkI(i, t, T, z, y).second;
    Real Pn_t_T = lgm(ccy)->discountBond(t, T, irz);

    Real yySwaplet = (I_tildeT / I_tildeS) * Pn_t_T * C_tilde - Pn_t_T;

    return yySwaplet;
}

std::pair<Real, Real> CrossAssetModel::crlgm1fS(const Size i, const Size ccy, const Time t, const Time T, const Real z,
                                                const Real y) const {
    QL_REQUIRE(ccy < nIrLgm1f_, "ccy index (" << ccy << ") must be in 0..." << (nIrLgm1f_ - 1));
    QL_REQUIRE(t < T || close_enough(t, T), "crlgm1fS: t (" << t << ") <= T (" << T << ") required");
    cache_key k = { i, ccy, t, T };
    boost::unordered_map<cache_key, std::pair<Real, Real> >::const_iterator it = cache_crlgm1fS_.find(k);
    Real V0, V_tilde;
    Real Hlt = Hl(i).eval(this, t);
    Real HlT = Hl(i).eval(this, T);

    if (it == cache_crlgm1fS_.end()) {
        // compute V0 and V_tilde
        if (ccy == 0) {
            // domestic credit
            Real Hzt = Hz(0).eval(this, t);
            Real HzT = Hz(0).eval(this, T);
            Real zetal0 = zetal(i).eval(this, t);
            Real zetal1 = integral(this, P(Hl(i), al(i), al(i)), 0.0, t);
            Real zetal2 = integral(this, P(Hl(i), Hl(i), al(i), al(i)), 0.0, t);
            Real zetanl0 = integral(this, P(rzl(0, i), az(0), al(i)), 0.0, t);
            Real zetanl1 = integral(this, P(rzl(0, i), Hl(i), az(0), al(i)), 0.0, t);
            // opposite signs for last two terms in the book
            V0 = 0.5 * Hlt * Hlt * zetal0 - Hlt * zetal1 + 0.5 * zetal2 + Hzt * Hlt * zetanl0 - Hzt * zetanl1;
            V_tilde = -0.5 * (HlT * HlT - Hlt * Hlt) * zetal0 + (HlT - Hlt) * zetal1 -
                      (HzT * HlT - Hzt * Hlt) * zetanl0 + (HzT - Hzt) * zetanl1;
        } else {
            // foreign credit
            V0 = crV(i, ccy, 0, t);
            V_tilde = crV(i, ccy, t, T) - crV(i, ccy, 0, T) + crV(i, ccy, 0, t);
        }
        cache_crlgm1fS_.insert(std::make_pair(k, std::make_pair(V0, V_tilde)));
    } else {
        // take V0 and V_tilde from cache
        V0 = it->second.first;
        V_tilde = it->second.second;
    }
    // compute final results depending on z and y
    // opposite sign for V0 in the book
    Real St = crlgm1f(i)->termStructure()->survivalProbability(t) * std::exp(-Hlt * z + y - V0);
    Real Stilde_t_T = crlgm1f(i)->termStructure()->survivalProbability(T) /
                      crlgm1f(i)->termStructure()->survivalProbability(t) * std::exp(-(HlT - Hlt) * z + V_tilde);
    return std::make_pair(St, Stilde_t_T);
}

Real CrossAssetModel::infV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HyT = Hy(i).eval(this, T);
    Real HdT = irlgm1f(0)->H(T);
    Real rhody = correlation(IR, 0, INF, i, 0, 0);
    Real V;
    if (ccy == 0) {
        V = 0.5 * (HyT * HyT * (zetay(i).eval(this, T) - zetay(i).eval(this, t)) -
                   2.0 * HyT * integral(this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * HdT * (HyT * integral(this, P(az(0), ay(i)), t, T) - integral(this, P(az(0), Hy(i), ay(i)), t, T));
    } else {
        Real HfT = irlgm1f(ccy)->H(T);
        Real rhofy = correlation(IR, ccy, INF, i, 0, 0);
        Real rhoxy = correlation(FX, ccy - 1, INF, i, 0, 0);
        V = 0.5 * (HyT * HyT * (zetay(i).eval(this, T) - zetay(i).eval(this, t)) -
                   2.0 * HyT * integral(this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * (HyT * integral(this, P(Hz(0), az(0), ay(i)), t, T) -
                     integral(this, P(Hz(0), az(0), Hy(i), ay(i)), t, T)) -
            rhofy * (HfT * HyT * integral(this, P(az(ccy), ay(i)), t, T) -
                     HfT * integral(this, P(az(ccy), Hy(i), ay(i)), t, T) -
                     HyT * integral(this, P(Hz(ccy), az(ccy), ay(i)), t, T) +
                     integral(this, P(Hz(ccy), az(ccy), Hy(i), ay(i)), t, T)) +
            rhoxy * (HyT * integral(this, P(sx(ccy - 1), ay(i)), t, T) -
                     integral(this, P(sx(ccy - 1), Hy(i), ay(i)), t, T));
    }
    return V;
}

Real CrossAssetModel::crV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HlT = Hl(i).eval(this, T);
    Real HfT = Hz(ccy).eval(this, T);
    Real rhodl = correlation(IR, 0, CR, i, 0, 0);
    Real rhofl = correlation(IR, ccy, CR, i, 0, 0);
    Real rhoxl = correlation(FX, ccy - 1, CR, i, 0, 0);
    return 0.5 * (HlT * HlT * (zetal(i).eval(this, T) - zetal(i).eval(this, t)) -
                  2.0 * HlT * integral(this, P(Hl(i), al(i), al(i)), t, T) +
                  integral(this, P(Hl(i), Hl(i), al(i), al(i)), t, T)) +
           rhodl * (HlT * integral(this, P(Hz(0), az(0), al(i)), t, T) -
                    integral(this, P(Hz(0), az(0), Hl(i), al(i)), t, T)) +
           rhofl * (HfT * HlT * integral(this, P(az(ccy), al(i)), t, T) -
                    HfT * integral(this, P(az(ccy), Hl(i), al(i)), t, T) -
                    HlT * integral(this, P(Hz(ccy), az(ccy), al(i)), t, T) +
                    integral(this, P(Hz(ccy), az(ccy), Hl(i), al(i)), t, T)) -
           rhoxl *
               (HlT * integral(this, P(sx(ccy - 1), al(i)), t, T) - integral(this, P(sx(ccy - 1), Hl(i), al(i)), t, T));
}

} // namespace QuantExt
