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

#include <qle/cashflows/bondtrscashflow.hpp>
#include <qle/instruments/bondtotalreturnswap.hpp>

#include <ql/event.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

BondTRS::BondTRS(const QuantLib::ext::shared_ptr<QuantExt::BondIndex>& bondIndex, const Real bondNotional,
                 const Real initialPrice, const std::vector<QuantLib::Leg>& fundingLeg, const bool payTotalReturnLeg,
                 const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
                 const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex, bool payBondCashFlowsImmediately,
                 const Currency& fundingCurrency, const Currency& bondCurrency)
    : bondIndex_(bondIndex), bondNotional_(bondNotional), initialPrice_(initialPrice), fundingLeg_(fundingLeg),
      payTotalReturnLeg_(payTotalReturnLeg), fxIndex_(fxIndex),
      payBondCashFlowsImmediately_(payBondCashFlowsImmediately), fundingCurrency_(fundingCurrency),
      bondCurrency_(bondCurrency), valuationDates_(valuationDates), paymentDates_(paymentDates) {

    QL_REQUIRE(bondIndex, "BondTRS: no bond index given");
    registerWith(bondIndex);

    if (fxIndex_ != nullptr)
        registerWith(fxIndex_);

    if (!fundingCurrency_.empty() && !bondCurrency_.empty()) {
        // do we require an fx index for conversion ...
        QL_REQUIRE(fundingCurrency_ == bondCurrency_ || fxIndex_ != nullptr,
                   "BondTRS: fx index required if funding ccy ("
                       << fundingCurrency_.code() << ") not equal to bond ccy (" << bondCurrency_.code() << ")");
        // ... and if yes, has it the right currencies?
        if (fxIndex_ != nullptr) {
            QL_REQUIRE(fxIndex_->sourceCurrency() == bondCurrency_ && fxIndex_->targetCurrency() == fundingCurrency_,
                       "BondTRS: fx index '" << fxIndex_->name() << "' currencies must match bond ccy / funding ccy ("
                                             << bondCurrency_.code() << " / " << fundingCurrency_.code() << ")");
        }
    }

    for (auto const& l : fundingLeg) {
        for (auto const& c : l)
            registerWith(c);
    }
    QL_REQUIRE(valuationDates.size() > 1, "valuation dates size > 1 required");

    returnLeg_ =
        BondTRSLeg(valuationDates_, paymentDates_, bondNotional_, bondIndex_, fxIndex_).withInitialPrice(initialPrice_);
}

bool BondTRS::isExpired() const { return detail::simple_event(valuationDates_.back()).hasOccurred(); }

void BondTRS::setupArguments(PricingEngine::arguments* args) const {
    BondTRS::arguments* arguments = dynamic_cast<BondTRS::arguments*>(args);
    QL_REQUIRE(arguments, "BondTRS instrument: wrong argument type in bond total return swap");
    arguments->bondIndex = bondIndex_;
    arguments->fxIndex = fxIndex_;
    arguments->bondNotional = bondNotional_;
    arguments->fundingLeg = fundingLeg_;
    arguments->returnLeg = returnLeg_;
    arguments->payTotalReturnLeg = payTotalReturnLeg_;
    arguments->payBondCashFlowsImmediately = payBondCashFlowsImmediately_;
    arguments->fundingCurrency = fundingCurrency_;
    arguments->bondCurrency = bondCurrency_;
    arguments->valuationDates = valuationDates_;
    arguments->paymentDates = paymentDates_;
}

} // namespace QuantExt
