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

/*! \file qle/cashflows/overnightindexedcouponbase.hpp
    \brief base class for overnight indexed coupons
*/
#pragma once

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>
#include <ql/shared_ptr.hpp>

namespace QuantExt {

/** Base overnight coupon class.
 *  This class is the base class for an overnight indexed coupon i.e. a coupon that generally spans a period longer 
 *  than one day and has a sequence of underlying overnight periods with an associated overnight index. For example, a 
 *  3 month coupon referencing SOFR with underlying overnight periods on the SOFR calendar. The overnight rate may be 
 *  accumulated in different ways as specified by the type, currently either compounding or averaging - see the derived 
 *  classes \ref OvernightIndexedCoupon and \ref AverageONIndexedCoupon respectively.
 *
 *  Let us denote \f$\hat{t}^i_0\f$ and \f$\hat{t}^i_n\f$ as the start and end dates of the coupon. They may or may not 
 *  be holidays on the underlying overnight index calendar. Let \f$t^i_0\f$ be \f$\hat{t}^i_0\f$ adjusted to the 
 *  preceding good index business day if necessary. Let \f$t^i_n\f$ be \f$\hat{t}^i_n\f$ adjusted to the following good 
 *  index business day if necessary. We then define the \f$n\f$ underlying overnight interest periods as:
 *  \f[
 *      P^i_j = [t^i_{j-1}, t^i_j] \quad \text{for } j=1, \dots, n.
 *  \f]
 *
 *  Each period \f$P^i_j\f$ is associated with an overnight fixing date \f$t^f_k\f$, where the indices are related by:
 *  \f[
 *      k = j - 1 \quad \text{for } j=1, \dots, n
 *  \f]
 *
 *  Each overnight fixing date \f$t^f_k\f$ has its associated value date period \f$P^v_j\f$:
 *  \f[
 *      P^v_j = [t^v_{j-1}, t^v_j] \quad \text{for } j=1, \dots, n.
 *  \f]
 *  Note that \f$t^i_J\f$, \f$t^v_J\f$ for \f$J=0, \dots, n\f$ and \f$t^f_k\f$ are all good index business days.
 *
 *  There are a number of coupon features which impact the interest periods and value date periods, and in turn the 
 *  fixing dates:
 *  - if there is no lookback and no externally specified fixing days, i.e. the fixing lag is the natural fixing lag of 
 *    the overnight index, then the value date periods and interest periods align. Under this setup, the overnight 
 *    index fixing rate is applied over its natural interest period, modulo rate cut-off mentioned below and the 
 *    coupon start or end dates being holidays in which case the rate is applied for a stub period that is a subset of 
 *    the overnight value date period.
 *  - if there is a non-zero lookback, \f$\delta_{LB}\f$, which must be a positive number of days, and there is no 
 *    observation shift, then the value date periods and associated fixing dates are shifted back by \f$\delta_{LB}\f$ 
 *    number of index business days. In other words, under this setup, the overnight rate applied for an underlying 
 *    overnight interest period is the overnight rate fixing observed for an overnight period \f$\delta_{LB}\f$ index 
 *    business days prior to the interest period.
 *  - if there is a non-zero lookback, \f$\delta_{LB}\f$, and an observation shift, then the interest periods are 
 *    shifted back also by \f$\delta_{LB}\f$ number of index business days. So we have new interest periods:
 *    \f[
 *        \begin{aligned}
 *        \bar{P}^i_j &= [\bar{t}^i_{j-1}, \bar{t}^i_j] \\
 *                    &= [LB(t^i_{j-1}, \delta_{LB}), LB(t^i_j, \delta_{LB})]
 *        \end{aligned}
 *    \f]
 *    where \f$LB(t, \delta)\f$ is the function that shifts the business date \f$t\f$ back by \f$\delta\f$ index 
 *    business days. So the overnight rate is applied over these new interest periods.
 *  - if in any of the scenarios above, the coupon has an external fixing lag that is not equal to the fixing lag of 
 *    the overnight index, then the value date periods and associated fixing dates are shifted back by this fixing lag,
 *    which must be a positive number of business days.
 *  - if there is a non-zero rate cut-off, \f$M\f$, then the last \f$M\f$ overnight fixing rates are frozen at the 
 *    \f$M+1\f$ from last overnight fixing rate i.e. fixing rate observed at \f$t^f_{n-M-1}\f$. Clearly, we must have 
 *    \f$M < n\f$.
 *  - if there is a rate computation start or end date given, they are used as the starting point for the interest 
 *    periods in the discussion above instead of the coupon start and end dates. In this case, the coupon start and end
 *    dates are used only in the final coupon or accrual calculation i.e. the accumulated, either compounded or
 *    averaged, overnight rate is applied to the coupon accrual period defined by the coupon start and end dates.
 *
 *  If telescopic dates is requested for the coupon, the coupon stores only the dates necessary for a calculations 
 *  using a telescopic formula if such a calculation is possible.
 *  - for coupon type `Compounding`, telescopic dates are allowed if there is no lookback or there is a lookback with 
 *    observation shift and the coupon fixing lag is either not specified or if specified it is the same as the index 
 *    fixing lag. As outlined above, in this case the value date periods align with the interest date periods and the 
 *    telescopic formula gives the same result as the full calculation.
 *  - for coupon type `Averaging`, telescopic dates are always allowed if requested. See 
 *    \ref AverageONIndexedCouponPricer, if telescopic dates are requested, it implies use of the `Takada` 
 *    approximation and in this case, because the valuation is an approximation in any case, telescopic dates are 
 *    allowed even when value period dates do not align with interest period dates.
 *
 *  When telescopic dates are used, there are certain cases where we want to bypass the check that determines if we 
 *  need to add or remove dates from the schedule when the evaluation date changes. For example, in the OISRateHelper 
 *  or AverageOISRateHelper where the date schedule is rebuilt when the evaluation date changes, there is no need to 
 *  perform this check. A parameter `staleDatesCheck` is provided in the constructor for this purpose.
 */
class OvernightIndexedCouponBase : public QuantLib::FloatingRateCoupon {
public:
    enum class Type { Compounding, Averaging, BrlCdi };

protected:
    // Note: class is abstract (effectiveRate method) but make the ctor protected in any case.
    OvernightIndexedCouponBase(
        Type rateType,
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
        const QuantLib::Natural fixingDays = QuantLib::Null<QuantLib::Natural>(),
        const QuantLib::Date& rateComputationStartDate = QuantLib::Date(),
        const QuantLib::Date& rateComputationEndDate = QuantLib::Date(),
        bool observationShift = true,
        bool staleDatesCheck = true);

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
    bool observationShift() const { return observationShift_; }
    //! True if there is a lookback period, false otherwise.
    bool hasLookback() const { return lookback_.length() != 0; }
    //! The underlying overnight index.
    const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& overnightIndex() const { return overnightIndex_; }
    //! True if telescopic formula can be applied, false otherwise.
    bool canApplyTelescopic() const;
    //! True if telescopic dates were requested and can be applied, false otherwise.
    bool telescopicDates() const { return telescopicDates_; }
    //! True if there is a rate computation period separate from the main coupon accrual period.
    bool separateRateCompPeriod() const { return separateRateCompPeriod_; }
    //! Default implemenation which is overridden for example in \ref OvernightIndexedCoupon.
    virtual bool includeSpread() const { return false; }
    //! Whether the overnight coupon is compouding or averaging.
    Type rateType() const { return rateType_; }
    //! The index of the period where telescopic formula starts to apply.
    QuantLib::ext::optional<QuantLib::Size> telescopicStartIdx() const { return tsStartIdx_; }
    //! The last fixing date ignoring any rate cut-off.
    QuantLib::Date fixingDateNoCutoff() const { return lastFixingDateNoCutoff_; }
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const override;
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! The date when the coupon is fully determined i.e. the last fixing date.
    QuantLib::Date fixingDate() const override { return lastFixingDate_; }
    //@}
    //! \name Coupon interface
    //@{
    //! The accrued amount up to `date`.
    QuantLib::Real accruedAmount(const QuantLib::Date& date) const override;
    //@}

protected:
    // Stored along with rate_ to avoid recalculation of amount().
    mutable QuantLib::Date upToDateAdj_;
private:
    // Calculate the effective rate up to a given date. Must be implemented in derived classes.
    // The second element is the date up to which the effective rate is calculated. With observation shift, it will be 
    // different from the input `date` and we do not want to calculate it again elsewhere.
    virtual std::pair<QuantLib::Rate, QuantLib::Date> effectiveRate(const QuantLib::Date& date) const = 0;

    Type rateType_;
    // True if telescopic dates requested and can be applied.
    bool telescopicDates_;
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
    bool observationShift_;
    // Made mutable so that we can update it in certain places where it is efficient to do so.
    mutable bool staleDatesCheck_;
    bool separateRateCompPeriod_;

    // Record last possible fixing date.
    QuantLib::Date lastFixingDate_;

    // In some places, e.g. BlackOvernightIndexedCouponPricer, the logic needs the last fixing date ignoring any rate 
    // cut-off. We record this date as well.
    QuantLib::Date lastFixingDateNoCutoff_;

    // Index into fixing dates for current start of telescopic period. If not set, all dates are present.
    mutable QuantLib::ext::optional<QuantLib::Size> tsStartIdx_;

    // Cached adjusted evaluation date i.e. first business day preceding the evaluation date for which the 
    // current date schedules were calculated.
    mutable QuantLib::Date cachedEvalDate_;

    // Set value of telescopicDates_ according to whether telescoping can be used or not.
    void setTelescopicDates();

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
        const QuantLib::Date& lbEnd, bool addFixEnd = false);

    // After adding dates in telescopic period, check if we have all dates and update tsStartIdx_ accordingly.
    void checkForAllDates() const;

    // Validate date schedule sizes and populate number of periods.
    void validateDates() const;

    // Populate accrual values dt_.
    void populateAccruals() const;
};

//! Convenience builder class for overnight coupons.
class OvernightCouponBuilder {
public:
    OvernightCouponBuilder(
        OvernightIndexedCouponBase::Type rateType,
        const QuantLib::Date& paymentDate,
        QuantLib::Real nominal,
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& index);

    OvernightCouponBuilder& withGearing(QuantLib::Real gearing);
    OvernightCouponBuilder& withSpread(QuantLib::Real spread);
    OvernightCouponBuilder& withRefPeriodStart(const QuantLib::Date& refPeriodStart);
    OvernightCouponBuilder& withRefPeriodEnd(const QuantLib::Date& refPeriodEnd);
    OvernightCouponBuilder& withDayCounter(const QuantLib::DayCounter& dayCounter);
    OvernightCouponBuilder& withTelescopicValueDates(bool telescopicValueDates);
    OvernightCouponBuilder& withIncludeSpread(bool includeSpread);
    OvernightCouponBuilder& withLookback(const QuantLib::Period& lookback);
    OvernightCouponBuilder& withRateCutoff(QuantLib::Natural rateCutoff);
    OvernightCouponBuilder& withFixingDays(QuantLib::Natural fixingDays);
    OvernightCouponBuilder& withRateComputationStart(const QuantLib::Date& rateComputationStart);
    OvernightCouponBuilder& withRateComputationEnd(const QuantLib::Date& rateComputationEnd);
    OvernightCouponBuilder& withObservationShift(bool observationShift);

    QuantLib::ext::shared_ptr<OvernightIndexedCouponBase> build() const;

private:
    OvernightIndexedCouponBase::Type rateType_;
    QuantLib::Date paymentDate_;
    QuantLib::Real nominal_;
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> index_;
    QuantLib::Real gearing_ = 1.0;
    QuantLib::Spread spread_ = 0.0;
    QuantLib::Date refPeriodStart_;
    QuantLib::Date refPeriodEnd_;
    QuantLib::DayCounter dayCounter_;
    bool telescopicValueDates_ = false;
    bool includeSpread_ = false;
    QuantLib::Period lookback_ = 0 * QuantLib::Days;
    QuantLib::Natural rateCutoff_ = 0;
    QuantLib::Natural fixingDays_ = QuantLib::Null<QuantLib::Natural>();
    QuantLib::Date rateComputationStart_;
    QuantLib::Date rateComputationEnd_;
    bool observationShift_ = true;
};

}
