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

 Copyright (C) 2003, 2004 Ferdinando Ametrano
 Copyright (C) 2007 StatPro Italia srl

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

/*! \file baroneadesiwhaleyengine.hpp
    \brief Barone-Adesi and Whaley approximation engine
*/

#ifndef quantext_barone_adesi_whaley_engine_hpp
#define quantext_barone_adesi_whaley_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

/*! Barone-Adesi and Whaley pricing engine for American options (1987)
    This QuantExt class is a copy of the class with the same name in
    QuantLib v1.14 with the following change
    - Added handling for put option where early exercise is never optimal */
/*! \ingroup vanillaengines

    \test the correctness of the returned value is tested by
            reproducing results available in literature.
*/
class BaroneAdesiWhaleyApproximationEngine : public QuantLib::VanillaOption::engine {
public:
    BaroneAdesiWhaleyApproximationEngine(const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>&);
    static QuantLib::Real criticalPrice(const QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff>& payoff,
                                        QuantLib::DiscountFactor riskFreeDiscount,
                                        QuantLib::DiscountFactor dividendDiscount, QuantLib::Real variance,
                                        QuantLib::Real tolerance = 1e-6);
    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> process_;
};

} // namespace QuantExt

#endif
