/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This file is supposed to be part of the QuantLib library eventually,
   in the meantime we provide is as part of the QuantExt library. */

/*
 Copyright (C) 2014 Michael von den Driesch

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file bacheliercapfloorengine.hpp
    \brief Bachelier-Black-formula cap/floor engine
*/

#ifndef quantext_pricers_bachelier_capfloor_hpp
#define quantext_pricers_bachelier_capfloor_hpp

#include <qle/termstructures/optionletvolatilitystructure.hpp>
#include <ql/instruments/capfloor.hpp>

namespace QuantLib {
class Quote;
}

namespace QuantExt {

    //! Bachelier-Black-formula cap/floor engine
    /*! \ingroup capfloorengines */
    class BachelierCapFloorEngine : public CapFloor::engine {
      public:
        BachelierCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                Volatility vol,
                                const DayCounter& dc = Actual365Fixed());
        BachelierCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                const Handle<Quote>& vol,
                                const DayCounter& dc = Actual365Fixed());
        BachelierCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                const Handle<OptionletVolatilityStructure>& vol);
        void calculate() const;
        Handle<YieldTermStructure> termStructure() { return discountCurve_; }
        Handle<OptionletVolatilityStructure> volatility() { return vol_; }
      private:
        Handle<YieldTermStructure> discountCurve_;
        Handle<OptionletVolatilityStructure> vol_;
    };

}

#endif
