/*
 Copyright (C) 2025 AcadiaSoft Inc.
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

/*! \file strippedoptionlet.hpp
*/

#pragma once

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionlet.hpp>

namespace QuantExt {

using namespace QuantLib;
using std::vector;

  /*! Helper class to wrap in a StrippedOptionlet object a OptionletVolatilityStructure
  */
  class StrippedOptionlet : public QuantLib::StrippedOptionlet {
    public:
      StrippedOptionlet(Natural settlementDays,
                        const Calendar& calendar,
                        BusinessDayConvention bdc,
                        ext::shared_ptr<IborIndex> iborIndex,
                        const vector<Date>& optionletDates,
                        const vector<Rate>& strikes,
                        const Handle<OptionletVolatilityStructure>& baseVol,
                        vector<vector<Handle<Quote>>> quotes,
                        DayCounter dc,
                        VolatilityType type = ShiftedLognormal,
                        Real displacement = 0.0,
                        const vector<Real>& atmOptionletRates = {});
      StrippedOptionlet(Natural settlementDays,
                        const Calendar& calendar,
                        BusinessDayConvention bdc,
                        ext::shared_ptr<IborIndex> iborIndex,
                        const vector<Date>& optionletDates,
                        const vector<vector<Rate>>& strikes,
                        const Handle<OptionletVolatilityStructure>& baseVol,
                        vector<vector<Handle<Quote>>> quotes,
                        DayCounter dc,
                        VolatilityType type = ShiftedLognormal,
                        Real displacement = 0.0,
                        const vector<Real>& atmOptionletRates = {});

    private:
      vector<vector<Handle<Quote>>> createEmptyQuotesMatrix(const vector<Date>& optionletDates,
                                                            const vector<vector<Rate>>& strikes);
      void performCalculations() const override;
      
      Handle<OptionletVolatilityStructure> baseVol_;
      vector<vector<Handle<Quote>>> quotes_;
      
  };

} // namespace QuantExt
