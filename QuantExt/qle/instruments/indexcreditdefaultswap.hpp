/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008 StatPro Italia srl

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

/*! \file indexcreditdefaultswap.hpp
    \brief Index Credit default swap
*/

#ifndef quantext_index_credit_default_swap_hpp
#define quantext_index_credit_default_swap_hpp

#include <qle/instruments/creditdefaultswap.hpp>

#include <ql/cashflow.hpp>
#include <ql/default.hpp>
#include <ql/instrument.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantLib {
class YieldTermStructure;
class Claim;
}

namespace QuantExt {

class IndexCreditDefaultSwap : public CreditDefaultSwap {
public:
    class arguments;
    class results;
    class engine;

    IndexCreditDefaultSwap(Protection::Side side, Real notional, std::vector<Real> underlyingNotionals, Rate spread,
                           const Schedule& schedule, BusinessDayConvention paymentConvention,
                           const DayCounter& dayCounter, bool settlesAccrual = true, bool paysAtDefaultTime = true,
                           const Date& protectionStart = Date(),
                           const boost::shared_ptr<Claim>& claim = boost::shared_ptr<Claim>())
        : CreditDefaultSwap(side, notional, spread, schedule, paymentConvention, dayCounter, settlesAccrual,
                            paysAtDefaultTime, protectionStart, claim),
          underlyingNotionals_(underlyingNotionals) {}

    IndexCreditDefaultSwap(Protection::Side side, Real notional, std::vector<Real> underlyingNotionals, Rate upfront,
                           Rate spread, const Schedule& schedule, BusinessDayConvention paymentConvention,
                           const DayCounter& dayCounter, bool settlesAccrual = true, bool paysAtDefaultTime = true,
                           const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                           const boost::shared_ptr<Claim>& claim = boost::shared_ptr<Claim>())
        : CreditDefaultSwap(side, notional, upfront, spread, schedule, paymentConvention, dayCounter, settlesAccrual,
                            paysAtDefaultTime, protectionStart, upfrontDate, claim),
          underlyingNotionals_(underlyingNotionals) {}

    //@}
    //! \name Inspectors
    //@{
    const std::vector<Real>& underlyingNotionals() const;
    //@}

    //@}
    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments*) const;
    //@}

protected:
    std::vector<Real> underlyingNotionals_;
};

class IndexCreditDefaultSwap::arguments : public virtual CreditDefaultSwap::arguments {
public:
    std::vector<Real> underlyingNotionals;
};

class IndexCreditDefaultSwap::results : public CreditDefaultSwap::results {};

class IndexCreditDefaultSwap::engine
    : public GenericEngine<IndexCreditDefaultSwap::arguments, IndexCreditDefaultSwap::results> {};
}

#endif
