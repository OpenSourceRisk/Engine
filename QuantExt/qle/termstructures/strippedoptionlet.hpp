/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This file is supposed to be part of the QuantLib library eventually,
   in the meantime we provide is as part of the QuantExt library. */

/*
 Copyright (C) 2008 Ferdinando Ametrano
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2015 Peter Caspers

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

/*! \file strippedoptionlet.hpp
*/

#ifndef quantext_strippedoptionlet_hpp
#define quantext_strippedoptionlet_hpp

#include <qle/termstructures/strippedoptionletbase.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/quote.hpp>

namespace QuantExt {

    /*! Helper class to wrap in a StrippedOptionletBase object a matrix of
        exogenously calculated optionlet (i.e. caplet/floorlet) volatilities
        (a.k.a. forward-forward volatilities).
    */
    class StrippedOptionlet : public StrippedOptionletBase {
      public:
        StrippedOptionlet(Natural settlementDays,
                          const Calendar& calendar,
                          BusinessDayConvention bdc,
                          const boost::shared_ptr<IborIndex>& iborIndex,
                          const std::vector<Date>& optionletDates,
                          const std::vector<Rate>& strikes,
                          const std::vector<std::vector<Handle<Quote> > >&,
                          const DayCounter& dc,
                          VolatilityType type = ShiftedLognormal,
                          Real displacement = 0.0);
        //! \name StrippedOptionletBase interface
        //@{
        const std::vector<Rate>& optionletStrikes(Size i) const;
        const std::vector<Volatility>& optionletVolatilities(Size i) const;

        const std::vector<Date>& optionletFixingDates() const;
        const std::vector<Time>& optionletFixingTimes() const;
        Size optionletMaturities() const;

        const std::vector<Rate>& atmOptionletRates() const;

        DayCounter dayCounter() const;
        Calendar calendar() const;
        Natural settlementDays() const;
        BusinessDayConvention businessDayConvention() const;
        //@}
        const VolatilityType volatilityType() const;
        const Real displacement() const;
      private:
        void checkInputs() const;
        void registerWithMarketData();
        void performCalculations() const;

        Calendar calendar_;
        Natural settlementDays_;
        BusinessDayConvention businessDayConvention_;
        DayCounter dc_;
        boost::shared_ptr<IborIndex> iborIndex_;
        VolatilityType type_;
        Real displacement_;

        Size nOptionletDates_;
        std::vector<Date> optionletDates_;
        std::vector<Time> optionletTimes_;
        mutable std::vector<Rate> optionletAtmRates_;
        std::vector<std::vector<Rate> > optionletStrikes_;
        Size nStrikes_;

        std::vector<std::vector<Handle<Quote> > > optionletVolQuotes_;
        mutable std::vector<std::vector<Volatility> > optionletVolatilities_;
    };

}

#endif
