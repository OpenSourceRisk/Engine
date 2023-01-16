/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file discountingcreditlinkedswapengine.hpp
    \brief credit linked swap pricing engine
*/

#pragma once

#include <qle/instruments/creditlinkedswap.hpp>

namespace QuantExt {

class DiscountingCreditLinkedSwapEngine : public CreditLinkedSwap::engine {
public:
    DiscountingCreditLinkedSwapEngine(const Handle<YieldTermStructure>& irCurve,
                                      const Handle<DefaultProbabilityTermStructure>& creditCurve,
                                      const Handle<Quote>& marketRecovery, const Size timeStepsPerYear,
                                      const bool generateAdditionalResults);
    void calculate() const override;

private:
    Handle<YieldTermStructure> irCurve_;
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    Handle<Quote> marketRecovery_;
    Size timeStepsPerYear_;
    bool generateAdditionalResults_;
};

} // namespace QuantExt
