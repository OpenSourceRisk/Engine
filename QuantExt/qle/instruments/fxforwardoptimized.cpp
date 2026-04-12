/*
 Copyright (C) 2026 AcadiaSoft, Inc.

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

#include <qle/instruments/fxforwardoptimized.hpp>

#include <ql/event.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;

namespace QuantExt {

FxForwardOptimized::FxForwardOptimized(const Real& nominal1, const Currency& currency1, const Real& nominal2, const Currency& currency2,
                     const Date& maturityDate, const bool& payCurrency1, const bool isPhysicallySettled,
                     const Date& payDate, const Currency& payCcy, const Date& fixingDate,
                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                     const Handle<YieldTermStructure>& currency1DiscountCurve,
                     const Handle<YieldTermStructure>& currency2DiscountCurve, const Handle<Quote>& spotFX)
    : nominal1_(nominal1), currency1_(currency1), nominal2_(nominal2), currency2_(currency2),
      maturityDate_(maturityDate), payCurrency1_(payCurrency1), isPhysicallySettled_(isPhysicallySettled),
      payDate_(payDate), payCcy_(payCcy), fxIndex_(fxIndex), fixingDate_(fixingDate),
      currency1DiscountCurve_(currency1DiscountCurve), currency2DiscountCurve_(currency2DiscountCurve),
      spotFX_(spotFX) {

    if (payDate_ == Date())
        payDate_ = maturityDate_;

    if (fixingDate_ == Date())
        fixingDate_ = maturityDate_;

    if (!isPhysicallySettled && payDate_ > fixingDate_) {
        QL_REQUIRE(fxIndex_, "FxForwardOptimized: no FX index given for non-deliverable forward.");
        QL_REQUIRE(fixingDate_ != Date(), "FxForwardOptimized: no FX fixing date given for non-deliverable forward.");
        registerWith(fxIndex_);
    }

    registerWith(currency1DiscountCurve_);
    registerWith(currency2DiscountCurve_);
    registerWith(spotFX_);
}

bool FxForwardOptimized::isExpired() const {
    ext::optional<bool> includeToday = Settings::instance().includeTodaysCashFlows();
    Date refDate = Settings::instance().evaluationDate();
    return detail::simple_event(payDate_).hasOccurred(refDate, includeToday);
}

void FxForwardOptimized::performCalculations() const {
    NPV_ = -nominal1_ * currency1DiscountCurve_->discount(payDate_) +
           nominal2_ * currency2DiscountCurve_->discount(payDate_) * spotFX_->value();
}

} // namespace QuantExt
