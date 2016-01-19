/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/processes/irlgm1fstateprocess.hpp>

namespace QuantExt {

IrLgm1fStateProcess::IrLgm1fStateProcess(
    const boost::shared_ptr<IrLgm1fParametrization> &parametrization)
    : p_(parametrization) {}

} // namespace QuantExt
