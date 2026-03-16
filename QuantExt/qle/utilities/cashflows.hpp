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

/*! \file qle/utilities/cashflows.hpp
    \brief some cashflow related utilities.
*/

#pragma once

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/utilities/time.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>

namespace QuantExt {

/*! Utility function for calculating the atm strike level to a given \p fixingDate based on a given ois index,
    \p on, and a given irate computation period, \p rateComputationPeriod.

\ingroup utilities
*/

Real getOisAtmLevel(const QuantLib::ext::shared_ptr<OvernightIndex>& on, const Date& fixingDate,
                    const Period& rateComputationPeriod);

/*! Utility function for calculating the atm strike level to a given \p fixingDate based on a given BMA index,
    \p on, and a given irate computation period, \p rateComputationPeriod.

\ingroup utilities
*/
Real getBMAAtmLevel(const QuantLib::ext::shared_ptr<BMAIndex>& bma, const Date& fixingDate,
                    const Period& rateComputationPeriod);

} // namespace QuantExt
