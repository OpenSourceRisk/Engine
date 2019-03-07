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


/*! \file blackbondoptionengine.hpp
\brief Black bond option engine
\ingroup engines
*/

#ifndef quantext_black_bond_option_engine_hpp
#define quantext_black_bond_option_engine_hpp

#include <qle/instruments/bondoption.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace QuantExt {
    using namespace QuantLib;

    //! Black-formula bond option engine
    class BlackBondOptionEngine : public BondOption::engine {
    public:
        //! volatility is the quoted fwd yield volatility, not price vol
        BlackBondOptionEngine(
            const Handle<SwaptionVolatilityStructure>& yieldVolStructure,
            const Handle<YieldTermStructure>& discountCurve);
        void calculate() const;
    private:
        Handle<SwaptionVolatilityStructure> volatility_;
        Handle<YieldTermStructure> discountCurve_;
        // present value of all coupons paid during the life of option
        Real spotIncome() const;
        // converts the yield volatility into a forward price volatility
        Volatility forwardPriceVolatility() const;
    };
} // namespace QuantExt

#endif

