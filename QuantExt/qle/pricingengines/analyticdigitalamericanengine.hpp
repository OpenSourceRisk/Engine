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

/*! \file qle/pricingengines/analyticdigitalamericanengine.hpp
    \brief Wrapper of QuantLib analytic digital American option engine to allow for flipping back some of the additional
   results in the case of FX instruments where the trade builder may have inverted the underlying pair
*/

#ifndef quantext_analytic_digital_american_engine_hpp
#define quantext_analytic_digital_american_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticdigitalamericanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Analytic pricing engine for American vanilla options with digital payoff

class AnalyticDigitalAmericanEngine : public QuantLib::AnalyticDigitalAmericanEngine {
public:
    AnalyticDigitalAmericanEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                  const QuantLib::Date& payDate, const bool flipResults = false)
        : QuantLib::AnalyticDigitalAmericanEngine(process), process_(std::move(process)), payDate_(payDate),
          flipResults_(flipResults) {
        registerWith(process_);
    }
    void calculate() const override;
    virtual bool knock_in() const override { return true; }

private:
    ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    QuantLib::Date payDate_;
    bool flipResults_;
};

//! Analytic pricing engine for American Knock-out options with digital payoff

class AnalyticDigitalAmericanKOEngine : public AnalyticDigitalAmericanEngine {
public:
    AnalyticDigitalAmericanKOEngine(const ext::shared_ptr<GeneralizedBlackScholesProcess>& engine,
                                    const QuantLib::Date& payDate, const bool flipResults = false)
        : AnalyticDigitalAmericanEngine(engine, payDate, flipResults) {}
    bool knock_in() const override { return false; }
};

} // namespace QuantExt

#endif