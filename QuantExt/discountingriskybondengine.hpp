#pragma once
/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/discountingriskybondengine.hpp
    \brief Risky Bond Engine

    \ingroup engines
*/

#ifndef quantext_discounting_riskybond_engine_hpp
#define quantext_discounting_riskybond_engine_hpp

#include <ql/instruments/bond.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

class DiscountingRiskyBondEngine : public QuantLib::Bond::engine {
public:
    DiscountingBondEngine(const Handle<YieldTermStructure>& discountCurve, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                          const Handle<Quote>& securitySpread, boost::optional<bool> includeSettlementDateFlows = boost::none);
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<DefaultProbabilityTermStructure> discountCurve_;
    const Handle<Quote> securitySpread_;
    boost::optional<bool> includeSettlementDateFlows_;
};

}

#endif
