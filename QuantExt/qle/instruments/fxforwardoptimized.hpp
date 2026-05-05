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

/*! \file qle/instruments/fxforwardoptimized.hpp
    \brief fxforward instrument, speed optimized for use in exposure simulation
*/

#pragma once

#include <ql/currency.hpp>
#include <ql/exchangerate.hpp>
#include <ql/instrument.hpp>
#include <ql/money.hpp>
#include <ql/quote.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

class FxForwardOptimized : public Instrument {
public:
    // spot must be given as ccy2-ccy1 (for-dom)
    FxForwardOptimized(const Real& nominal1, const Currency& currency1, const Real& nominal2, const Currency& currency2,
                       const Date& maturityDate, const bool& payCurrency1, const bool isPhysicallySettled = true,
                       const Date& payDate = Date(), const Currency& payCcy = Currency(),
                       const Date& fixingDate = Date(),
                       const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
                       const Handle<YieldTermStructure>& currency1DiscountCurve = {},
                       const Handle<YieldTermStructure>& currency2DiscountCurve = {}, const Handle<Quote>& spotFX = {});
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override {}
    void fetchResults(const PricingEngine::results*) const override {}
    void performCalculations() const override;

private:
    Real nominal1_;
    Currency currency1_;
    Real nominal2_;
    Currency currency2_;
    Date maturityDate_;
    [[maybe_unused]] bool payCurrency1_;
    [[maybe_unused]] bool isPhysicallySettled_;
    Date payDate_;
    Currency payCcy_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Date fixingDate_;
    Handle<YieldTermStructure> currency1DiscountCurve_;
    Handle<YieldTermStructure> currency2DiscountCurve_;
    Handle<Quote> spotFX_;
};

} // namespace QuantExt
