/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022 Quaternion Risk Management Ltd

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

#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {

BaseCorrelationTermStructure::BaseCorrelationTermStructure(const Date& refDate, const Calendar& cal,
                                                           BusinessDayConvention bdc, const std::vector<Period>& tenors,
                                                           const std::vector<Real>& detachmentPoints,
                                                           const DayCounter& dc, const Date& startDate,
                                                           boost::optional<DateGeneration::Rule> rule)
    : CorrelationTermStructure(refDate, cal, dc), tenors_(tenors), detachmentPoints_(detachmentPoints) {
    // Ensure tenors are sorted and positive
    validate();
    initializeDatesAndTimes(tenors, startDate, bdc, rule);
}

BaseCorrelationTermStructure::BaseCorrelationTermStructure(Natural settlementDays, const Calendar& cal,
                                                           BusinessDayConvention bdc, const std::vector<Period>& tenors,
                                                           const std::vector<Real>& detachmentPoints,
                                                           const DayCounter& dc, const Date& startDate,
                                                           boost::optional<DateGeneration::Rule> rule)
    : CorrelationTermStructure(settlementDays, cal, dc), tenors_(tenors), detachmentPoints_(detachmentPoints) {
    validate();
    initializeDatesAndTimes(tenors, startDate, bdc, rule);
}

void BaseCorrelationTermStructure::validate() const {
    Period prevTenor(0, Days);
    for (size_t i = 0; i < tenors_.size(); ++i) {
        QL_REQUIRE(tenors_[i] > prevTenor, "Tenors need to be sorted and larger than 0 * Days");
    }

    // Ensure detachmentpoints are sorted and between 0.0 and 1.0
    double prevDetachmentPoint = 0.0;
    for (size_t i = 0; i < tenors_.size(); ++i) {
        QL_REQUIRE(detachmentPoints_[i] > prevDetachmentPoint,
                   "Detachmentpoints need to be sorted and between (0, 1].");
    }
    QL_REQUIRE(detachmentPoints_.back() <= 1.0, "Detachmentpoints need to be sorted and between (0, 1].");
}

void BaseCorrelationTermStructure::initializeDatesAndTimes(const std::vector<Period>& tenors, const Date& startDate,
                                                           BusinessDayConvention bdc,
                                                           boost::optional<DateGeneration::Rule> rule) const {
    const Date& refDate = referenceDate();
    Date start = startDate == Date() ? refDate : startDate;
    Calendar cldr = calendar();
    
    Period previousTenor = 0 * Days;
    for (Size i = 0; i < tenors_.size(); i++) {
        if (i > 1) {
            QL_REQUIRE(tenors[i] > previousTenor, "Tenors need to be sorted");
        }
        Date d;
        if (rule) {
            d = start + tenors_[i];
            if (*rule == DateGeneration::CDS2015 || *rule == DateGeneration::CDS || *rule == DateGeneration::OldCDS) {
                d = cdsMaturity(start, tenors_[i], *rule);
            }
            Schedule schedule = MakeSchedule()
                                    .from(start)
                                    .to(d)
                                    .withFrequency(Quarterly)
                                    .withCalendar(cldr)
                                    .withConvention(bdc)
                                    .withTerminationDateConvention(Unadjusted)
                                    .withRule(*rule);
            d = cldr.adjust(schedule.dates().back(), bdc);
        } else {
            d = cldr.advance(start, tenors_[i], bdc);
        }

        dates_.push_back(d);
        times_.push_back(timeFromReference(d));

        QL_REQUIRE(!dates_.empty(), "no dates left after removing expired dates");
    }
}

void BaseCorrelationTermStructure::checkRange(Time t, Real strike, bool extrapolate) const { 
    bool extrapolationNeeded = t < times_.front() || t > times_.back() || strike < detachmentPoints_.front() ||
                               strike > detachmentPoints_.back();
    QL_REQUIRE(extrapolate || allowsExtrapolation() || !extrapolationNeeded,
               "No extrapolation allowed,  require t = "
                   << t << " to be between (" << times_.front() << ", " << times_.back() << ") and detachmentPoint = " << strike
                   << " to be between (" << detachmentPoints_.front() << ", " << detachmentPoints_.back() << ").");
}




} // namespace QuantExt
