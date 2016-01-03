/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/models/xassetmodel.hpp>
#include <qle/models/pseudoparameter.hpp>

namespace QuantExt {

XAssetModel::XAssetModel(
    const std::vector<boost::shared_ptr<Parametrization> > &parametrizations,
    const Matrix &correlation)
    : CalibratedModel(0), p_(parametrizations), rho_(correlation) {
    initialize();
}

void XAssetModel::initialize() {

    initializeParametrizations();
    initializeCorrelation();
    initializeArguments();
}

void XAssetModel::initializeCorrelation() {

    QL_REQUIRE(rho_.rows() == nIrLgm1f_ + nFxBs_ &&
                   rho_.columns() == nIrLgm1f_ + nFxBs_,
               "correlation matrix is " << rho_.rows() << " x "
                                        << rho_.columns() << " but should be "
                                        << nIrLgm1f_ + nFxBs_ << " x "
                                        << nIrLgm1f_ + nFxBs_);

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
        QL_REQUIRE(
            close_enough(rho_[i][j], 1.0),
            "correlation matrix must have unit diagonal elements, but rho(i,i)="
                << rho_[i][i] << " for i=" << i);
    }

    SymmetricSchurDecomposition ssd(rho_);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0,
                   "correlation matrix has negative eigenvalue #"
                       << i << " (" << ssd.eigenvalues()[i] << ")");
    }
}

void XAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    nIrLgm1f_ = 0;
    nFxBs_ = 0;

    Size i = 0;

    while (i < p_.size() &&
           boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]) != NULL) {
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

    QL_REQUIRE(nIrLgm1f_ + nFxBs_ == p_.size(),
               "the parametrizations must be given in the following order: ir, "
               "fx (others not yet supported), found "
                   << nIrLgm1f_ << " ir and " << nFxBs_
                   << " parametrizations, but there are " << p_.size()
                   << " parametrizations given in total");

    // check currencies

    std::set<Currency> currencies;
    for (Size i = 0; i < nIrmLgm1f_; ++i) {
        currencies.add(irLgm1f(i)->currency());
    }
    QL_REQUIRE(currencies.size() == nIrLgm1f_, "there are duplicate currencies "
                                               "in the set of irlgm1f "
                                               "parametrizations");
    for (Size i = 0; i < nFxBs_; ++i) {
        QL_REQUIRE(fxbs(i)->currecy() == irlgm1f(i + 1)->currenc(),
                   "fx parametrization #"
                       << i << " must be for currency of ir parametrization #"
                       << (i + 1) << ", but they are " << fxbs(i)->currency()
                       << " and " << irlgm1f << " respectively");
    }
}

void XAssetModel::initializeArguments() {

    arguments_.resize(2 * nIrLgm1f_ + nFxBs_);
    for (Size i = 0; i < nIrLgm1f_; ++i) {
        // volatility
        arguments_[2 * i] = PseudoParameter();
        arguments_[2 * i].params() = irlgm1f(i)->rawValues(0);
        // reversion
        arguments_[2 * i + 1] = PseudoParameter();
        arguments_[2 * i].params() = irlgm1f(i)->rawValues(1);
    }
    for (Size i = 0; i < nFxBs_; ++i) {
        // volatility
        arguments_[i] = PseudoParameter();
        arguments_[i].params() = fxbs(i)->rawValues(0);
    }
}

} // namespace QuantExt
