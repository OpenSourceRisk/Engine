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

#include <iostream>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/instruments/makecds.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

MakeCreditDefaultSwap::MakeCreditDefaultSwap(const Period& tenor, const Real couponRate)
    : side_(Protection::Buyer), nominal_(1.0), tenor_(tenor), termDate_(boost::none), couponTenor_(3 * Months),
      couponRate_(couponRate), upfrontRate_(0.0),
      dayCounter_(Actual360()) /*, lastPeriodDayCounter_(Actual360(tshellrue))*/ {}
MakeCreditDefaultSwap::MakeCreditDefaultSwap(const Date& termDate, const Real couponRate)
    : side_(Protection::Buyer), nominal_(1.0), tenor_(boost::none), termDate_(termDate), couponTenor_(3 * Months),
      couponRate_(couponRate), upfrontRate_(0.0),
      dayCounter_(Actual360()) /*, lastPeriodDayCounter_(Actual360(true))*/ {}
MakeCreditDefaultSwap::operator CreditDefaultSwap() const {
    boost::shared_ptr<CreditDefaultSwap> swap = *this;
    return *swap;
}

MakeCreditDefaultSwap::operator boost::shared_ptr<CreditDefaultSwap>() const {

    Date evaluation = Settings::instance().evaluationDate();
    Date start = evaluation + 1;
    Date upfrontDate = WeekendsOnly().advance(evaluation, 3 * Days);
    Date end;
    if (tenor_) {
        end = start + *tenor_;
    } else {
        end = *termDate_;
    }

    Schedule schedule(start, end, couponTenor_, WeekendsOnly(), Following, Unadjusted,
                      evaluation >= Date(21, Dec, 2015) ? DateGeneration::CDS2015 : DateGeneration::CDS, false, Date(),
                      Date());

    boost::shared_ptr<CreditDefaultSwap> cds = boost::shared_ptr<CreditDefaultSwap>(
        new CreditDefaultSwap(side_, nominal_, upfrontRate_, couponRate_, schedule, Following, dayCounter_, true, true,
                              start, upfrontDate, boost::shared_ptr<Claim>() /*, lastPeriodDayCounter_*/));

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

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withDayCounter(DayCounter& dayCounter) {
    dayCounter_ = dayCounter;
    return *this;
}

// MakeCreditDefaultSwap& MakeCreditDefaultSwap::withLastPeriodDayCounter(DayCounter& lastPeriodDayCounter) {
//     lastPeriodDayCounter_ = lastPeriodDayCounter;
//     return *this;
// }

MakeCreditDefaultSwap& MakeCreditDefaultSwap::withPricingEngine(const boost::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
    return *this;
}
} // namespace QuantExt
