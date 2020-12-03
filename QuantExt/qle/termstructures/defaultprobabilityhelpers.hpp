/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Chris Kenyon
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008 StatPro Italia srl

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

/*! \file defaultprobabilityhelpers.hpp
    \brief bootstrap helpers for default-probability term structures
     \ingroup termstructures
*/

#ifndef quantext_default_probability_helpers_hpp
#define quantext_default_probability_helpers_hpp

#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/time/schedule.hpp>
#include <qle/instruments/creditdefaultswap.hpp>

namespace QuantExt {

//! alias for default-probability bootstrap helpers
typedef BootstrapHelper<DefaultProbabilityTermStructure> DefaultProbabilityHelper;
typedef RelativeDateBootstrapHelper<DefaultProbabilityTermStructure> RelativeDateDefaultProbabilityHelper;

/*! Base default-probability bootstrap helper
    \ingroup termstructures
*/
class CdsHelper : public RelativeDateDefaultProbabilityHelper {
public:
    /*! Constructor
        @param quote  The helper's market quote.
        @param tenor  CDS tenor.
        @param settlementDays  The number of days from evaluation date to the start of the protection period. Prior to
                               the CDS Big Bang in 2009, this was typically 1 calendar day. After the CDS Big Bang,
                               this is typically 0 calendar days i.e. protection starts immediately.
        @param calendar  CDS calendar. Typically weekends only for standard non-JPY CDS and TYO for JPY.
        @param frequency  Coupon frequency. Typically 3 months for standard CDS.
        @param paymentConvention  The convention applied to coupons schedules and settlement dates.
        @param rule  The date generation rule for generating the CDS schedule. Typically, for CDS prior to the Big
                     Bang, \c OldCDS should be used. After the Big Bang, \c CDS was typical and since 2015 \c CDS2015
                     is standard.
        @param dayCounter  The day counter for CDS fee leg coupons. Typically it is Actual/360, excluding accrual end,
                           for all but the final coupon period with Actual/360, including accrual end, for the final
                           coupon. The \p lastPeriodDayCounter below allows for this distinction.
        @param recoveryRate  The recovery rate of the underlying reference entity.
        @param discountCurve  A handle to the relevant discount curve.
        @param startDate  Used to specify an explicit start date for the CDS schedule and the date from which the
                          CDS maturity is calculated via the \p tenor. Useful for off-the-run index schedules.
        @param settlesAccrual  Set to \c true if accrued fee is paid on the occurence of a credit event and set to
                               \c false if it is not. Typically this is \c true.
        @param protectionPaymentTime  Time at which protection payments are made when there is a credit event.
        @param lastPeriodDayCounter  The day counter for the last fee leg coupon. See comment on \p dayCounter.
    */
    CdsHelper(const Handle<Quote>& quote, const Period& tenor, Integer settlementDays, const Calendar& calendar,
              Frequency frequency, BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
              const DayCounter& dayCounter, Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
              const Date& startDate = Date(), bool settlesAccrual = true,
              CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                  CreditDefaultSwap::ProtectionPaymentTime::atDefault,
              const DayCounter& lastPeriodDayCounter = DayCounter());

    //! \copydoc QuantExt::CdsHelper::CdsHelper
    CdsHelper(Rate quote, const Period& tenor,
              Integer settlementDays,
              const Calendar& calendar,
              Frequency frequency,
              BusinessDayConvention paymentConvention,
              DateGeneration::Rule rule,
              const DayCounter& dayCounter,
              Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
              const Date& startDate = Date(),
              bool settlesAccrual = true,
              CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                  CreditDefaultSwap::ProtectionPaymentTime::atDefault,
              const DayCounter& lastPeriodDayCounter = DayCounter());
    void setTermStructure(DefaultProbabilityTermStructure*);
    boost::shared_ptr<QuantExt::CreditDefaultSwap> swap() const { return swap_; }

protected:
    void update();
    void initializeDates();
    virtual void resetEngine() = 0;
    Period tenor_;
    Integer settlementDays_;
    Calendar calendar_;
    Frequency frequency_;
    BusinessDayConvention paymentConvention_;
    DateGeneration::Rule rule_;
    DayCounter dayCounter_;
    Real recoveryRate_;
    Handle<YieldTermStructure> discountCurve_;
    bool settlesAccrual_;
    CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime_;
    DayCounter lastPeriodDayCounter_;

    Schedule schedule_;
    boost::shared_ptr<QuantExt::CreditDefaultSwap> swap_;
    RelinkableHandle<DefaultProbabilityTermStructure> probability_;
    //! protection effective date.
    Date protectionStart_, startDate_;
};

//! Spread-quoted CDS hazard rate bootstrap helper.
//!  \ingroup termstructures
class SpreadCdsHelper : public CdsHelper {
public:
    SpreadCdsHelper(const Handle<Quote>& runningSpread, const Period& tenor, Integer settlementDays,
                    const Calendar& calendar, Frequency frequency, BusinessDayConvention paymentConvention,
                    DateGeneration::Rule rule, const DayCounter& dayCounter, Real recoveryRate,
                    const Handle<YieldTermStructure>& discountCurve, const Date& startDate = Date(),
                    bool settlesAccrual = true,
                    CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                        CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                    const DayCounter& lastPeriodDayCounter = DayCounter());

    SpreadCdsHelper(Rate runningSpread, const Period& tenor, Integer settlementDays, const Calendar& calendar,
                    Frequency frequency, BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
                    const DayCounter& dayCounter, Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
                    const Date& startDate = Date(), bool settlesAccrual = true,
                    CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                        CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                    const DayCounter& lastPeriodDayCounter = DayCounter());
    Real impliedQuote() const;

private:
    void resetEngine();
};

//! Upfront-quoted CDS hazard rate bootstrap helper.
//!  \ingroup termstructures
class UpfrontCdsHelper : public CdsHelper {
public:
    /*! \note the upfront must be quoted in fractional units. */
    UpfrontCdsHelper(const Handle<Quote>& upfront, Rate runningSpread, const Period& tenor, Integer settlementDays,
                     const Calendar& calendar, Frequency frequency, BusinessDayConvention paymentConvention,
                     DateGeneration::Rule rule, const DayCounter& dayCounter, Real recoveryRate,
                     const Handle<YieldTermStructure>& discountCurve, const Date& startDate = Date(),
                     Natural upfrontSettlementDays = 3, bool settlesAccrual = true,
                     CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                         CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                     const DayCounter& lastPeriodDayCounter = DayCounter());

    /*! \note the upfront must be quoted in fractional units. */
    UpfrontCdsHelper(Rate upfront, Rate runningSpread, const Period& tenor, Integer settlementDays,
                     const Calendar& calendar, Frequency frequency, BusinessDayConvention paymentConvention,
                     DateGeneration::Rule rule, const DayCounter& dayCounter, Real recoveryRate,
                     const Handle<YieldTermStructure>& discountCurve, const Date& startDate = Date(),
                     Natural upfrontSettlementDays = 3, bool settlesAccrual = true,
                     CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                         CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                     const DayCounter& lastPeriodDayCounter = DayCounter());
    Real impliedQuote() const;
    void initializeUpfront();

private:
    Natural upfrontSettlementDays_;
    Date upfrontDate_;
    Rate runningSpread_;
    void resetEngine();
};
} // namespace QuantExt

#endif
