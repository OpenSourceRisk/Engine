/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file averageois.hpp
    \brief Swap of arithmetic average overnight index against fixed

    \ingroup instruments
*/

#ifndef quantext_average_ois_hpp
#define quantext_average_ois_hpp

#include <boost/make_shared.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

#include <qle/cashflows/averageonindexedcouponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Average overnight index swap
/*! Swap with first leg fixed and the second
    leg being an arithmetic average overnight index.

            \ingroup instruments
*/
class AverageOIS : public Swap {
public:
    //! Receiver (Payer) means receive (pay) fixed.
    enum Type { Receiver = -1, Payer = 1 };

    //! Arithmetic average ON leg vs. fixed leg constructor.
    AverageOIS(Type type, Real nominal, const Schedule& fixedSchedule, Rate fixedRate,
               const DayCounter& fixedDayCounter, BusinessDayConvention fixedPaymentAdjustment,
               const Calendar& fixedPaymentCalendar, const Schedule& onSchedule,
               const boost::shared_ptr<OvernightIndex>& overnightIndex, BusinessDayConvention onPaymentAdjustment,
               const Calendar& onPaymentCalendar, Natural rateCutoff = 0, Spread onSpread = 0.0, Real onGearing = 1.0,
               const DayCounter& onDayCounter = DayCounter(),
               const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer =
                   boost::shared_ptr<AverageONIndexedCouponPricer>());

    /*! Arithmetic average ON leg vs. fixed leg constructor, allowing for
        varying nominals, fixed rates, ON leg spreads and ON leg gearings.
    */
    AverageOIS(Type type, std::vector<Real> nominals, const Schedule& fixedSchedule, std::vector<Rate> fixedRates,
               const DayCounter& fixedDayCounter, BusinessDayConvention fixedPaymentAdjustment,
               const Calendar& fixedPaymentCalendar, const Schedule& onSchedule,
               const boost::shared_ptr<OvernightIndex>& overnightIndex, BusinessDayConvention onPaymentAdjustment,
               const Calendar& onPaymentCalendar, Natural rateCutoff = 0,
               std::vector<Spread> onSpreads = std::vector<Spread>(1, 0.0),
               std::vector<Real> onGearings = std::vector<Real>(1, 1.0), const DayCounter& onDayCounter = DayCounter(),
               const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer =
                   boost::shared_ptr<AverageONIndexedCouponPricer>());

    //! \name Inspectors
    //@{
    Type type() const { return type_; }

    Real nominal() const;
    const std::vector<Real>& nominals() const { return nominals_; }

    Rate fixedRate() const;
    const std::vector<Rate>& fixedRates() const { return fixedRates_; }
    const DayCounter& fixedDayCounter() { return fixedDayCounter_; }

    const boost::shared_ptr<OvernightIndex>& overnightIndex() { return overnightIndex_; }
    Natural rateCutoff() { return rateCutoff_; }
    Spread onSpread() const;
    const std::vector<Spread>& onSpreads() const { return onSpreads_; }
    Real onGearing() const;
    const std::vector<Real>& onGearings() const { return onGearings_; }
    const DayCounter& onDayCounter() { return onDayCounter_; }

    const Leg& fixedLeg() const { return legs_[0]; }
    const Leg& overnightLeg() const { return legs_[1]; }
    //@}

    //! \name Results
    //@{
    Real fixedLegBPS() const;
    Real fixedLegNPV() const;
    Real fairRate() const;

    Real overnightLegBPS() const;
    Real overnightLegNPV() const;
    Spread fairSpread() const;
    //@}

    void setONIndexedCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer);

private:
    void initialize(const Schedule& fixedLegSchedule, const Schedule& onLegSchedule);

    Type type_;
    std::vector<Real> nominals_;

    std::vector<Rate> fixedRates_;
    DayCounter fixedDayCounter_;
    BusinessDayConvention fixedPaymentAdjustment_;
    Calendar fixedPaymentCalendar_;

    boost::shared_ptr<OvernightIndex> overnightIndex_;
    BusinessDayConvention onPaymentAdjustment_;
    Calendar onPaymentCalendar_;
    Natural rateCutoff_;
    std::vector<Spread> onSpreads_;
    std::vector<Real> onGearings_;
    DayCounter onDayCounter_;
    boost::shared_ptr<AverageONIndexedCouponPricer> onCouponPricer_;
};
} // namespace QuantExt

#endif
