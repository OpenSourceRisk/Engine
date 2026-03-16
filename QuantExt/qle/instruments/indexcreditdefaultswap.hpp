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

/*! \file qle/instruments/indexcreditdefaultswap.hpp
    \brief Index Credit default swap
*/

#ifndef quantext_index_credit_default_swap_hpp
#define quantext_index_credit_default_swap_hpp

#include <ql/instruments/creditdefaultswap.hpp>

#include <ql/cashflow.hpp>
#include <ql/default.hpp>
#include <ql/instrument.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {
class YieldTermStructure;
class Claim;
} // namespace QuantLib

namespace QuantExt {
using namespace QuantLib;

class IndexCreditDefaultSwap : public QuantLib::CreditDefaultSwap {
public:
    class arguments;
    class results;
    class engine;

    IndexCreditDefaultSwap(Protection::Side side, Real notional, std::vector<Real> underlyingNotionals, Rate spread,
                           const Schedule& schedule, BusinessDayConvention paymentConvention,
                           const DayCounter& dayCounter, bool settlesAccrual = true,
                           ProtectionPaymentTime protectionPaymentTime = atDefault,
                           const Date& protectionStart = Date(),
                           const QuantLib::ext::shared_ptr<Claim>& claim = QuantLib::ext::shared_ptr<Claim>(),
                           const DayCounter& lastPeriodDayCounter = DayCounter(),
                           bool rebatesAccrual = true,
                           const Date& tradeDate = Date(),
                           Natural cashSettlementDays = 3)
        : QuantLib::CreditDefaultSwap(side, notional, spread, schedule, paymentConvention, dayCounter, settlesAccrual,
                            protectionPaymentTime, protectionStart, claim, lastPeriodDayCounter, rebatesAccrual,
                            tradeDate, cashSettlementDays),
          underlyingNotionals_(underlyingNotionals) {}

    IndexCreditDefaultSwap(Protection::Side side, Real notional, std::vector<Real> underlyingNotionals, Rate upfront,
                           Rate spread, const Schedule& schedule, BusinessDayConvention paymentConvention,
                           const DayCounter& dayCounter, bool settlesAccrual = true,
                           ProtectionPaymentTime protectionPaymentTime = atDefault,
                           const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                           const QuantLib::ext::shared_ptr<Claim>& claim = QuantLib::ext::shared_ptr<Claim>(),
                           const DayCounter& lastPeriodDayCounter = DayCounter(),
                           bool rebatesAccrual = true,
                           const Date& tradeDate = Date(),
                           Natural cashSettlementDays = 3)
        : QuantLib::CreditDefaultSwap(side, notional, upfront, spread, schedule, paymentConvention, dayCounter, settlesAccrual,
                            protectionPaymentTime, protectionStart, upfrontDate, claim, lastPeriodDayCounter,
                            rebatesAccrual, tradeDate, cashSettlementDays),
          underlyingNotionals_(underlyingNotionals) {}

    //@}
    //! \name Inspectors
    //@{
    const std::vector<Real>& underlyingNotionals() const { return underlyingNotionals_; }
    //@}

    //@}
    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

protected:
    std::vector<Real> underlyingNotionals_;
    //! \name Additional interface
    //@{
    virtual QuantLib::ext::shared_ptr<PricingEngine> buildPricingEngine(const Handle<DefaultProbabilityTermStructure>& p,
                                                                Real r, const Handle<YieldTermStructure>& d,
								PricingModel model = Midpoint) const override;
    //@}
};

class IndexCreditDefaultSwap::arguments : public QuantLib::CreditDefaultSwap::arguments {
public:
    std::vector<Real> underlyingNotionals;
};

class IndexCreditDefaultSwap::results : public QuantLib::CreditDefaultSwap::results {};

class IndexCreditDefaultSwap::engine
    : public GenericEngine<IndexCreditDefaultSwap::arguments, IndexCreditDefaultSwap::results> {};

} // namespace QuantExt

#endif
