/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/lgmcg.hpp>

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/instruments/vanillaswap.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>

namespace ore::data {

std::size_t LgmCG::numeraire(const Date& d, const std::size_t x) const {

    modelParameters_.clear();
    QL_FAIL("TODO");
}

std::size_t LgmCG::discountBond(const Date& d, const Date& e, const std::size_t x) const { QL_FAIL("TODO"); }

std::size_t LgmCG::reducedDiscountBond(const Date& d, const Date& e, const std::size_t x) const { QL_FAIL("TODO"); }

/* Handles IborIndex and SwapIndex. Requires observation time t <= fixingDate */
std::size_t LgmCG::fixing(const boost::shared_ptr<InterestRateIndex>& index, const Date& fixingDate, const Date& t,
                          const std::size_t x) const {
    QL_FAIL("TODO");
}

} // namespace ore::data
