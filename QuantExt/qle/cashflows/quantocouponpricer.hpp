/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
 Copyright (C) 2008 Toyin Akin

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

/*! \file quantocouponpricer.hpp
    \brief quanto-adjusted coupon
*/

#ifndef quantext_coupon_quanto_pricer_hpp
#define quantext_coupon_quanto_pricer_hpp

#include <ql/cashflows/couponpricer.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {
using QuantLib::Handle;
using QuantLib::Null;
using QuantLib::Rate;
using QuantLib::Real;

/*! Same as QuantLib, but with fixed t1 computation (dc from vol ts instead
  of index) and extended to SLN and N vol types */
class BlackIborQuantoCouponPricer : public QuantLib::BlackIborCouponPricer {
public:
    BlackIborQuantoCouponPricer(const Handle<QuantLib::BlackVolTermStructure>& fxRateBlackVolatility,
                                const Handle<QuantLib::Quote>& underlyingFxCorrelation,
                                const Handle<QuantLib::OptionletVolatilityStructure>& capletVolatility)
        : BlackIborCouponPricer(capletVolatility), fxRateBlackVolatility_(fxRateBlackVolatility),
          underlyingFxCorrelation_(underlyingFxCorrelation) {
        registerWith(fxRateBlackVolatility_);
        registerWith(underlyingFxCorrelation_);
    }

protected:
    Rate adjustedFixing(Rate fixing = Null<Rate>()) const override;

private:
    Handle<QuantLib::BlackVolTermStructure> fxRateBlackVolatility_;
    Handle<QuantLib::Quote> underlyingFxCorrelation_;
};

} // namespace QuantExt

#endif
