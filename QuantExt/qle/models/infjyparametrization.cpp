/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/infjyparametrization.hpp>

namespace QuantExt {

InfJYParametrization::InfJYParametrization(
    const boost::shared_ptr<IrLgm1fParametrization> real,
    const boost::shared_ptr<FxBsParametrization> cpi)
    : Parametrization(real->currency()), real_(real), cpi_(cpi) {}

} // namespace QuantExt
