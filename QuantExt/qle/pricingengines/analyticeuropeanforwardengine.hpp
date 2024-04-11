/*
 Copyright (C) 2002, 2003, 2004 Ferdinando Ametrano
 Copyright (C) 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2007 StatPro Italia srl

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


/*! \file analyticeuropeanforwardengine.hpp
    \brief Analytic European Forward engine
*/

#ifndef quantext_analytic_european_forward_engine_hpp
#define quantext_analytic_european_forward_engine_hpp

#include <qle/instruments/vanillaforwardoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

    //! Pricing engine for European vanilla forward options using analytical formulae
    /*! \ingroup vanillaengines */
    class AnalyticEuropeanForwardEngine : public VanillaForwardOption::engine {
      public:
        /*! This constructor triggers the usual calculation, in which
            the risk-free rate in the given process is used for both
            forecasting and discounting.
        */
        explicit AnalyticEuropeanForwardEngine(
                    const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>&);

        /*! This constructor allows to use a different term structure
            for discounting the payoff. As usual, the risk-free rate
            from the given process is used for forecasting the forward
            price.
        */
        AnalyticEuropeanForwardEngine(
             const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& process,
             const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve);
        void calculate() const override;
      private:
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> process_;
        QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    };

}


#endif
