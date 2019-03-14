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

/*! \file bondoption.hpp
\brief bond option classes

\ingroup instruments
*/

#ifndef quantext_bond_option_hpp
#define quantext_bond_option_hpp

#include <ql/instruments/bond.hpp>
#include <ql/pricingengine.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/handle.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantLib {

    class Schedule;
    class DayCounter;

} // namespace QuantLib

namespace QuantExt {
    using namespace QuantLib;

    //! Bond option base class
    /*! Base bond option class for fixed and zero coupon bonds.
    Defines commonalities between fixed and zero coupon bond options. 
    At present, only European and Bermudan put/call schedules supported 
    (no American optionality), as defined by the Callability class.

    \todo American/Bermudan bond options ?
    \todo floating rate bond options ?

    \ingroup instruments
    */
    class BondOption : public QuantLib::Bond {
    public:
        class arguments;
        class results;
        class engine;
        //! \name Inspectors
        //@{
        //! return the put/call schedule
        const CallabilitySchedule& callability() const {
            return putCallSchedule_;
        }

    protected:
        BondOption(Natural settlementDays,
            const Schedule& schedule,
            const DayCounter& paymentDayCounter,
            const Date& issueDate = Date(),
            const CallabilitySchedule& putCallSchedule
            = CallabilitySchedule());

        DayCounter paymentDayCounter_;
        Frequency frequency_;
        CallabilitySchedule putCallSchedule_;
    };

    class BondOption::arguments : public Bond::arguments {
    public:
        arguments() {}
        std::vector<Date> couponDates;
        std::vector<Real> couponAmounts;
        //! redemption = face amount * redemption / 100.
        Real redemption;
        Date redemptionDate;
        DayCounter paymentDayCounter;
        Frequency frequency;
        CallabilitySchedule putCallSchedule;
        //! bond full/dirty/cash prices
        std::vector<Real> callabilityPrices;
        std::vector<Date> callabilityDates;
        //! Spread to apply to the valuation. This is a continuously
        //! componded rate added to the model. 
        Real spread;
        bool bondInPrice;
        void validate() const;
    };

    //! results for a bond option calculation
    class BondOption::results : public Bond::results {
    public:
        // no extra results set yet
    };

    //! base class for fixed rate bond option engine
    class BondOption::engine
        : public GenericEngine<BondOption::arguments,
        BondOption::results> {};


    //! Fixed rate bond option
    /*! Fixed rate bond option class.

    \ingroup instruments
    */
    class FixedRateBondOption : public BondOption {
    public:
        FixedRateBondOption(Natural settlementDays,
            Real faceAmount,
            const Schedule& schedule,
            const std::vector<Rate>& coupons,
            const DayCounter& accrualDayCounter,
            BusinessDayConvention paymentConvention = Following,
            Real redemption = 100.0,
            const Date& issueDate = Date(),
            const CallabilitySchedule& putCallSchedule
            = CallabilitySchedule(),
            bool bondInPrice = false);

        virtual void setupArguments(PricingEngine::arguments* args) const;

    private:
        //! accrued interest used internally, where includeToday = false
        /*! same as Bond::accruedAmount() but with enable early
        payments true.  Forces accrued to be calculated in a
        consistent way for future put/ call dates, which can be
        problematic in lattice engines when option dates are also
        coupon dates.
        */
        Real accrued(Date settlement) const;
        bool bondInPrice_;
    };
}

#endif
