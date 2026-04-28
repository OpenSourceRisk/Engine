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

/*! \file qle/pricingengines/cashsettledamericanengine.hpp
    \brief pricing engine for cash settled American vanilla options.
    \ingroup engines
*/

#ifndef quantext_cash_settled_american_engine_hpp
#define quantext_cash_settled_american_engine_hpp

#include <ql/pricingengine.hpp>
#include <qle/instruments/cashsettledamericanoption.hpp>

namespace QuantExt {

//! Wrapper engine for cash settled American options
/*! Wraps an existing American option engine and handles:
    - FX conversion for settlement in a different currency
    - Discounting adjustments for deferred payment dates
    \ingroup engines
*/
class CashSettledAmericanEngine : public CashSettledAmericanOption::engine {
public:
    CashSettledAmericanEngine(const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& underlyingEngine,
                              const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve);

    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> underlyingEngine_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

} // namespace QuantExt

#endif
