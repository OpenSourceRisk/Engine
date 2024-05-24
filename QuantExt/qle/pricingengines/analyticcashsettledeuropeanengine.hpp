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

/*! \file qle/pricingengines/analyticcashsettledeuropeanengine.hpp
    \brief pricing engine for cash settled European vanilla options.
    \ingroup engines
*/

#ifndef quantext_analytic_cash_settled_european_engine_hpp
#define quantext_analytic_cash_settled_european_engine_hpp

#include <ql/processes/blackscholesprocess.hpp>
#include <qle/pricingengines/analyticeuropeanforwardengine.hpp>
#include <qle/instruments/cashsettledeuropeanoption.hpp>

namespace QuantExt {

//! Pricing engine for cash settled European vanilla options using analytical formulae
/*! \ingroup engines
 */
class AnalyticCashSettledEuropeanEngine : public CashSettledEuropeanOption::engine {
public:
    /*! The risk-free rate in the given process \p bsp is used for both forecasting and discounting.
     */
    AnalyticCashSettledEuropeanEngine(const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& bsp);

    /*! As usual, the risk-free rate from the given process \p bsp is used for forecasting the forward price. The
        \p discountCurve is used for discounting.
    */
    AnalyticCashSettledEuropeanEngine(const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& bsp,
                                      const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve);

    //! \name PricingEngine interface
    //@{
    void calculate() const override;
    //@}

private:
    //! Underlying engine that does the work.
    mutable QuantExt::AnalyticEuropeanForwardEngine underlyingEngine_;
    //! Underlying process
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> bsp_;
    //! Curve for discounting cashflows
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

} // namespace QuantExt

#endif
