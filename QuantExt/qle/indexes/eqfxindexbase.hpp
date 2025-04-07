/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/eqfxindexbase.hpp
    \brief A common base class for the FX and Equity Indices. Provides a forecast fixing method for time
    so the indices can be used in termstructures that use time lookup.
    \ingroup indexes
*/

#ifndef quantext_eqfxindexbase_hpp
#define quantext_eqfxindexbase_hpp

#include <ql/currency.hpp>
#include <ql/handle.hpp>
#include <ql/index.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Equity Index
/*! \ingroup indexes */
class EqFxIndexBase : public Index {
public:
    virtual ~EqFxIndexBase() {}

    //! returns the fixing at the given time
    virtual Real forecastFixing(const Time& fixingTime) const = 0;

    //! returns a past fixing at the given date
    /*! the date passed as arguments must be the actual calendar
        date of the fixing; no settlement days must be used.
    */
    virtual Real pastFixing(const Date& fixingDate) const = 0;
};

} // namespace QuantExt
#endif
