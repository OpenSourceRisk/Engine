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

/*! \file makeaverageois.hpp
    \brief Helper class to instantiate standard average ON indexed swaps.

        \ingroup instruments
*/

#ifndef quantext_make_average_ois_hpp
#define quantext_make_average_ois_hpp

#include <qle/instruments/averageois.hpp>

using namespace QuantLib;

namespace QuantExt {

//! helper class
/*! This class provides a more comfortable way to instantiate standard
    average ON indexed swaps.

            \ingroup instruments
*/
class MakeAverageOIS {
public:
    MakeAverageOIS(const Period& swapTenor, const boost::shared_ptr<OvernightIndex>& overnightIndex,
                   const Period& onTenor, Rate fixedRate, const Period& fixedTenor, const DayCounter& fixedDayCounter,
                   const Period& spotLagTenor = 2 * Days, const Period& forwardStart = 0 * Days);

    operator AverageOIS() const;
    operator boost::shared_ptr<AverageOIS>() const;

    // Swap.
    MakeAverageOIS& receiveFixed(bool receiveFixed = true);
    MakeAverageOIS& withType(AverageOIS::Type type);
    MakeAverageOIS& withNominal(Real nominal);
    MakeAverageOIS& withEffectiveDate(const Date& effectiveDate);
    MakeAverageOIS& withTerminationDate(const Date& terminationDate);
    MakeAverageOIS& withRule(DateGeneration::Rule rule);
    MakeAverageOIS& withSpotLagCalendar(const Calendar& spotLagCalendar);

    // Fixed Leg.
    MakeAverageOIS& withFixedCalendar(const Calendar& fixedCalendar);
    MakeAverageOIS& withFixedConvention(BusinessDayConvention fixedConvention);
    MakeAverageOIS& withFixedTerminationDateConvention(BusinessDayConvention fixedTerminationDateConvention);
    MakeAverageOIS& withFixedRule(DateGeneration::Rule fixedRule);
    MakeAverageOIS& withFixedEndOfMonth(bool fixedEndOfMonth = false);
    MakeAverageOIS& withFixedFirstDate(const Date& fixedFirstDate);
    MakeAverageOIS& withFixedNextToLastDate(const Date& fixedNextToLastDate);
    MakeAverageOIS& withFixedPaymentAdjustment(BusinessDayConvention fixedPaymentAdjustment);
    MakeAverageOIS& withFixedPaymentCalendar(const Calendar& fixedPaymentCalendar);

    // ON Leg.
    MakeAverageOIS& withONCalendar(const Calendar& onCalendar);
    MakeAverageOIS& withONConvention(BusinessDayConvention onConvention);
    MakeAverageOIS& withONTerminationDateConvention(BusinessDayConvention onTerminationDateConvention);
    MakeAverageOIS& withONRule(DateGeneration::Rule onRule);
    MakeAverageOIS& withONEndOfMonth(bool onEndOfMonth = false);
    MakeAverageOIS& withONFirstDate(const Date& onFirstDate);
    MakeAverageOIS& withONNextToLastDate(const Date& onNextToLastDate);
    MakeAverageOIS& withRateCutoff(Natural rateCutoff);
    MakeAverageOIS& withONSpread(Spread onSpread);
    MakeAverageOIS& withONGearing(Real onGearing);
    MakeAverageOIS& withONDayCounter(const DayCounter& onDayCounter);
    MakeAverageOIS& withONPaymentAdjustment(BusinessDayConvention onPaymentAdjustment);
    MakeAverageOIS& withONPaymentCalendar(const Calendar& onPaymentCalendar);

    // Pricing.
    MakeAverageOIS& withONCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer);
    MakeAverageOIS& withDiscountingTermStructure(const Handle<YieldTermStructure>& discountCurve);
    MakeAverageOIS& withPricingEngine(const boost::shared_ptr<PricingEngine>& engine);

private:
    Period swapTenor_;
    boost::shared_ptr<OvernightIndex> overnightIndex_;
    Period onTenor_;
    Rate fixedRate_;
    Period fixedTenor_;
    DayCounter fixedDayCounter_;
    Period spotLagTenor_;
    Period forwardStart_;

    AverageOIS::Type type_;
    Real nominal_;
    Date effectiveDate_;
    Date terminationDate_;
    Calendar spotLagCalendar_;

    Calendar fixedCalendar_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention fixedTerminationDateConvention_;
    DateGeneration::Rule fixedRule_;
    bool fixedEndOfMonth_;
    Date fixedFirstDate_;
    Date fixedNextToLastDate_;
    BusinessDayConvention fixedPaymentAdjustment_;
    Calendar fixedPaymentCalendar_;

    Calendar onCalendar_;
    BusinessDayConvention onConvention_;
    BusinessDayConvention onTerminationDateConvention_;
    DateGeneration::Rule onRule_;
    bool onEndOfMonth_;
    Date onFirstDate_;
    Date onNextToLastDate_;
    Natural rateCutoff_;
    Spread onSpread_;
    Real onGearing_;
    DayCounter onDayCounter_;
    BusinessDayConvention onPaymentAdjustment_;
    Calendar onPaymentCalendar_;

    boost::shared_ptr<PricingEngine> engine_;
    boost::shared_ptr<AverageONIndexedCouponPricer> onCouponPricer_;
};
} // namespace QuantExt

#endif
