/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/math/cumulativenormaldistribution.hpp>

namespace QuantExt {

CumulativeNormalDistribution::CumulativeNormalDistribution(Real average,
                                                           Real sigma)
    : average_(average), sigma_(sigma) {}

} // namespace QuantExt
