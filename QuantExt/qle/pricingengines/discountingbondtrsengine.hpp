/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
    \brief Engine to value a Forwadr Bond contract

    \ingroup engines
*/

#ifndef quantext_discounting_bond_trs_engine_hpp
#define quantext_discounting_bond_trs_engine_hpp

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <ql/time/period.hpp>
#include <qle/instruments/bondtotalreturnswap.hpp>

namespace QuantExt {

//! Discounting Forward Bond Engine

/*!
  \ingroup engines
*/
class DiscountingBondTRSEngine : public QuantExt::BondTRS::engine {
public:
    DiscountingBondTRSEngine(const Handle<YieldTermStructure>&, const Handle<YieldTermStructure>&, const Handle<Quote>&,
                             const Handle<DefaultProbabilityTermStructure>&, const Handle<Quote>&, Period,
                             boost::optional<bool> includeSettlementDateFlows = boost::none);

    void calculate() const;
    Real calculateBondNpv(Date, Date, Date, Handle<YieldTermStructure>) const;
    Real calculateFundingLegNpv(Date, Date, Date) const;
    Real calculatecompensationPaymentsNpv(Date, Date, Date) const;

    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }
    const Handle<YieldTermStructure>& bondReferenceYieldCurve() const { return bondReferenceYieldCurve_; }
    const Handle<Quote>& bondSpread() const { return bondSpread_; }
    const Handle<DefaultProbabilityTermStructure>& bondDefaultCurve() const { return bondDefaultCurve_; }
    const Handle<Quote>& bondRecoveryRate() const { return bondRecoveryRate_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<YieldTermStructure> bondReferenceYieldCurve_;
    Handle<Quote> bondSpread_;
    Handle<DefaultProbabilityTermStructure> bondDefaultCurve_;
    Handle<Quote> bondRecoveryRate_;
    Period timestepPeriod_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date npvDate_;
};
} // namespace QuantExt

#endif
