/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/cashflows/commoditycashflow.hpp
    \brief Some data and logic shared among commodity cashflows
 */

#ifndef quantext_commodity_cash_flow_hpp
#define quantext_commodity_cash_flow_hpp

#include <ql/time/date.hpp>
#include <ql/time/calendar.hpp>
#include <set>

namespace QuantExt {

/*! \brief Enumeration indicating the frequency associated with a commodity quantity.
*/
enum class CommodityQuantityFrequency {
    PerCalculationPeriod,
    PerCalendarDay,
    PerPricingDay,
    PerHour
};

/*! \brief Get the set of valid pricing dates in a period.

    \param start            The start date of the period.
    \param end              The end date of the period.
    \param pricingCalendar  The pricing calendar used to determine valid dates
    \param excludeStart     Set to \c true if the start date should be excluded from the set of pricing dates and to 
                            \c false if the start date should be included.
    \param includeEnd       set to \c true if the end date should be included in the set of pricing dates and to 
                            \c false if the end date should be excluded.
    \param useBusinessDays  Set to \c true if \p pricingCalendar business dates are to be considered valid pricing 
                            dates and \p false if \p pricingCalendar holidays are to be considered valid pricing 
                            dates. The latter case is unusual but is useful for some electricity futures e.g. ICE PW2 
                            contract which averages over weekends and non-NERC business days.

    \return                 The set of valid pricing dates in the period.
*/
std::set<QuantLib::Date> pricingDates(const QuantLib::Date& start, const QuantLib::Date& end,
    const QuantLib::Calendar& pricingCalendar, bool excludeStart, bool includeEnd, bool useBusinessDays = true);

/*! \brief Check if a date is a pricing date.

    \param d                The date that we wish to check.
    \param pricingCalendar  The pricing calendar used to determine valid dates
    \param useBusinessDays  Set to \c true if \p pricingCalendar business dates are to be considered valid pricing
                            dates and \p false if \p pricingCalendar holidays are to be considered valid pricing
                            dates. The latter case is unusual but is useful for some electricity futures e.g. ICE PW2
                            contract which averages over weekends and non-NERC business days.

    \return                 Returns \c true if \p d is a pricing date and \c false otherwise.
*/
bool isPricingDate(const QuantLib::Date& d, const QuantLib::Calendar& pricingCalendar, bool useBusinessDays = true);

}

#endif
