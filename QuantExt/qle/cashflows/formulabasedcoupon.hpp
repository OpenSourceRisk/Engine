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

/*! \file qle/cashflows/formulabasedcoupon.hpp
    \brief formula based coupon
    \ingroup cashflows
*/

#pragma once

#include <qle/indexes/formulabasedindex.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/time/schedule.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! formula based coupon class
class FormulaBasedCoupon : public FloatingRateCoupon {
public:
    FormulaBasedCoupon(const Currency& paymentCurrency, const Date& paymentDate, Real nominal, const Date& startDate,
                       const Date& endDate, Natural fixingDays, const QuantLib::ext::shared_ptr<FormulaBasedIndex>& index,
                       const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                       const DayCounter& dayCounter = DayCounter(), bool isInArrears = false);
    //! \name Inspectors
    //@{
    const Currency& paymentCurrency() const { return paymentCurrency_; }
    const QuantLib::ext::shared_ptr<FormulaBasedIndex>& formulaBasedIndex() const { return index_; }
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
private:
    const Currency paymentCurrency_;
    QuantLib::ext::shared_ptr<FormulaBasedIndex> index_;
};

//! helper class building a sequence of formula based coupons
class FormulaBasedLeg {
public:
    FormulaBasedLeg(const Currency& paymentCurrency, const Schedule& schedule,
                    const QuantLib::ext::shared_ptr<FormulaBasedIndex>& index);
    FormulaBasedLeg& withNotionals(Real notional);
    FormulaBasedLeg& withNotionals(const std::vector<Real>& notionals);
    FormulaBasedLeg& withPaymentDayCounter(const DayCounter&);
    FormulaBasedLeg& withPaymentAdjustment(BusinessDayConvention);
    FormulaBasedLeg& withPaymentLag(Natural lag);
    FormulaBasedLeg& withPaymentCalendar(const Calendar&);
    FormulaBasedLeg& withFixingDays(Natural fixingDays);
    FormulaBasedLeg& withFixingDays(const std::vector<Natural>& fixingDays);
    FormulaBasedLeg& inArrears(bool flag = true);
    FormulaBasedLeg& withZeroPayments(bool flag = true);
    operator Leg() const;

private:
    Currency paymentCurrency_;
    Schedule schedule_;
    QuantLib::ext::shared_ptr<FormulaBasedIndex> index_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Natural paymentLag_;
    Calendar paymentCalendar_;
    std::vector<Natural> fixingDays_;
    bool inArrears_, zeroPayments_;
};

//! base pricer for formula based coupons
class FormulaBasedCouponPricer : public FloatingRateCouponPricer {
public:
    // fx vols should be given for index currencies vs. payment currency pairs;
    // correlations should be given for pairs of index names resp. (index name,"FX"), if not given
    // they are assumed to be zero
    FormulaBasedCouponPricer(
        const std::string& paymentCurrencyCode,
        const std::map<std::string, Handle<BlackVolTermStructure>>& fxVolatilities,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlation)
        : paymentCurrencyCode_(paymentCurrencyCode), fxVolatilities_(fxVolatilities), correlation_(correlation) {
        for (auto const& v : fxVolatilities)
            registerWith(v.second);
        for (auto const& c : correlation)
            registerWith(c.second);
    }

protected:
    const std::string paymentCurrencyCode_;
    const std::map<std::string, Handle<BlackVolTermStructure>> fxVolatilities_;
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlation_;
};

} // namespace QuantExt
