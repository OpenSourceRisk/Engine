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

CompoEquityIndex::CompoEquityIndex(const boost::shared_ptr<EquityIndex>& source,
                                   const boost::shared_ptr<FxIndex>& fxIndex)
    : EquityIndex(source->familyName() + "_compo_" + fxIndex->targetCurrency().code(),
                  JointCalendar(source->fixingCalendar(), fxIndex->fixingCalendar()), fxIndex->targetCurrency(),
                  Handle<Quote>(boost::make_shared<CompositeQuote<std::function<Real(Real, Real)>>>(
                      source->equitySpot(), fxIndex->fxQuote(),
                      fxIndex->inverseIndex() ? std::function<Real(Real, Real)>([](Real x, Real y) { return x / y; })
                                              : std::function<Real(Real, Real)>([](Real x, Real y) { return x * y; }))),
                  fxIndex->inverseIndex()
                      ? Handle<YieldTermStructure>(boost::make_shared<DiscountRatioModifiedCurve>(
                            source->equityForecastCurve(), fxIndex->sourceCurve(), fxIndex->targetCurve()))
                      : Handle<YieldTermStructure>(boost::make_shared<DiscountRatioModifiedCurve>(
                            source->equityForecastCurve(), fxIndex->targetCurve(), fxIndex->sourceCurve())),
                  source->equityDividendCurve()),
      source_(source), fxIndex_(fxIndex) {
    LazyObject::registerWith(source_);
    LazyObject::registerWith(fxIndex_);
}

boost::shared_ptr<EquityIndex> CompoEquityIndex::source() const { return source_; }

void CompoEquityIndex::addDividend(const Date& fixingDate, Real fixing, bool forceOverwrite) {
    source_->addDividend(fixingDate, fixing / fxIndex_->fixing(fixingDate), forceOverwrite);
    LazyObject::update();
}

void CompoEquityIndex::performCalculations() const {
    dividendFixings_ = TimeSeries<Real>();
    auto const& ts = source_->dividendFixings();
    for (auto const& d : ts) {
        dividendFixings_[d.first] = d.second * fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(d.first, Preceding));
    }
}

const TimeSeries<Real>& CompoEquityIndex::dividendFixings() const {
    calculate();
    return dividendFixings_;
}

Real CompoEquityIndex::pastFixing(const Date& fixingDate) const {
    return source_->fixing(fixingDate) * fxIndex_->fixing(fixingDate);
}

boost::shared_ptr<EquityIndex> CompoEquityIndex::clone(const Handle<Quote> spotQuote,
                                                       const Handle<YieldTermStructure>& rate,
                                                       const Handle<YieldTermStructure>& dividend) const {
    return boost::make_shared<CompoEquityIndex>(source_->clone(spotQuote, rate, dividend), fxIndex_);
}

} // namespace QuantExt
