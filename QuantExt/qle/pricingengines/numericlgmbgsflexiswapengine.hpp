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

/*! \file qle/pricingengines/numericlgmbgsflexiswapengine.hpp
    \brief numeric engine for balance guaranteed swaps using a flexi swap proxy in the LGM model
*/

#pragma once

#include <qle/instruments/balanceguaranteedswap.hpp>
#include <qle/pricingengines/numericlgmflexiswapengine.hpp>

namespace QuantExt {

//! Numerical engine for balance guaranteed swaps using a flexi swap proxy in the LGM model
/*! Two notional schedules are constructed using a simple prepayment model with rates minCpr and
  maxCpr. These two schedules define a lower / upper notional bounds of a flexi swap. The NPV of this
  flexi swap is by definition the NPV of the BGS itself.

  The prepayment model assumes that prepayments amortise the tranches in the order of their seniority.

  Notice that prepayments start in the first period of the tranche nominal schedule that has a start date
  that lies in the future. Therefore the tranche notionals in the BGS should contain past (known) prepayments
  already, only for future periods the notionals should be given under a zero CPR assumption.
*/

class NumericLgmBgsFlexiSwapEngine
    : public GenericEngine<BalanceGuaranteedSwap::arguments, BalanceGuaranteedSwap::results>,
      public NumericLgmFlexiSwapEngineBase {
public:
    NumericLgmBgsFlexiSwapEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                 const Real sx, const Size nx, const Handle<Quote>& minCpr, const Handle<Quote>& maxCpr,
                                 const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                 const Method method = Method::Automatic, const Real singleSwaptionThreshold = 20.0);

private:
    void calculate() const override;
    const Handle<Quote> minCpr_, maxCpr_;

}; // NumericLgmBgsFlexiSwapEngine

} // namespace QuantExt
