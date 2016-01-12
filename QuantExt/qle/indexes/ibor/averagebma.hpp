/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file averagebma.hpp
    \brief Average BMA Index presented as an Ibor Index
*/

#ifndef quantext_bma_ibor_hpp
#define quantext_bma_ibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/currencies/america.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/calendars/unitedstates.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! %BMA Index wrapped in an QuantLib::IborIndex
    
    /*! Use this index when you require BMA fixings in an IborIndex.

        Note that the fixings for this index should be rolling average BMA fixings over a 6M (?)
        Period, that way BMA swaps can be priced quickly without the need to look up multiple fixings.

        Calendar and DayCounter copied from QuantLib::BMAIndex.
        BusinessDayConvention is a guess.
    */
    class AverageBMA: public IborIndex {
      public:
        AverageBMA(const Period& tenor, const Handle<YieldTermStructure>& h =
            Handle<YieldTermStructure>())
        : IborIndex("AverageBMA", tenor, 0, USDCurrency(), UnitedStates(UnitedStates::NYSE),
              ModifiedFollowing, false, ActualActual(ActualActual::ISDA), h) {}
    };
}

#endif
