/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#ifndef ored_conventionsbasedfutureexpiry_i
#define ored_conventionsbasedfutureexpiry_i

%include qle_futureexpirycalculator.i

%{
using std::string;
using std::map;

using ore::data::ConventionsBasedFutureExpiry;

using QuantExt::FutureExpiryCalculator;
using ore::data::CommodityFutureConvention;
%}

%shared_ptr(ConventionsBasedFutureExpiry)
class ConventionsBasedFutureExpiry : public QuantExt::FutureExpiryCalculator {
  public:
    ConventionsBasedFutureExpiry(const std::string& commName, QuantLib::Size maxIterations = 10);
    ConventionsBasedFutureExpiry(const CommodityFutureConvention& convention, QuantLib::Size maxIterations = 10);

    QuantLib::Date nextExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                              QuantLib::Natural offset = 0, bool forOption = false) override;

    QuantLib::Date priorExpiry(bool includeExpiry = true, const QuantLib::Date& referenceDate = QuantLib::Date(),
                               bool forOption = false) override;

    QuantLib::Date expiryDate(const QuantLib::Date& contractDate, QuantLib::Natural monthOffset = 0,
                              bool forOption = false) override;

    QuantLib::Date contractDate(const QuantLib::Date& expiryDate) override;

    QuantLib::Date applyFutureMonthOffset(const QuantLib::Date& contractDate, QuantLib::Natural futureMonthOffset) override;

    const CommodityFutureConvention& commodityFutureConvention() const;

    QuantLib::Size maxIterations() const;

};

#endif