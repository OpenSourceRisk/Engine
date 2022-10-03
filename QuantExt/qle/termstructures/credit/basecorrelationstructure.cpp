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

#include <ql/time/schedule.hpp>
#include <qle/termstructures/credit/basecorrelationstructure.hpp>

namespace QuantExt {

BaseCorrelationTermStructure::BaseCorrelationTermStructure()
    : CorrelationTermStructure(), bdc_(Unadjusted) {}

BaseCorrelationTermStructure::BaseCorrelationTermStructure(const Date& refDate, const Calendar& cal,
                                                           BusinessDayConvention bdc, const std::vector<Period>& tenors,
                                                           const std::vector<Real>& detachmentPoints,
                                                           const DayCounter& dc, const Date& startDate,
                                                           boost::optional<DateGeneration::Rule> rule)
    : CorrelationTermStructure(refDate, cal, dc), bdc_(bdc), startDate_(startDate), rule_(rule), tenors_(tenors), detachmentPoints_(detachmentPoints) {
    // Ensure tenors are sorted and positive
    validate();
    initializeDatesAndTimes();
}

BaseCorrelationTermStructure::BaseCorrelationTermStructure(Natural settlementDays, const Calendar& cal,
                                                           BusinessDayConvention bdc, const std::vector<Period>& tenors,
                                                           const std::vector<double>& detachmentPoints,
                                                           const DayCounter& dc, const Date& startDate,
                                                           boost::optional<DateGeneration::Rule> rule)
    : CorrelationTermStructure(settlementDays, cal, dc), bdc_(bdc), startDate_(startDate), rule_(rule), tenors_(tenors),
      detachmentPoints_(detachmentPoints) {
    validate();
    initializeDatesAndTimes();
}

void BaseCorrelationTermStructure::validate() const {
    Period prevTenor(0, Days);
    for (size_t i = 0; i < tenors_.size(); ++i) {
        QL_REQUIRE(tenors_[i] > prevTenor, "Tenors need to be sorted and larger than 0 * Days");
    }

    // Ensure detachmentpoints are sorted and between 0.0 and 1.0
    double prevDetachmentPoint = 0.0;
    for (size_t i = 0; i < detachmentPoints_.size(); ++i) {
        QL_REQUIRE(detachmentPoints_[i] > prevDetachmentPoint,
                   "Detachmentpoints need to be sorted and between (0, 1].");
        QL_REQUIRE(detachmentPoints_[i] < 1.0 || QuantLib::close_enough(detachmentPoints_[i], 1.0),
                   "Detachmentpoints need to be sorted and between (0, 1].");
    }
}

void BaseCorrelationTermStructure::initializeDatesAndTimes() const {
    const Date& refDate = referenceDate();
    Date start = startDate_ == Date() ? refDate : startDate_;
    Calendar cldr = calendar();

    for (Size i = 0; i < tenors_.size(); i++) {
        Date d;
        if (rule_) {
            d = start + tenors_[i];
            if (*rule_ == DateGeneration::CDS2015 || *rule_ == DateGeneration::CDS ||
                *rule_ == DateGeneration::OldCDS) {
                d = cdsMaturity(start, tenors_[i], *rule_);
            }
        } else {
            d = cldr.advance(start, tenors_[i], bdc_);
        }

        dates_.push_back(d);
        times_.push_back(timeFromReference(d));

        QL_REQUIRE(!dates_.empty(), "no dates left after removing expired dates");
    }
}

void BaseCorrelationTermStructure::checkRange(Time t, Real strike, bool extrapolate) const {
    bool extrapolationNeeded =
        t < minTime() || t > maxTime() || strike < minDetachmentPoint() || strike > maxDetachmentPoint();
    QL_REQUIRE(extrapolate || allowsExtrapolation() || !extrapolationNeeded,
               "No extrapolation allowed,  require t = "
                   << t << " to be between (" << minTime() << ", " << maxTime() << ") and detachmentPoint = " << strike
                   << " to be between (" << minDetachmentPoint() << ", " << maxDetachmentPoint() << ").");
}

} // namespace QuantExt
