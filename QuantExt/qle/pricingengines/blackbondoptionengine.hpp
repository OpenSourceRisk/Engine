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

/*
This file is part of QuantLib, a free-software/open-source library
for financial quantitative analysts and developers - http://quantlib.org/

QuantLib is free software: you can redistribute it and/or modify it
under the terms of the QuantLib license.  You should have received a
copy of the license along with this program; if not, please email
<quantlib-dev@lists.sf.net>. The license is also available online at
<http://quantlib.org/license.shtml>.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file blackbondoptionengine.hpp
\brief Black credit default swap option engine, with handling
of upfront amount and exercise before CDS start
\ingroup engines
*/

#ifndef quantext_black_bond_option_engine_hpp
#define quantext_black_bond_option_engine_hpp

#include <ql/experimental/callablebonds/callablebond.hpp>
#include <ql/experimental/callablebonds/blackcallablebondengine.hpp>
#include <ql/experimental/callablebonds/callablebondvolstructure.hpp>


namespace QuantExt {
    using namespace QuantLib;

    //! Black-formula bond option engine
    class BlackBondOptionEngine : public BlackCallableFixedRateBondEngine {
    public:
        //! volatility is the quoted fwd yield volatility, not price vol
        BlackBondOptionEngine(
            const Handle<Quote>& fwdYieldVol,
            const Handle<YieldTermStructure>& discountCurve, const bool& isCallableBond);
        //! volatility is the quoted fwd yield volatility, not price vol
        BlackBondOptionEngine(
            const Handle<CallableBondVolatilityStructure>& yieldVolStructure,
            const Handle<YieldTermStructure>& discountCurve, const bool& isCallableBond);
        void calculate() const;
    private:
        Handle<CallableBondVolatilityStructure> volatility_;
        Handle<YieldTermStructure> discountCurve_;
        bool isCallableBond_;
        // present value of all coupons paid during the life of option
        Real spotIncome() const;
        // converts the yield volatility into a forward price volatility
        Volatility forwardPriceVolatility() const;
    };
} // namespace QuantExt

#endif

