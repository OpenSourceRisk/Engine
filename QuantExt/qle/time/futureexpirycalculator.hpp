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

/*! \file qle/time/futureexpirycalculator.hpp
    \brief Base class for classes that perform date calculations for future contracts
*/

#ifndef quantext_future_expiry_calculator_hpp
#define quantext_future_expiry_calculator_hpp

#include <ql/settings.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {

//! Base class for classes that perform date calculations for future contracts
class FutureExpiryCalculator {
public:
    virtual ~FutureExpiryCalculator() {}

    /*! Given  a reference date, \p referenceDate, return the expiry date of the next futures contract relative to
        the reference date.

        The \p includeExpiry parameter controls what happens when the \p referenceDate is equal to the next contract's
        expiry date. If \p includeExpiry is \c true, the contract's expiry date is returned. If \p includeExpiry is
        \c false, the next succeeding contract's expiry is returned.

        If \p forOption is \c true, the next expiry for the option contract, as opposed to the future contract, is
        returned.
    */
    virtual QuantLib::Date nextExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                                      QuantLib::Natural offset = 0, bool forOption = false) = 0;

    /*! Given  a reference date, \p referenceDate, return the expiry date of the first futures contract prior to
        the reference date.

        The \p includeExpiry parameter controls what happens when the \p referenceDate is equal to the prior contract's
        expiry date. If \p includeExpiry is \c true, the contract's expiry date is returned. If \p includeExpiry is
        \c false, the next preceding contract's expiry is returned.

        If \p forOption is \c true, the prior expiry for the option contract, as opposed to the future contract, is
        returned.
    */
    virtual QuantLib::Date priorExpiry(bool includeExpiry = true,
                                       const QuantLib::Date& referenceDate = QuantLib::Date(),
                                       bool forOption = false) = 0;

    /*! Given a date, \p contractDate, return the future expiry date associated with that date.
        
        If the future contract has a frequency that is less than monthly, the next available future contract expiry 
        date will be returned. If \p forOption is \c true, the next available future option expiry is returned. For 
        future contracts that have a frequency that is less than monthly, the \p monthOffset parameter is ignored.
        
        If the future contract has a frequency that is monthly or greater, the contract's month and year is taken 
        to be the \p contractDate month and year, and the expiry date of the future contract that is \p monthOffset 
        number of months from that month's future contract is returned. If \p monthOffset is zero, the expiry date of 
        the future contract associated with that month and year is returned. If \p forOption is \c true, the expiry 
        date for the option contract, as opposed to the future contract, is returned.
    */
    virtual QuantLib::Date expiryDate(const QuantLib::Date& contractDate, QuantLib::Natural monthOffset = 0,
        bool forOption = false) = 0;

    /*! Given a futures (not option) expiry date, return a corresponding contract date */
    virtual QuantLib::Date contractDate(const QuantLib::Date& expiryDate) = 0;

    /*! apply a month offset to a given contract date and return the new contract date
        if the contract frequency is daily or weekly, the input contract date is not modified */
    virtual QuantLib::Date applyFutureMonthOffset(const QuantLib::Date& contractDate, Natural futureMonthOffset) = 0;
};

} // namespace QuantExt

#endif
