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

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer
 Copyright (C) 2026 AcadiaSoft, Inc.

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

/*! \file overnightindexedcouponbase.hpp
    \brief base class for overnight indexed coupons
*/
#pragma once

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>
#include <ql/shared_ptr.hpp>

namespace QuantExt {

//! Base overnight coupon class.
class OvernightIndexedCouponBase : public QuantLib::FloatingRateCoupon {
protected:
    // Note: class is abstract (effectiveRate method) but make the ctor protected in any case.
    OvernightIndexedCouponBase(
        const QuantLib::Date& paymentDate,
        QuantLib::Real nominal,
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& overnightIndex,
        QuantLib::Real gearing = 1.0,
        QuantLib::Spread spread = 0.0,
        const QuantLib::Date& refPeriodStart = QuantLib::Date(),
        const QuantLib::Date& refPeriodEnd = QuantLib::Date(),
        const QuantLib::DayCounter& dayCounter = QuantLib::DayCounter(),
        bool telescopicValueDates = false,
        const QuantLib::Period& lookback = 0 * QuantLib::Days,
        const QuantLib::Natural rateCutoff = 0,
        const QuantLib::Natural fixingDays = QuantLib::Null<QuantLib::Size>(),
        const QuantLib::Date& rateComputationStartDate = QuantLib::Null<QuantLib::Date>(),
        const QuantLib::Date& rateComputationEndDate = QuantLib::Null<QuantLib::Date>(),
        bool applyObservationShift = false);

    // True if telescopic dates requested and can be applied.
    bool telescopicDates_;

public:
    //! \name Inspectors
    //@{
    //! Underlying overnight fixing dates.
    const std::vector<QuantLib::Date>& fixingDates() const;
    //! Underlying overnight accrual fractions.
    const std::vector<QuantLib::Time>& dt() const;
    //! The overnight fixings.
    const std::vector<QuantLib::Rate>& indexFixings() const;
    //! Value dates, associated with the fixing dates, for the rates to be compounded
    const std::vector<QuantLib::Date>& valueDates() const;
    //! Interest dates against which the overnight rates are applied.
    const std::vector<QuantLib::Date>& interestDates() const;
    //! Lookback period. It must be a non-negative number of days.
    const QuantLib::Period& lookback() const { return lookback_; }
    //! Number of underlying overnight periods at the end of the coupon for which the rate is frozen.
    QuantLib::Natural rateCutoff() const { return rateCutoff_; }
    //! Explicitly specified date for the start of underlying overnight rate computation.
    const QuantLib::Date& rateComputationStartDate() const { return rateComputationStartDate_; }
    //! Explicitly specified date for the end of underlying overnight rate computation.
    const QuantLib::Date& rateComputationEndDate() const { return rateComputationEndDate_; }
    //! Is there an observation shift.
    bool applyObservationShift() const { return applyObservationShift_; }
    //! True if there is a lookback period, false otherwise.
    bool hasLookback() const { return lookback_.length() != 0; }
    //! The underlying overnight index.
    const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& overnightIndex() const { return overnightIndex_; }
    //! True is telescopic dates were requested and can be applied, false otherwise.
    bool telescopicDates() const { return telescopicDates_; }
    //! True if there is a rate computation period separate from the main coupon accrual period.
    bool separateRateCompPeriod() const { return separateRateCompPeriod_; }
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! The date when the coupon is fully determined i.e. the last fixing date.
    QuantLib::Date fixingDate() const override { return fixingDates_.back(); }
    //! The accrued amount up to `date`.
    QuantLib::Real accruedAmount(const QuantLib::Date& date) const override;
    //@}

private:
    // Calculate the effective rate up to a given date. Must be implemented in derived classes.
    virtual QuantLib::Rate effectiveRate(const QuantLib::Date& date) const = 0;

    QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> overnightIndex_;
    // The valueDates_ are the value dates associated with the corresponding fixing date.
    mutable std::vector<QuantLib::Date> valueDates_;
    // The fixingDates_ are the dates on which the overnight index fixings are observed.
    mutable std::vector<QuantLib::Date> fixingDates_;
    // The interestDates_ define the overnight periods against which the overnight rate is applied.
    mutable std::vector<QuantLib::Date> interestDates_;
    mutable std::vector<QuantLib::Rate> fixings_;
    mutable QuantLib::Size n_;
    mutable std::vector<QuantLib::Time> dt_;
    QuantLib::Period lookback_;
    QuantLib::Natural rateCutoff_;
    QuantLib::Date rateComputationStartDate_;
    QuantLib::Date rateComputationEndDate_;
    bool applyObservationShift_;
    bool separateRateCompPeriod_;

    // Record last possible fixing date.
    QuantLib::Date lastFixingDate_;

    // Index into fixing dates for current start of telescopic period. If not set, all dates are present.
    mutable QuantLib::ext::optional<QuantLib::Size> tsStartIdx_;

    // Cached adjusted evaluation date i.e. first business day preceding the evaluation date for which the 
    // current date schedules were calculated.
    mutable QuantLib::Date cachedEvalDate_;

    // Set value of telescopicDates_ according to whether telescoping can be used or not.
    virtual void setTelescopicDates() { /* Default is to leave telescopicDates_ unchanged */ }

    // Check if (telescopic) date schedules are stale.
    bool haveStaleDates() const;

    // Update (telescopic) date schedules that are stale.
    void updateSchedules() const;

    // Add schedule dates corresponding to all fixing dates from `fixStart` up to `fixEnd`.
    void addScheduleDates(QuantLib::Date fixEnd, QuantLib::Date fixStart, QuantLib::Date intStart,
        QuantLib::Date lbStart, bool exclEnd = false);

    // Add rate cut-off dates
    void addRateCutoffDates(QuantLib::Date fixEnd, QuantLib::Date rcoStart, QuantLib::Date rcoIntStart,
        QuantLib::Date rcoLbStart, bool exclEnd = false);

    // Add final dates in telescopic period.
    void addTelescopeBackStub(QuantLib::Date fixEnd, QuantLib::Date rcoStart, QuantLib::Date rcoIntStart,
        QuantLib::Date rcoLbStart, const QuantLib::Date& intEnd, const QuantLib::Date& adjIntEnd,
        const QuantLib::Date& lbEnd);

    // After adding dates in telescopic period, check if we have all dates and update tsStartIdx_ accordingly.
    void checkForAllDates() const;

    // Validate date schedule sizes and populate number of periods.
    void validateDates() const;

    // Populate accrual values dt_.
    void populateAccruals() const;
};

}
