/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file qle/pricingengines/discountingswapenginedelta.hpp
    \brief Swap engine providing analytical deltas for vanilla swaps

    \ingroup engines
*/

#ifndef quantext_discounting_swap_engine_delta_hpp
#define quantext_discounting_swap_engine_delta_hpp

#include <ql/handle.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

class DiscountingSwapEngineDelta : public Swap::engine {
public:
    DiscountingSwapEngineDelta(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                          const std::vector<Time>& deltaTimes = std::vector<Time>());
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    const std::vector<Real>& deltaTimes_;
};

} // namespace QuantExt

#endif
