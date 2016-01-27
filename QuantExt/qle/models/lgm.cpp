/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/lgm.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

namespace QuantExt {

Lgm::Lgm(const boost::shared_ptr<IrLgm1fParametrization> &parametrization)
    : parametrization_(parametrization) {
    stateProcess_ = boost::make_shared<IrLgm1fStateProcess>(parametrization_);
    arguments_.resize(2);
    arguments_[0] = parametrization_->parameter(0);
    arguments_[1] = parametrization_->parameter(1);
}

} // namespace QuantExt
