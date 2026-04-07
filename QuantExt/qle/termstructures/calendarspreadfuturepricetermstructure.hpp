/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/calendarspreadfuturepricetermstructure.hpp
    \brief Calendar spread future price term structure
*/

#pragma once

#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Calendar spread future price term structure
/*! Given a commodity futures price curve and a future expiry calculator,
    this term structure returns the calendar spread price at any given date.

    For a given date \c d the spread is computed as:
    \f[
        \Pi(d) = P(\text{nextExpiry}(d, 0)) - P(\text{nextExpiry}(d, \text{offset}))
    \f]
    where \c offset is the number of contract periods separating the near and far legs.

    \ingroup termstructures
*/
class CalendarSpreadFuturePriceTermStructure : public PriceTermStructure {
public:
    CalendarSpreadFuturePriceTermStructure(
        const QuantLib::Handle<PriceTermStructure>& baseCurve,
        const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& expiryCalculator,
        QuantLib::Natural calendarSpreadOffset = 1);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    const QuantLib::Date& referenceDate() const override;
    QuantLib::DayCounter dayCounter() const override;
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Real price(QuantLib::Time t, bool extrapolate = false) const override;
    QuantLib::Real price(const QuantLib::Date& d, bool extrapolate = false) const override;
    QuantLib::Time minTime() const override;
    const QuantLib::Currency& currency() const override;
    std::vector<QuantLib::Date> pillarDates() const override;
    //@}

    //! \name LazyObject interface
    //@{
    void deepUpdate() override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::Handle<PriceTermStructure>& baseCurve() const;
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& expiryCalculator() const;
    QuantLib::Natural calendarSpreadOffset() const;
    //@}

protected:
    QuantLib::Real priceImpl(QuantLib::Time t) const override { return 0.0; }

private:
    QuantLib::Handle<PriceTermStructure> baseCurve_;
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> expiryCalculator_;
    QuantLib::Natural calendarSpreadOffset_;
};

} // namespace QuantExt
