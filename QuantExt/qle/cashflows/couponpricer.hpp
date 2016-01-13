/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/cashflows/couponpricer.hpp
    \brief Utility functions for setting coupon pricers on legs
*/

#ifndef quantext_coupon_pricer_hpp
#define quantext_coupon_pricer_hpp

#include <boost/shared_ptr.hpp>

#include <ql/cashflows/couponpricer.hpp>

#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

    /*! Amend QuantLib BlackIborCouponPricer by allowing the forward rate to be floored
        at a predefined level before being used in the Black formula
    */
    class BlackIborCouponPricerExt : public BlackIborCouponPricer {
      public:
        BlackIborCouponPricerExt(const Handle<OptionletVolatilityStructure>& v = 
            Handle<OptionletVolatilityStructure>(), bool floorForwards = false, Rate floor = 0.0010)
        : BlackIborCouponPricer(v), floorForwards_(floorForwards), floor_(floor) {};

      protected:
        Rate adjustedFixing(Rate fixing = Null<Rate>()) const;
     
      private:
        bool floorForwards_;
        Rate floor_;
    };

    void setCouponPricer(const Leg& leg, const boost::shared_ptr<FloatingRateCouponPricer>&);

    void setCouponPricers(const Leg& leg, const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >&);
}

#endif
