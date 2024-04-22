/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/indexes/compoequityindex.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>

#include <ql/quotes/compositequote.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

namespace QuantExt {

CompoEquityIndex::CompoEquityIndex(const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& source,
                                   const QuantLib::ext::shared_ptr<FxIndex>& fxIndex, const Date& dividendCutoffDate)
    : QuantExt::EquityIndex2(source->familyName() + "_compo_" + fxIndex->targetCurrency().code(),
                  JointCalendar(source->fixingCalendar(), fxIndex->fixingCalendar()), fxIndex->targetCurrency(),
                  Handle<Quote>(QuantLib::ext::make_shared<CompositeQuote<std::function<Real(Real, Real)>>>(
                      source->equitySpot(), fxIndex->fxQuote(),
                      std::function<Real(Real, Real)>([](Real x, Real y) { return x * y; }))),
                  Handle<YieldTermStructure>(QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(
                      source->equityForecastCurve(), fxIndex->targetCurve(), fxIndex->sourceCurve())),
                  source->equityDividendCurve()),
      source_(source), fxIndex_(fxIndex), dividendCutoffDate_(dividendCutoffDate) {
    LazyObject::registerWith(source_);
    LazyObject::registerWith(fxIndex_);
}

QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> CompoEquityIndex::source() const { return source_; }

void CompoEquityIndex::addDividend(const Dividend& dividend, bool forceOverwrite) {
    if (dividendCutoffDate_ == Date() || dividend.exDate >= dividendCutoffDate_) {
        Dividend newDiv(dividend.exDate, dividend.name, dividend.rate / fxIndex_->fixing(dividend.exDate),
                        dividend.payDate);
        source_->addDividend(newDiv, forceOverwrite);
        LazyObject::update();
    }
}

void CompoEquityIndex::performCalculations() const {
    dividendFixings_ = std::set<Dividend>();
    auto const& ts = source_->dividendFixings();
    for (auto const& d : ts) {
        if (dividendCutoffDate_ == Date() || d.exDate >= dividendCutoffDate_) {
            Dividend div(d.exDate, d.name, d.rate * fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(d.exDate, Preceding)), d.payDate);
            dividendFixings_.insert(div);
        }
    }
}

const std::set<Dividend>& CompoEquityIndex::dividendFixings() const {
    calculate();
    return dividendFixings_;
}

Real CompoEquityIndex::pastFixing(const Date& fixingDate) const {
    return source_->fixing(fixingDate) * fxIndex_->fixing(fixingDate);
}

QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> CompoEquityIndex::clone(const Handle<Quote> spotQuote,
                                                       const Handle<YieldTermStructure>& rate,
                                                       const Handle<YieldTermStructure>& dividend) const {
    return QuantLib::ext::make_shared<CompoEquityIndex>(source_->clone(spotQuote, rate, dividend), fxIndex_);
}

} // namespace QuantExt
