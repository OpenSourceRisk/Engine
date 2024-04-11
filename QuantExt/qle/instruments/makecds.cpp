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
  Copyright (C) 2014 Jose Aparicio
  Copyright (C) 2014 Peter Caspers

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

#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/instruments/makecds.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

MakeCreditDefaultSwap::MakeCreditDefaultSwap(const Period& tenor, const Real couponRate)
    : side_(Protection::Buyer), nominal_(1.0), tenor_(tenor), termDate_(boost::none), couponTenor_(3 * Months),
      couponRate_(couponRate), upfrontRate_(0.0),
      dayCounter_(Actual360()), lastPeriodDayCounter_(Actual360(true)),
      rule_(DateGeneration::CDS2015), cashSettlementDays_(3), settlesAccrual_(true),
      paysAtDefaultTime_(true), rebatesAccrual_(true) {}

MakeCreditDefaultSwap::MakeCreditDefaultSwap(const Date& termDate, const Real couponRate)
    : side_(Protection::Buyer), nominal_(1.0), tenor_(boost::none), termDate_(termDate), couponTenor_(3 * Months),
      couponRate_(couponRate), upfrontRate_(0.0),
      dayCounter_(Actual360()), lastPeriodDayCounter_(Actual360(true)),
      rule_(DateGeneration::CDS2015), cashSettlementDays_(3), settlesAccrual_(true),
      paysAtDefaultTime_(true), rebatesAccrual_(true) {}

MakeCreditDefaultSwap::operator CreditDefaultSwap() const {
    QuantLib::ext::shared_ptr<CreditDefaultSwap> swap = *this;
    return *swap;
}

    MakeCreditDefaultSwap::operator QuantLib::ext::shared_ptr<QuantExt::CreditDefaultSwap>() const {

    Date tradeDate = Settings::instance().evaluationDate();
    Date upfrontDate = WeekendsOnly().advance(tradeDate, cashSettlementDays_, Days);

    Date protectionStart;
    if (rule_ == DateGeneration::CDS2015 || rule_ == DateGeneration::CDS) {
        protectionStart = tradeDate;
    } else {
        protectionStart = tradeDate + 1;
    }

    Date end;
    if (tenor_) {
        if (rule_ == DateGeneration::CDS2015 || rule_ == DateGeneration::CDS || rule_ == DateGeneration::OldCDS) {
            end = cdsMaturity(tradeDate, *tenor_, rule_);
        } else {
            end = tradeDate + *tenor_;
        }
    } else {
        end = *termDate_;
    }

    Schedule schedule(protectionStart, end, couponTenor_, WeekendsOnly(), Following, Unadjusted, rule_, false);

    CreditDefaultSwap::ProtectionPaymentTime timing = paysAtDefaultTime_ ?
        CreditDefaultSwap::ProtectionPaymentTime::atDefault :
        CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
    QuantLib::ext::shared_ptr<CreditDefaultSwap> cds = QuantLib::ext::make_shared<CreditDefaultSwap>(
        side_, nominal_, upfrontRate_, couponRate_, schedule, Following, dayCounter_, settlesAccrual_,
        timing, protectionStart, upfrontDate, QuantLib::ext::shared_ptr<Claim>(),
        lastPeriodDayCounter_, rebatesAccrual_, tradeDate, cashSettlementDays_); 

    cds->setPricingEngine(engine_);
    return cds;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withUpfrontRate(Real upfrontRate) {
    upfrontRate_ = upfrontRate;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withSide(Protection::Side side) {
    side_ = side;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withNominal(Real nominal) {
    nominal_ = nominal;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withCouponTenor(Period couponTenor) {
    couponTenor_ = couponTenor;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withDayCounter(const DayCounter& dayCounter) {
    dayCounter_ = dayCounter;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withLastPeriodDayCounter(const DayCounter& lastPeriodDayCounter) {
    lastPeriodDayCounter_ = lastPeriodDayCounter;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withDateGenerationRule(DateGeneration::Rule rule) {
    rule_ = rule;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withCashSettlementDays(Natural cashSettlementDays) {
    cashSettlementDays_ = cashSettlementDays;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withSettlesAccrual(bool settlesAccrual) {
    settlesAccrual_ = settlesAccrual;
    return *this;
}
    
MakeCreditDefaultSwap& MakeCreditDefaultSwap::withPaysAtDefaultTime(bool paysAtDefaultTime) {
    paysAtDefaultTime_ = paysAtDefaultTime;
    return *this;
}

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withRebatesAccrual(bool rebatesAccrual) {
    rebatesAccrual_ = rebatesAccrual;
    return *this;
}
} // namespace QuantExt
