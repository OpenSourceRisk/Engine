/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

/*! \file cbo.cpp
    \brief collateralized bond obligation instrument
*/

#include <qle/instruments/cbo.hpp>

namespace QuantExt {

CBO::CBO(const QuantLib::ext::shared_ptr<BondBasket>& basket, const Schedule& schedule,
         Rate seniorFee, const DayCounter& feeDayCounter, const std::vector<Tranche>& tranches,
         Rate subordinatedFee, Rate equityKicker, const Currency& ccy,
         const std::string& investedTrancheName)
    : basket_(basket), schedule_(schedule), seniorFee_(seniorFee), feeDayCounter_(feeDayCounter),
      tranches_(tranches), subordinatedFee_(subordinatedFee), equityKicker_(equityKicker), ccy_(ccy),
      investedTrancheName_(investedTrancheName) {
    QL_REQUIRE(basket->bonds().size() > 0, "basket is empty");
    QL_REQUIRE(tranches.size() > 0, "no tranches specified");
}

//------------------------------
// Notes
//------------------------------

//------------------------------
// Functions
//------------------------------

Real CBO::basketValue() const {
    calculate();
    return basketValue_;
}

std::vector<Real> CBO::trancheValue() const {
    calculate();
    return trancheValue_;
}

Real CBO::feeValue() const {
    calculate();
    return feeValue_;
}

Real CBO::basketValueStd() const {
    calculate();
    return basketValueStd_;
}

std::vector<Real> CBO::trancheValueStd() const {
    calculate();
    return trancheValueStd_;
}

Real CBO::feeValueStd() const {
    calculate();
    return feeValueStd_;
}

const std::vector<CashflowTable>& CBO::trancheCashflows() const {
    calculate();
    return trancheCashflows_;
}

bool CBO::isExpired() const {
    if (schedule_.dates().back() <= Settings::instance().evaluationDate())
        return true;
    else
        return false;
}

void CBO::setupArguments(PricingEngine::arguments* args) const {
    CBO::arguments* arguments = dynamic_cast<CBO::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");
    arguments->basket = basket_;
    arguments->schedule = schedule_;
    arguments->seniorFee = seniorFee_;
    arguments->feeDayCounter = feeDayCounter_;
    arguments->tranches = tranches_;
    arguments->equityKicker = equityKicker_;
    arguments->subordinatedFee = subordinatedFee_;
    arguments->ccy = ccy_;
    arguments->investedTrancheName = investedTrancheName_;

}

void CBO::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);

    const CBO::results* results = dynamic_cast<const CBO::results*>(r);
    QL_REQUIRE(results != 0, "wrong result type");

    basketValue_ = results->basketValue;
    trancheValue_ = results->trancheValue;
    feeValue_ = results->feeValue;
    basketValueStd_ = results->basketValueStd;
    trancheValueStd_ = results->trancheValueStd;
    feeValueStd_ = results->feeValueStd;
    trancheCashflows_ = results->trancheCashflows;
}

void CBO::setupExpired() const {
    Instrument::setupExpired();
    basketValue_ = 0.0;
    trancheValue_.clear();
    feeValue_ = 0.0;

    basketValueStd_ = 0.0;
    trancheValueStd_.clear();
    feeValueStd_ = 0.0;
    trancheCashflows_.clear();
}

void CBO::arguments::validate() const {
    QL_REQUIRE(basket && !basket->bonds().empty(), "no basket given");
    QL_REQUIRE(seniorFee != Null<Real>(), "no senior fee given");
    QL_REQUIRE(!feeDayCounter.empty(), "no fee day counter given");
}

void CBO::results::reset() {
    Instrument::results::reset();
    basketValue = Null<Real>();
    trancheValue.clear();
    basketValueStd = Null<Real>();
    trancheValueStd.clear();
    trancheCashflows.clear();
    feeValue = Null<Real>();
    subfeeValue = Null<Real>();
}

} // namespace QuantExt
