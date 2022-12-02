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

/*! \file qle/pricingengines/analyticeuropeanengine.hpp
    \brief Wrapper of QuantLib analytic European engine to allow for flipping back some of the additional
   results in the case of FX instruments where the trade builder may have inverted the underlying pair
*/

#pragma once

#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Pricing engine for European vanilla options using analytical formulae

class AnalyticEuropeanEngine : public QuantLib::AnalyticEuropeanEngine {
public:
    explicit AnalyticEuropeanEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp, const bool flipResults = false)
        : QuantLib::AnalyticEuropeanEngine(gbsp), flipResults_(flipResults) {}

    AnalyticEuropeanEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                           Handle<YieldTermStructure> discountCurve, const bool flipResults = false)
        : QuantLib::AnalyticEuropeanEngine(process, discountCurve), flipResults_(flipResults) {}
    void calculate() const override;

private:
    ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<YieldTermStructure> discountCurve_;
    bool flipResults_;
};

} // namespace QuantExt
