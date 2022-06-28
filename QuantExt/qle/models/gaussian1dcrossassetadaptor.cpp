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

#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

Gaussian1dCrossAssetAdaptor::Gaussian1dCrossAssetAdaptor(const boost::shared_ptr<LinearGaussMarkovModel>& model)
    : Gaussian1dModel(model->parametrization()->termStructure()), x_(model) {
    initialize();
}

Gaussian1dCrossAssetAdaptor::Gaussian1dCrossAssetAdaptor(Size ccy, const boost::shared_ptr<CrossAssetModel>& model)
    : Gaussian1dModel(model->irlgm1f(ccy)->termStructure()), x_(model->lgm(ccy)) {
    initialize();
}

void Gaussian1dCrossAssetAdaptor::initialize() {
    registerWith(x_);
    stateProcess_ = x_->stateProcess();
}

} // namespace QuantExt
