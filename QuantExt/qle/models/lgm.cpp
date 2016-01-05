/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/lgm.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

namespace QuantExt {

Lgm::Lgm(const boost::shared_ptr<IrLgm1fParametrization> &parametrization) {
    x_ = boost::make_shared<XAssetModel>(
        std::vector<boost::shared_ptr<Parametrization> >(1, parametrization),
        Matrix(1, 1, 1.0));
    stateProcess_ = boost::make_shared<IrLgm1fStateProcess>(parametrization);
}

} // namespace QuantExt
