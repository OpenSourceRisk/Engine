/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/discountingforwardbondengine.hpp
    \brief Engine to value a Forward Bond contract

    \ingroup engines
*/

#ifndef quantext_discounting_forward_bond_engine_hpp
#define quantext_discounting_forward_bond_engine_hpp

#include <qle/instruments/cashflowresults.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/period.hpp>

#include <qle/instruments/forwardbond.hpp>
#include <qle/pricingengines/forwardenabledbondengine.hpp>

#include <ql/tuple.hpp>

namespace QuantExt {

//! Discounting Forward Bond Engine

/*!
  \ingroup engines
*/
class DiscountingForwardBondEngine : public QuantExt::ForwardBond::engine, public QuantExt::ForwardEnabledBondEngine {
public:
    //! conversion factor is only used for deprecated representation of bond futures as bond forwards
    DiscountingForwardBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                 const Handle<Quote>& conversionFactor);

    void calculate() const override;

    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }
    const Handle<Quote>& conversionFactor() const { return conversionFactor_; }

    std::pair<QuantLib::Real, QuantLib::Real>
    forwardPrice(const QuantLib::Date& forwardNpvDate, const QuantLib::Date& settlementDate,
                 const bool conditionalOnSurvival = true, std::vector<CashFlowResults>* const cfResults = nullptr,
                 QuantLib::Leg* const expectedCashflows = nullptr) const override;

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<Quote> conversionFactor_;
};
} // namespace QuantExt

#endif
