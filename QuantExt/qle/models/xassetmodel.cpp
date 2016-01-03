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

namespace QuantExt {

XAssetModel::XAssetModel(
    const std::vector<boost::shared_ptr<Parametrization> > &parametrizations,
    const Matrix &correlation)
    : CalibratedModel(0), p_(parametrizations), rho_(correlation) {
    initialize();
}

void XAssetModel::initialize() {

    initializeParametrizations();
    initializeArguments();
}

void XAssetModel::initializeParametrizations() {

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
}

void XAssetModel::initializeArguments() {

    for (Size i = 0; i < nIrLgm1f_; ++i) {
    }
}

} // namespace QuantExt
