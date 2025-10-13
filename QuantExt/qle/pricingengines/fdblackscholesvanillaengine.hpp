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

/*! \file qle/pricingengines/fdblackscholesvanillaengine.hpp
    \brief Wrapper of QuantLib engine to allow cashflow results
*/

#pragma once

#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>

namespace QuantExt {

class FdBlackScholesVanillaEngine2 : public QuantLib::FdBlackScholesVanillaEngine {
public:
    using FdBlackScholesVanillaEngine::FdBlackScholesVanillaEngine;
    void calculate() const override;
};

} // namespace QuantExt
