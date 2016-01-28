/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

Gaussian1dCrossAssetAdaptor::Gaussian1dCrossAssetAdaptor(
    const boost::shared_ptr<LinearGaussMarkovModel> &model)
    : Gaussian1dModel(model->parametrization()->termStructure()), x_(model) {
    initialize();
}

Gaussian1dCrossAssetAdaptor::Gaussian1dCrossAssetAdaptor(
    Size ccy, const boost::shared_ptr<CrossAssetModel> &model)
    : Gaussian1dModel(model->irlgm1f(ccy)->termStructure()),
      x_(model->lgm(ccy)) {
    initialize();
}

void Gaussian1dCrossAssetAdaptor::initialize() {
    registerWith(x_);
    stateProcess_ = x_->stateProcess();
}

} // namespace QuantExt
