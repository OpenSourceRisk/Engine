/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/methods/irdeltaparconverter.hpp>
#include <qle/termstructures/spreadeddiscountcurve.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>

namespace QuantExt {

IrDeltaParConverter::IrDeltaParConverter(const std::vector<QuantLib::Period>& terms,
                                         const std::vector<InstrumentType>& instrumentTypes,
                                         const QuantExt::ext::shared_ptr<QuantLib::SwapIndex>& indexBase,
                                         const std::function<Real(Date)>& dateToTime)
    : terms_(terms), instrumentTypes_(instrumentTypes), indexBase_(indexBase), dateToTime_(dateToTime) {

    dpardzero_ = Matrix(terms_.size(), terms_.size(), 0.0);

    QL_REQUIRE(terms_.size() == instrumentTypes_.size(),
               "IrDeltaParConverter: number of terms (" << terms_.size() << ") does not match instrument type terms ("
                                                        << terms_.size() << ")");

    // build helpers

    std::vector<QuantLib::ext::shared_ptr<RelativeDateRateHelper>> helpers;

    for (std::size_t i = 0; i < terms_.size(); ++i) {

        switch (instrumentTypes_[i]) {
        case InstrumentType::Deposit: {
            helpers.push_back(QuantLib::ext::make_shared<DepositRateHelper>(
                0.0, terms[i], indexBase->iborIndex()->fixingDays(), indexBase->iborIndex()->fixingCalendar(),
                indexBase->iborIndex()->businessDayConvention(), indexBase->iborIndex()->endOfMonth(),
                indexBase->iborIndex()->dayCounter()));
            break;
        }
        case InstrumentType::Swap: {
            if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedSwapIndex>(indexBase)) {
                helpers.push_back(QuantLib::ext::make_shared<OISRateHelper>(
                    on->fixingDays(), terms[i], Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)),
                    on->overnightIndex(), on->discountingTermStructure(), true, 0, Following, Annual, Calendar(),
                    0 * Days, 0.0, Pillar::LastRelevantDate, Date(), on->averagingMethod()));
            } else {
                helpers.push_back(QuantLib::ext::make_shared<SwapRateHelper>(
                    0.0, indexBase->clone(terms[i]), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)),
                    0 * Days, indexBase->discountingTermStructure()));
            }
            break;
        }
        default:
            QL_FAIL("IrDeltaParConverter: instrument type at index " << i
                                                                     << " not handled. This is an internal error.");
        }
    }

    // determine times

    times_.resize(terms_.size());

    std::transform(helpers.begin(), helpers.end(), times_.begin(),
                   [this](const QuantLib::ext::shared_ptr<QuantLib::RelativeDateRateHelper>& h) {
                       return dateToTime_(h->pillarDate());
                   });

    // set up spread curve

    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> spreads;
    std::vector<Handle<Quote>> spreadsH;
    for (Size i = 0; i < times_.size(); ++i) {
        spreads.push_back(QuantLib::ext::make_shared<SimpleQuote>(1.0));
        spreadsH.push_back(Handle<Quote>(spreads.back()));
        if (i == 0)
            spreadsH.push_back(spreadsH.back());
    }

    std::vector<Real> timesInc0 = times_;
    timesInc0.insert(timesInc0.begin(), 0.0);
    auto ts = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<SpreadedDiscountCurve>(indexBase->iborIndex()->forwardingTermStructure(), timesInc0,
                                                          spreadsH, SpreadedDiscountCurve::Interpolation::linearZero));

    // assign spread curve to helpers

    for (auto& h : helpers)
        h->setTermStructure(ts.currentLink().get());

    // remove todays fixings

    Date today = indexBase->iborIndex()->forwardingTermStructure()->referenceDate();
    QL_DEPRECATED_DISABLE_WARNING
    struct FixingRemover {
        FixingRemover(const Date& today, const QuantLib::ext::shared_ptr<Index>& index) : today(today), index(index) {
            TimeSeries<Real> s = IndexManager::instance().getHistory(index->name());
            savedFixing = s[today];
            s[today] = Null<Real>();
            IndexManager::instance().setHistory(index->name(), s);
        }
        ~FixingRemover() {
            TimeSeries<Real> s = IndexManager::instance().getHistory(index->name());
            s[today] = savedFixing;
            IndexManager::instance().setHistory(index->name(), s);
        }
        Date today;
        QuantLib::ext::shared_ptr<Index> index;
        Real savedFixing;
    } FixingRemover(today, indexBase->iborIndex());
    QL_DEPRECATED_ENABLE_WARNING
    // compute jacobian and its inverse

    constexpr Real shift = 1E-4;

    for (std::size_t i = 0; i < terms_.size(); ++i) {
        Real baseRate = helpers[i]->impliedQuote();
        for (std::size_t j = 0; j <= i; ++j) {
            spreads[j]->setValue(std::exp(-shift * times_[j]));
            Real bumpedRate = helpers[i]->impliedQuote();
            spreads[j]->setValue(1.0);
            dpardzero_(i, j) = (bumpedRate - baseRate) / shift;
        }
    }

    dzerodpar_ = inverse(dpardzero_);
}

const std::vector<Real>& IrDeltaParConverter::times() const { return times_; }
const Matrix& IrDeltaParConverter::dpardzero() const { return dpardzero_; }
const Matrix& IrDeltaParConverter::dzerodpar() const { return dzerodpar_; }

} // namespace QuantExt
