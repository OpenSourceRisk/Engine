/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_futureexpirycalculator_i
#define qle_futureexpirycalculator_i

%include types.i
%include instruments.i
%include settings.i
%include date.i

%{
using QuantExt::FutureExpiryCalculator;
%}

%shared_ptr(FutureExpiryCalculator)
class FutureExpiryCalculator {
  public:
    virtual ~FutureExpiryCalculator() {}

    virtual QuantLib::Date nextExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                                      QuantLib::Natural offset = 0, bool forOption = false) = 0;
    virtual QuantLib::Date priorExpiry(bool includeExpiry = true,
                                       const QuantLib::Date& referenceDate = QuantLib::Date(),
                                       bool forOption = false) = 0;
    virtual QuantLib::Date expiryDate(const QuantLib::Date& contractDate, QuantLib::Natural monthOffset = 0,
        bool forOption = false) = 0;
    virtual QuantLib::Date contractDate(const QuantLib::Date& expiryDate) = 0;
    virtual QuantLib::Date applyFutureMonthOffset(const QuantLib::Date& contractDate, Natural futureMonthOffset) = 0;

};

#endif
