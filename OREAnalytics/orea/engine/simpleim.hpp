/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file orea/engine/simpleim.hpp
    \brief simple im model for dynamic im calculations
*/

#pragma once

#include <qle/math/randomvariable.hpp>

namespace ore {
namespace analytics {

QuantExt::RandomVariable simpleIM(const std::vector<QuantExt::RandomVariable>& irDelta,
                                  const std::vector<std::vector<QuantExt::RandomVariable>>& irVega,
                                  const std::vector<QuantExt::RandomVariable>& fxDelta,
                                  const std::vector<std::vector<QuantExt::RandomVariable>>& fxVega);

} // namespace analytics
} // namespace ore
