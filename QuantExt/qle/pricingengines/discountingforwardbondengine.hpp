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

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/period.hpp>

#include <qle/instruments/forwardbond.hpp>

#include <ql/tuple.hpp>

namespace QuantExt {

//! Discounting Forward Bond Engine

/*!
  \ingroup engines
*/
class DiscountingForwardBondEngine : public QuantExt::ForwardBond::engine {
public:
    DiscountingForwardBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                 const Handle<YieldTermStructure>& incomeCurve,
                                 const Handle<YieldTermStructure>& bondReferenceYieldCurve,
                                 const Handle<Quote>& bondSpread,
                                 const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                 const Handle<Quote>& recoveryRate, Period timestepPeriod,
                                 boost::optional<bool> includeSettlementDateFlows = boost::none,
                                 const Date& settlementDate = Date(), const Date& npvDate = Date());

    void calculate() const override;
    Real calculateBondNpv(Date, Date) const;
    QuantLib::ext::tuple<Real, Real> calculateForwardContractPresentValue(Real spotValue, Real cmpPayment, Date npvDate,
                                                                  Date computeDate, Date settlementDate,
                                                                  bool cashSettlement, Date cmpPaymentDate,
                                                                  bool dirty) const;

    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }
    const Handle<YieldTermStructure>& incomeCurve() const { return incomeCurve_; }
    const Handle<YieldTermStructure>& bondReferenceYieldCurve() const { return bondReferenceYieldCurve_; }
    const Handle<Quote>& bondSpread() const { return bondSpread_; }
    const Handle<DefaultProbabilityTermStructure>& bondDefaultCurve() const { return bondDefaultCurve_; }
    const Handle<Quote>& bondRecoveryRate() const { return bondRecoveryRate_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<YieldTermStructure> incomeCurve_;
    Handle<YieldTermStructure> bondReferenceYieldCurve_;
    Handle<Quote> bondSpread_;
    Handle<DefaultProbabilityTermStructure> bondDefaultCurve_;
    Handle<Quote> bondRecoveryRate_;
    Period timestepPeriod_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_;
    Date npvDate_;
};
} // namespace QuantExt

#endif
