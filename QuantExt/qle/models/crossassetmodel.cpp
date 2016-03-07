/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament
*/

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/pseudoparameter.hpp>

#include <ql/experimental/math/piecewiseintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/processes/eulerdiscretization.hpp>

namespace QuantExt {

CrossAssetModel::CrossAssetModel(
    const std::vector<boost::shared_ptr<Parametrization> > &parametrizations,
    const Matrix &correlation, SalvagingAlgorithm::Type salvaging)
    : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation),
      salvaging_(salvaging) {
    initialize();
}

CrossAssetModel::CrossAssetModel(
    const std::vector<boost::shared_ptr<LinearGaussMarkovModel> >
        &currencyModels,
    const std::vector<boost::shared_ptr<FxBsParametrization> >
        &fxParametrizations,
    const Matrix &correlation, SalvagingAlgorithm::Type salvaging)
    : LinkableCalibratedModel(), lgm_(currencyModels), rho_(correlation),
      salvaging_(salvaging) {
    for (Size i = 0; i < currencyModels.size(); ++i) {
        p_.push_back(currencyModels[i]->parametrization());
    }
    for (Size i = 0; i < fxParametrizations.size(); ++i) {
        p_.push_back(fxParametrizations[i]);
    }
    initialize();
}

void CrossAssetModel::initialize() {

    initializeParametrizations();
    initializeCorrelation();
    initializeArguments();
    finalizeArguments();
    checkModelConsistency();

    // set default integrator
    setIntegrationPolicy(boost::make_shared<SimpsonIntegral>(1.0E-8, 100),
                         true);

    // create state processes
    stateProcessEuler_ = boost::make_shared<CrossAssetStateProcess>(
        this, CrossAssetStateProcess::euler, salvaging_);
    stateProcessExact_ = boost::make_shared<CrossAssetStateProcess>(
        this, CrossAssetStateProcess::exact, salvaging_);
}

void CrossAssetModel::setIntegrationPolicy(
    const boost::shared_ptr<Integrator> integrator,
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
        allTimes.insert(allTimes.end(), p_[i]->parameterTimes(0).begin(),
                        p_[i]->parameterTimes(0).end());
        allTimes.insert(allTimes.end(), p_[i]->parameterTimes(1).begin(),
                        p_[i]->parameterTimes(1).end());
    }
    for (Size i = 0; i < nFxBs_; ++i) {
        allTimes.insert(allTimes.end(),
                        p_[nIrLgm1f_ + i]->parameterTimes(0).begin(),
                        p_[nIrLgm1f_ + i]->parameterTimes(0).end());
    }

    // use piecewise integrator avoiding the step points
    integrator_ =
        boost::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

void CrossAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    nIrLgm1f_ = 0;
    nFxBs_ = 0;
    nCrLgm1f_ = 0;

    Size i = 0;

    bool genericCtor = lgm_.empty();
    while (i < p_.size() &&
           boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]) != NULL) {
        // initialize model, if generic constructor was used
        if (genericCtor) {
            lgm_.push_back(boost::make_shared<LinearGaussMarkovModel>(
                boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])));
        }
        // count things
        ++nIrLgm1f_;
        ++i;
    }

    while (i < p_.size() &&
           boost::dynamic_pointer_cast<FxBsParametrization>(p_[i]) != NULL) {
        ++nFxBs_;
        ++i;
    }

    QL_REQUIRE(nIrLgm1f_ > 0, "at least one ir parametrization must be given");

    QL_REQUIRE(nFxBs_ == nIrLgm1f_ - 1, "there must be n-1 fx "
                                        "for n ir parametrizations, found "
                                            << nIrLgm1f_ << " ir and " << nFxBs_
                                            << " fx parametrizations");

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
                   "fx parametrization #"
                       << i << " must be for currency of ir parametrization #"
                       << (i + 1) << ", but they are " << fxbs(i)->currency()
                       << " and " << irlgm1f(i + 1)->currency()
                       << " respectively");
    }
}

void CrossAssetModel::initializeCorrelation() {

    QL_REQUIRE(rho_.rows() == nIrLgm1f_ + nFxBs_ + nCrLgm1f_ &&
                   rho_.columns() == nIrLgm1f_ + nFxBs_ + nCrLgm1f_,
               "correlation matrix is "
                   << rho_.rows() << " x " << rho_.columns()
                   << " but should be " << nIrLgm1f_ + nFxBs_ + nCrLgm1f_
                   << " x " << nIrLgm1f_ + nFxBs_ + nCrLgm1f_);

    for (Size i = 0; i < rho_.rows(); ++i) {
        for (Size j = 0; j < rho_.columns(); ++j) {
            QL_REQUIRE(close_enough(rho_[i][j], rho_[j][i]),
                       "correlation matrix is no symmetric, for (i,j)=("
                           << i << "," << j << ") rho(i,j)=" << rho_[i][j]
                           << " but rho(j,i)=" << rho_[j][i]);
            QL_REQUIRE(rho_[i][j] >= -1.0 && rho_[i][j] <= 1.0,
                       "correlation matrix has invalid entry at (i,j)=("
                           << i << "," << j << ") equal to " << rho_[i][j]);
        }
        QL_REQUIRE(close_enough(rho_[i][i], 1.0),
                   "correlation matrix must have unit diagonal elements, "
                   "but rho(i,i)="
                       << rho_[i][i] << " for i=" << i);
    }

    SymmetricSchurDecomposition ssd(rho_);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0,
                   "correlation matrix has negative eigenvalue at "
                       << i << " (" << ssd.eigenvalues()[i] << ")");
    }
}

void CrossAssetModel::initializeArguments() {

    arguments_.resize(2 * nIrLgm1f_ + nFxBs_);
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        // volatility
        arguments_[2 * i] = irlgm1f(i)->parameter(0);
        // reversion
        arguments_[2 * i + 1] = irlgm1f(i)->parameter(1);
    }
    for (Size i = 0; i < nFxBs_; ++i) {
        // volatility
        arguments_[nIrLgm1f_ * 2 + i] = fxbs(i)->parameter(0);
    }
}

void CrossAssetModel::finalizeArguments() {

    totalNumberOfParameters_ = 0;
    for (Size i = 0; i < arguments_.size(); ++i) {
        QL_REQUIRE(arguments_[i] != NULL, "unexpected error: argument "
                                              << i << " is null");
        totalNumberOfParameters_ += arguments_[i]->size();
    }
}

void CrossAssetModel::checkModelConsistency() const {
    QL_REQUIRE(nIrLgm1f_ + nFxBs_ == p_.size(),
               "the parametrizations must be given in the following order: ir, "
               "fx (others not supported by this class), found "
                   << nIrLgm1f_ << " ir and " << nFxBs_
                   << " bs parametrizations, but there are " << p_.size()
                   << " parametrizations given in total");
}

void CrossAssetModel::calibrateIrLgm1fVolatilitiesIterative(
    const Size ccy,
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    lgm(ccy)->calibrateVolatilitiesIterative(helpers, method, endCriteria,
                                             constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fReversionsIterative(
    const Size ccy,
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    lgm(ccy)->calibrateReversionsIterative(helpers, method, endCriteria,
                                           constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fGlobal(
    const Size ccy,
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    lgm(ccy)->calibrate(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateFxBsVolatilitiesIterative(
    const Size ccy,
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveFxBsVolatility(ccy, i));
    }
    update();
}

} // namespace QuantExt
