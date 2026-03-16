/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

/*! \file vanillaforwardoption.hpp
    \brief Vanilla forward option on a single asset
*/

#ifndef quantext_vanilla_forward_option_hpp
#define quantext_vanilla_forward_option_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/payoffs.hpp>

namespace QuantExt {

    //! Vanilla Forward option on a single asset
    /*! \ingroup instruments */
    class VanillaForwardOption : public QuantLib::VanillaOption {
      public:
        class arguments;
        class engine;
        VanillaForwardOption(const QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff>& payoff,
                             const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise, const QuantLib::Date& forwardDate,
                             const QuantLib::Date& paymentDate)
            : VanillaOption(payoff, exercise), forwardDate_(forwardDate), paymentDate_(paymentDate) {}

        VanillaForwardOption(const QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff>& payoff,
            const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise, const QuantLib::Date& forwardDate)
                : VanillaOption(payoff, exercise), forwardDate_(forwardDate) {}

        void setupArguments(QuantLib::PricingEngine::arguments*) const override;
        
      private:
        QuantLib::Date forwardDate_;
        QuantLib::Date paymentDate_;
    };

    //! %Arguments for Vanilla Forward Option calculation
    class VanillaForwardOption::arguments : public VanillaOption::arguments {
    public:
        arguments() {}
        QuantLib::Date forwardDate;
        QuantLib::Date paymentDate;
    };

    inline void VanillaForwardOption::setupArguments(QuantLib::PricingEngine::arguments* args) const {
        VanillaOption::setupArguments(args);
        
        VanillaForwardOption::arguments* arguments =
            dynamic_cast<VanillaForwardOption::arguments*>(args);
        QL_REQUIRE(arguments != 0, "wrong argument type");

        arguments->forwardDate = forwardDate_;
        arguments->paymentDate = paymentDate_;
    }

    //! base class for swaption engines
    class VanillaForwardOption::engine : public QuantLib::GenericEngine<VanillaForwardOption::arguments, VanillaOption::results> {};
}


#endif

