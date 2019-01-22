/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
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

/*! \file calibrationhelper.hpp
    \brief Calibration helper class
*/

#ifndef quantext_cms_calibration_helper_h
#define quantext_cms_calibration_helper_h

#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/cashflows/lognormalcmsspreadpricer.hpp>
#include <list>

#include <iostream>

namespace QuantExt {
using namespace QuantLib;
    class CmsCapHelper : public LazyObject, public CalibrationHelperBase {
      public:
        CmsCapHelper(Date asof, boost::shared_ptr<SwapIndex>& index1, boost::shared_ptr<SwapIndex>& index2, 
                            const Handle<Quote>& price, const Handle<Quote>& correlation, const Period& length,
                            boost::shared_ptr<FloatingRateCouponPricer>& pricer, boost::shared_ptr<QuantLib::CmsCouponPricer>& cmsPricer)
        : asof_(asof), index1_(index1), index2_(index2), marketValue_(price->value()), correlation_(correlation), length_(length), pricer_(pricer), cmsPricer_(cmsPricer) {

            registerWith(correlation_);
        }

        void performCalculations() const ;

        //! returns the actual price of the instrument (from volatility)
        QuantLib::Real marketValue() const { return marketValue_; }

        //! returns the price of the instrument according to the model
        QuantLib::Real modelValue() const;

        //! returns the error resulting from the model valuation
        QuantLib::Real calibrationError() {return marketValue() - modelValue(); }


      protected:
        Date asof_;
        boost::shared_ptr<SwapIndex> index1_;
        boost::shared_ptr<SwapIndex> index2_;
        Real marketValue_;
        Handle<Quote> correlation_;
        Period length_;
        boost::shared_ptr<FloatingRateCouponPricer> pricer_;
        boost::shared_ptr<QuantLib::CmsCouponPricer> cmsPricer_;

      private:
        mutable boost::shared_ptr<QuantLib::Swap> cap_;
    };

}


#endif
