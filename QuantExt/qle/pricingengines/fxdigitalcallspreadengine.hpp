/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/fxdigitalcallspreadengine.hpp
    \brief Call-spread replication engine for cash-settled European FX digital options
    \ingroup engines

    Prices an FX digital (cash-or-nothing) option as a call spread on two vanilla
    FX options:
        Digital(K) ~ cash * [C(K - eps/2) - C(K + eps/2)] / eps   (call)
        Digital(K) ~ cash * [P(K + eps/2) - P(K - eps/2)] / eps   (put)
    The two vanilla FX option prices are computed with the Garman-Kohlhagen
    AnalyticEuropeanEngine, so the market smile is naturally incorporated.

    Cash settlement is handled identically to AnalyticCashSettledEuropeanEngine:
    - if the expiry date has passed the payout is fixed and discounted from the
      payment date;
    - if the expiry date is in the future the call spread is priced assuming
      payment on the option expiry date and the delayed payment is accounted for
      by multiplying with the forward discount factor between the option expiry
      date and the payment date.
*/

#pragma once

#include <ql/processes/blackscholesprocess.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>

namespace QuantExt {

//! Prices a cash-settled FX digital via call-spread replication on two vanilla FX options.
/*! \ingroup engines
 */
class FxDigitalCallSpreadEngine : public CashSettledEuropeanOption::engine {
public:
    /*! \param bsp         Garman-Kohlhagen process for the FX pair, providing the
                           FX spot, the foreign and domestic discount curves and
                           the FX vol surface (with smile).
        \param flipResults Flip results for the inverse FX quotation convention.
        \param eps         Width of the call spread (in FX rate terms).
    */
    FxDigitalCallSpreadEngine(const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& bsp,
                              const bool flipResults = false, QuantLib::Real eps = 1.0e-4);

    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> bsp_;
    bool flipResults_;
    QuantLib::Real eps_;
};

} // namespace QuantExt
