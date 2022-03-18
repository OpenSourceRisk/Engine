/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/termstructures/creditvolcurve.hpp>

#include <qle/utilities/time.hpp>

namespace QuantExt {

using namespace QuantLib;

CreditVolCurve::CreditVolCurve(BusinessDayConvention bdc, const DayCounter& dc) : VolatilityTermStructure(bdc, dc) {}

CreditVolCurve::CreditVolCurve(const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<QuantLib::Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(settlementDays, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

CreditVolCurve::CreditVolCurve(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<QuantLib::Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(referenceDate, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

void CreditVolCurve::init() const {
    QL_REQUIRE(terms_.size() == termCurves_.size(), "CreditVolCurve: terms size (" << terms_.size()
                                                                                   << ") must match termCurves size ("
                                                                                   << termCurves_.size());
}

Real CreditVolCurve::volatility(const Date& exerciseDate, const Period& underlyingTerm, const Real strike,
                                const Type& targetType) const {
    return volatility(timeFromReference(exerciseDate), periodToTime(underlyingTerm), strike, targetType);
}

Real CreditVolCurve::volatility(const Date& exerciseDate, const Real underlyingLength, const Real strike,
                                const Type& targetType) const {
    return volatility(timeFromReference(exerciseDate), underlyingLength, strike, targetType);
}

const std::vector<QuantLib::Period>& CreditVolCurve::terms() const { return terms_; }

const std::vector<Handle<CreditCurve>>& CreditVolCurve::termCurves() const { return termCurves_; }

const CreditVolCurve::Type& CreditVolCurve::type() const { return type_; }

Date CreditVolCurve::maxDate() const { return Date::maxDate(); }

Rate CreditVolCurve::minStrike() const { return -QL_MAX_REAL; }

Rate CreditVolCurve::maxStrike() const { return QL_MAX_REAL; }

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<QuantLib::Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Period, Date, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(settlementDays, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {}

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<QuantLib::Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Period, Date, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(referenceDate, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {}

Real InterpolatingCreditVolCurve::volatility(const Real exerciseTime, const Real underlyingLength, const Real strike,
                                             const Type& targetType) const {
    // TDODO
    return 0.0;
}

void InterpolatingCreditVolCurve::performCalculations() const {
    // TODO
}

SpreadedCreditVolCurve::SpreadedCreditVolCurve(const Handle<CreditVolCurve> baseCurve, const std::vector<Date> expiries,
                                               const std::vector<Handle<Quote>> spreads, const bool stickyMoneyness)
    : CreditVolCurve(baseCurve->businessDayConvention(), baseCurve->dayCounter()), baseCurve_(baseCurve),
      expiries_(expiries), spreads_(spreads), stickyMoneyness_(stickyMoneyness) {}

const std::vector<QuantLib::Period>& SpreadedCreditVolCurve::terms() const { return terms_; }

const std::vector<Handle<CreditCurve>>& SpreadedCreditVolCurve::termCurves() const { return termCurves_; }

const Date& SpreadedCreditVolCurve::referenceDate() const { return baseCurve_->referenceDate(); }

Real SpreadedCreditVolCurve::volatility(const Real exerciseTime, const Real underlyingLength, const Real strike,
                                        const Type& targetType) const {
    // TDODO
    return 0.0;
}

} // namespace QuantExt
