/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*!
  \file il/indexes/adjustedswapindex.cpp
  \brief swap index with convexity adjustment
*/

#include <qle/indexes/adjustedswapindex.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/instruments/makevanillaswap.hpp>

namespace QuantExt {
    
  //The adjusted swap index forecasts the forward CMS rate adjusted by the convexity 
  //quantity described in Hull (6th ed.), p. 637. The first and second derivative
  //of the value of the swap's fixed leg is calculated using the duration and 
  //convexity functions of the CashFlows class.

  Rate AdjustedSwapIndex::forecastFixing(const Date& fixingDate) const {
    
    Rate fwd = underlyingSwap(fixingDate)->fairRate();
    //    cout << vola_ << "  " << fixingDate <<  " " << name() << " " << fwd << endl;
    if (!vola_) return fwd;

    PTR<VanillaSwap> underlying = MakeVanillaSwap(tenor_, iborIndex_, fwd)
      .withEffectiveDate(valueDate(fixingDate))
      .withFixedLegCalendar(fixingCalendar())
      .withFixedLegDayCount(dayCounter_)
      .withFixedLegTenor(fixedLegTenor_)
      .withFixedLegConvention(fixedLegConvention_)
      .withFixedLegTerminationDateConvention(fixedLegConvention_);

    Date asof = Settings::instance().evaluationDate();
    if (fixingDate <= asof) return fwd;

    Leg leg = underlying->fixedLeg();

    Date start = underlying->startDate();
    Time expiry = dayCounter_.yearFraction(asof, start);
    Date matDate = leg[leg.size()-1]->date();
    Time maturity = dayCounter_.yearFraction(start, matDate);
    Real swapVol = vola_->volatility(expiry, maturity, 0);

    InterestRate ir = InterestRate(fwd, dayCounter_, Compounded, fixedLegTenor().frequency());
    Real duration  = -CashFlows::duration(leg, ir, Duration::Modified, false);
    Real convexity = CashFlows::convexity(leg, ir, false);

    return fwd * (1. - 0.5 * expiry * swapVol * swapVol * fwd * convexity / duration);
  }

}
