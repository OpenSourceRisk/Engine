/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file methods/brownianbridgepathinterpolator.hpp
    \brief brownian bridge path interpolator
    \ingroup models
*/

#pragma once

#include <qle/math/randomvariable.hpp>

namespace QuantExt {

/*! Input is

    - a vector of ascending times 0 < t1 < t2 < ... < tn
    - for a subset of these times a d-vector of N(0,1) variates that can be used to evolve a stochastic process
      of dimension d on this subset of the given times, this subset is required to be non-empty

    Here, the outer vector of the input variable variates refers to the times and the inner vector contains d random
    variables. The components of the random variables correspond to the monte carlo pathts. For times where initially no
    variates are given, the inner vector should be empty.

    After the function call, the variates vector contains N(0,1) variates for all times. These variates can be
    used to evolve the same stochastic process, but on the full time grid. The missing variates are interpolated using
    a brownian bridge.
*/

void interpolateVariatesWithBrownianBridge(const std::vector<QuantLib::Real>& times,
                                           std::vector<std::vector<QuantExt::RandomVariable>>& variates,
                                           const Size seed);

} // namespace QuantExt
