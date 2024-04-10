/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/indexes/indexmanager.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>

namespace QuantExt {

FXLinked::FXLinked(const Date& fxFixingDate, Real foreignAmount, QuantLib::ext::shared_ptr<FxIndex> fxIndex)
    : fxFixingDate_(fxFixingDate), foreignAmount_(foreignAmount), fxIndex_(fxIndex) {}

AverageFXLinked::AverageFXLinked(const std::vector<Date>& fxFixingDates, Real foreignAmount,
                                 QuantLib::ext::shared_ptr<FxIndex> fxIndex, const bool inverted)
    : fxFixingDates_(fxFixingDates), foreignAmount_(foreignAmount), fxIndex_(fxIndex), inverted_(inverted) {}

Real FXLinked::fxRate() const {
    return fxIndex_->fixing(fxFixingDate_);
}

Real AverageFXLinked::fxRate() const {
    Real fx = 0;
    for (auto const& d: fxFixingDates_)
        fx += inverted_ ? 1.0 / fxIndex_->fixing(d) : fxIndex_->fixing(d);
    fx /= fxFixingDates_.size();
    return inverted_ ? 1.0 / fx : fx;
}

FXLinkedCashFlow::FXLinkedCashFlow(const Date& cashFlowDate, const Date& fxFixingDate, Real foreignAmount,
                                   QuantLib::ext::shared_ptr<FxIndex> fxIndex)
    : FXLinked(fxFixingDate, foreignAmount, fxIndex), cashFlowDate_(cashFlowDate) {
    registerWith(FXLinked::fxIndex());
}

AverageFXLinkedCashFlow::AverageFXLinkedCashFlow(const Date& cashFlowDate, const std::vector<Date>& fxFixingDates,
                                                 Real foreignAmount, QuantLib::ext::shared_ptr<FxIndex> fxIndex,
                                                 const bool inverted)
    : AverageFXLinked(fxFixingDates, foreignAmount, fxIndex, inverted), cashFlowDate_(cashFlowDate) {
    registerWith(AverageFXLinked::fxIndex());
}

QuantLib::ext::shared_ptr<FXLinked> FXLinkedCashFlow::clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) {
    return QuantLib::ext::make_shared<FXLinkedCashFlow>(date(), fxFixingDate(), foreignAmount(), fxIndex);
}

QuantLib::ext::shared_ptr<AverageFXLinked> AverageFXLinkedCashFlow::clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) {
    return QuantLib::ext::make_shared<AverageFXLinkedCashFlow>(date(), fxFixingDates(), foreignAmount(), fxIndex);
}

std::map<Date, Real> AverageFXLinkedCashFlow::fixings() const {
    std::map<Date, Real> result;
    for (auto const& d : fxFixingDates_)
        result[d] = inverted_ ? 1.0 / fxIndex_->fixing(d) : fxIndex_->fixing(d);
    return result;
}

} // namespace QuantExt
