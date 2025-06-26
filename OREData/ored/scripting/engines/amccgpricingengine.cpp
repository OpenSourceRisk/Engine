/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/scripting/engines/amccgpricingengine.hpp>

namespace ore {
namespace data {

void scale(QuantExt::ComputationGraph& g, TradeExposure& t, double multiplier) {
    for(auto& c: t.componentPathValues) {
        c = cg_mult(g, c, cg_const(g, multiplier));
    }
    if(t.targetConditionalExpectation != QuantExt::ComputationGraph::nan) {
        t.targetConditionalExpectation = cg_mult(g, t.targetConditionalExpectation, cg_const(g, multiplier));
    }
        
}

} // namespace data
} // namespace ore
