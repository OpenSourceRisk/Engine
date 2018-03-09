/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/time/imm.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/termstructures/immfraratehelper.hpp>

using namespace std;
namespace QuantExt {

ImmFraRateHelper::ImmFraRateHelper(const Handle<Quote>& rate, const Size imm1, const Size imm2,
                                   const boost::shared_ptr<IborIndex>& i, Pillar::Choice pillarChoice,
                                   Date customPillarDate)
    : RelativeDateRateHelper(rate), imm1_(imm1), imm2_(imm2), pillarChoice_(pillarChoice) {
    // take fixing into account
    iborIndex_ = i->clone(termStructureHandle_);
    // see above
    iborIndex_->unregisterWith(termStructureHandle_);
    registerWith(iborIndex_);
    pillarDate_ = customPillarDate;
    initializeDates();
}

Real ImmFraRateHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    return iborIndex_->fixing(fixingDate_, true);
}

void ImmFraRateHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed---the index is not lazy
    bool observer = false;

    boost::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

void ImmFraRateHelper::initializeDates() {
    // if the evaluation date is not a business day
    // then move to the next business day
    Date referenceDate = iborIndex_->fixingCalendar().adjust(evaluationDate_);
    Date spotDate = iborIndex_->fixingCalendar().advance(referenceDate, iborIndex_->fixingDays() * Days);

    earliestDate_ = iborIndex_->fixingCalendar().adjust(getImmDate(spotDate, imm1_));
    maturityDate_ = iborIndex_->fixingCalendar().adjust(getImmDate(spotDate, imm2_));

    // latest relevant date is calculated from earliestDate_ instead
    latestRelevantDate_ = iborIndex_->maturityDate(earliestDate_);

    switch (pillarChoice_) {
    case Pillar::MaturityDate:
        pillarDate_ = maturityDate_;
        break;
    case Pillar::LastRelevantDate:
        pillarDate_ = latestRelevantDate_;
        break;
    case Pillar::CustomDate:
        // pillarDate_ already assigned at construction time
        QL_REQUIRE(pillarDate_ >= earliestDate_, "pillar date (" << pillarDate_
                                                                 << ") must be later "
                                                                    "than or equal to the instrument's earliest date ("
                                                                 << earliestDate_ << ")");
        QL_REQUIRE(pillarDate_ <= latestRelevantDate_, "pillar date ("
                                                           << pillarDate_
                                                           << ") must be before "
                                                              "or equal to the instrument's latest relevant date ("
                                                           << latestRelevantDate_ << ")");
        break;
    default:
        QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
    }

    latestDate_ = pillarDate_; // backward compatibility

    fixingDate_ = iborIndex_->fixingDate(earliestDate_);
}

Date ImmFraRateHelper::getImmDate(Date asof, Size i) {
    Date imm = asof;
    for (Size j = 0; j < i; j++) {
        imm = IMM::nextDate(imm, true);
    }
    return imm;
}

void ImmFraRateHelper::accept(AcyclicVisitor& v) {
    Visitor<ImmFraRateHelper>* v1 = dynamic_cast<Visitor<ImmFraRateHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}

}; // namespace QuantExt
