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

/*! \file analyticdigitalamericanengine.hpp
    \brief Wrapper of QuantLib analytic digital American option engine to allow for flipping back some of the additional
   results in the case of FX instruments where the trade builder may have inverted the underlying pair
*/

#pragma once

#include <ql/instruments/vanillaoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/pricingengines/vanilla/analyticdigitalamericanengine.hpp>

namespace QuantExt {

    using namespace QuantLib;

    //! Analytic pricing engine for American vanilla options with digital payoff
    /*! \ingroup vanillaengines

        \todo add more greeks (as of now only delta and rho available)

        \test
        - the correctness of the returned value in case of
          cash-or-nothing at-hit digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          asset-or-nothing at-hit digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          cash-or-nothing at-expiry digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          asset-or-nothing at-expiry digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned greeks in case of
          cash-or-nothing at-hit digital payoff is tested by
          reproducing numerical derivatives.
    */
    class AnalyticDigitalAmericanEngine : public QuantLib::AnalyticDigitalAmericanEngine {
      public:
        AnalyticDigitalAmericanEngine(const boost::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                      const bool flipResults = false)
              : QuantLib::AnalyticDigitalAmericanEngine(process), flipResults_(flipResults){};
        void calculate() const override;
        virtual bool knock_in() const override {
           return true;
        }
      private:
        boost::shared_ptr<GeneralizedBlackScholesProcess> process_;
        bool flipResults_;
    };

    //! Analytic pricing engine for American Knock-out options with digital payoff
    /*! \ingroup vanillaengines

        \todo add more greeks (as of now only delta and rho available)

        \test
        - the correctness of the returned value in case of
          cash-or-nothing at-hit digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          asset-or-nothing at-hit digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          cash-or-nothing at-expiry digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned value in case of
          asset-or-nothing at-expiry digital payoff is tested by
          reproducing results available in literature.
        - the correctness of the returned greeks in case of
          cash-or-nothing at-hit digital payoff is tested by
          reproducing numerical derivatives.
    */
    class AnalyticDigitalAmericanKOEngine : 
                              public AnalyticDigitalAmericanEngine {
      public:
        AnalyticDigitalAmericanKOEngine(const boost::shared_ptr<GeneralizedBlackScholesProcess>& engine,
                                        const bool flipResults = false)
            : AnalyticDigitalAmericanEngine(engine, flipResults) {}
        bool knock_in() const override { return false; }
    };

} // namespace QuantExt
