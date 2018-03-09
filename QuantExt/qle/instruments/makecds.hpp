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

/*! \file makecds.hpp
    \brief Helper class to instantiate standard market cds.
    \ingroup instruments
*/

#ifndef quantext_makecds_hpp
#define quantext_makecds_hpp

#include <boost/optional.hpp>
#include <qle/instruments/creditdefaultswap.hpp>

using namespace QuantLib;

namespace QuantExt {

//! helper class
/*! This class provides a more comfortable way
    to instantiate standard cds.
    \bug support last period dc
    \ingroup instruments
*/
class MakeCreditDefaultSwap {
public:
    MakeCreditDefaultSwap(const Period& tenor, const Real couponRate);
    MakeCreditDefaultSwap(const Date& termDate, const Real couponRate);

    operator CreditDefaultSwap() const;
    operator boost::shared_ptr<CreditDefaultSwap>() const;

    MakeCreditDefaultSwap& withUpfrontRate(Real);
    MakeCreditDefaultSwap& withSide(Protection::Side);
    MakeCreditDefaultSwap& withNominal(Real);
    MakeCreditDefaultSwap& withCouponTenor(Period);
    MakeCreditDefaultSwap& withDayCounter(DayCounter&);
    // MakeCreditDefaultSwap& withLastPeriodDayCounter(DayCounter&);

    MakeCreditDefaultSwap& withPricingEngine(const boost::shared_ptr<PricingEngine>&);

private:
    Protection::Side side_;
    Real nominal_;
    boost::optional<Period> tenor_;
    boost::optional<Date> termDate_;
    Period couponTenor_;
    Real couponRate_;
    Real upfrontRate_;
    DayCounter dayCounter_;
    // DayCounter lastPeriodDayCounter_;

    boost::shared_ptr<PricingEngine> engine_;
};
} // namespace QuantExt

#endif
