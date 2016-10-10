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

#include <qle/models/lgm.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

namespace QuantExt {

LinearGaussMarkovModel::LinearGaussMarkovModel(const boost::shared_ptr<IrLgm1fParametrization>& parametrization)
    : parametrization_(parametrization) {
    stateProcess_ = boost::make_shared<IrLgm1fStateProcess>(parametrization_);
    arguments_.resize(2);
    arguments_[0] = parametrization_->parameter(0);
    arguments_[1] = parametrization_->parameter(1);
    registerWith(parametrization_->termStructure());
}

} // namespace QuantExt
