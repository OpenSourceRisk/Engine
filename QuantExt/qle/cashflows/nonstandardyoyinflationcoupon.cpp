/*
 Copyright (C) 2021 Quaternion Risk Management Ltd

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

/*
 Copyright (C) 2009 Chris Kenyon

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
 */

#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/nonstandardinflationcouponpricer.hpp>
#include <qle/cashflows/nonstandardyoyinflationcoupon.hpp>

namespace QuantExt {

void NonStandardYoYInflationCoupon::setFixingDates(const Date& denumatorDate, const Date& numeratorDate,
                                                   const Period& observationLag) {

    fixingDateDenumerator_ = index_->fixingCalendar().advance(
        denumatorDate - observationLag_, -static_cast<Integer>(fixingDays_), Days, ModifiedPreceding);

    fixingDateNumerator_ = index_->fixingCalendar().advance(
        numeratorDate - observationLag_, -static_cast<Integer>(fixingDays_), Days, ModifiedPreceding);
}

NonStandardYoYInflationCoupon::NonStandardYoYInflationCoupon(
    const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
    const ext::shared_ptr<ZeroInflationIndex>& index, const Period& observationLag, const DayCounter& dayCounter,
    Real gearing, Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd, bool addInflationNotional, QuantLib::CPI::InterpolationType interpolation)
    : QuantLib::InflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, observationLag, dayCounter,
                                refPeriodStart, refPeriodEnd),
      gearing_(gearing), spread_(spread), addInflationNotional_(addInflationNotional),
      interpolationType_(interpolation) {
    setFixingDates(refPeriodStart, refPeriodEnd, observationLag);
}

void NonStandardYoYInflationCoupon::accept(AcyclicVisitor& v) {
    Visitor<NonStandardYoYInflationCoupon>* v1 = dynamic_cast<Visitor<NonStandardYoYInflationCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        InflationCoupon::accept(v);
}

bool NonStandardYoYInflationCoupon::checkPricerImpl(const ext::shared_ptr<InflationCouponPricer>& pricer) const {
    return static_cast<bool>(ext::dynamic_pointer_cast<NonStandardYoYInflationCouponPricer>(pricer));
}

Date NonStandardYoYInflationCoupon::fixingDateNumerator() const { return fixingDateNumerator_; }
Date NonStandardYoYInflationCoupon::fixingDateDenumerator() const { return fixingDateDenumerator_; }

Rate NonStandardYoYInflationCoupon::indexFixing() const {
    auto zii = QuantLib::ext::dynamic_pointer_cast<QuantLib::ZeroInflationIndex>(index_);
    Real I_t = CPI::laggedFixing(zii, fixingDateNumerator() + observationLag_, observationLag_, interpolationType_);
    Real I_s = CPI::laggedFixing(zii, fixingDateDenumerator() + observationLag_, observationLag_, interpolationType_);
    return I_t / I_s - 1.0;
}

Rate NonStandardYoYInflationCoupon::adjustedFixing() const { return (rate() - spread()) / gearing(); }

Date NonStandardYoYInflationCoupon::fixingDate() const { return fixingDateNumerator_; };

Rate NonStandardYoYInflationCoupon::rate() const {
    Rate r = InflationCoupon::rate();
    if (addInflationNotional_) {
        r = gearing() * ((r - spread()) / gearing() + 1) + spread();
    }
    return r;
}

bool NonStandardYoYInflationCoupon::addInflationNotional() const { return addInflationNotional_; };

ext::shared_ptr<ZeroInflationIndex> NonStandardYoYInflationCoupon::cpiIndex() const {
    return ext::dynamic_pointer_cast<ZeroInflationIndex>(index_);
}

} // namespace QuantExt
