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

/*! \file qle/instruments/varianceswap.hpp
    \brief Variance swap
*/

#pragma once

#include <ql/instruments/varianceswap.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Variance swap
/*! \warning This class does not manage seasoned variance swaps.

    \ingroup instruments
*/
class VarianceSwap2 : public QuantLib::VarianceSwap {
public:
    class arguments;
    class results;
    class engine;
    VarianceSwap2(Position::Type position, Real strike, Real notional, const Date& startDate, const Date& maturityDate,
                  const Calendar& calendar, bool addPastDividends);
    //! \name Additional interface
    //@{
    // inspectors
    Calendar calendar() const;
    bool addPastDividends() const;
    //@}
    // other
    void setupArguments(PricingEngine::arguments* args) const override;

protected:
    // data members
    Calendar calendar_;
    bool addPastDividends_;
};

//! %Arguments for forward fair-variance calculation
class VarianceSwap2::arguments : public virtual QuantLib::VarianceSwap::arguments {
public:
    arguments() : calendar(NullCalendar()), addPastDividends(false) {}
    Calendar calendar;
    bool addPastDividends;
};

//! %Results from variance-swap calculation
class VarianceSwap2::results : public QuantLib::VarianceSwap::results {};

//! base class for variance-swap engines
class VarianceSwap2::engine : public GenericEngine<QuantExt::VarianceSwap2::arguments, QuantExt::VarianceSwap2::results> {
};

// inline definitions

inline Calendar VarianceSwap2::calendar() const { return calendar_; }

inline bool VarianceSwap2::addPastDividends() const { return addPastDividends_; }

} // namespace QuantExt
