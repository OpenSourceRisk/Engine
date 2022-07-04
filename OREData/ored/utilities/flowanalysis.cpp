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

#include <iomanip>
#include <ored/utilities/flowanalysis.hpp>
#include <ored/utilities/to_string.hpp>
#include <ostream>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
using namespace std;
using QuantLib::Date;
using QuantLib::Real;

namespace ore {
namespace data {

class AnalysisGenerator : public QuantLib::AcyclicVisitor,
                          public QuantLib::Visitor<QuantLib::CashFlow>,
                          public QuantLib::Visitor<QuantLib::IndexedCashFlow>,
                          public QuantLib::Visitor<QuantLib::Coupon>,
                          public QuantLib::Visitor<QuantLib::FloatingRateCoupon>,
                          public QuantLib::Visitor<QuantExt::AverageONIndexedCoupon>,
                          public QuantLib::Visitor<QuantLib::AverageBMACoupon>,
                          public QuantLib::Visitor<QuantExt::FXLinkedCashFlow>,
                          public QuantLib::Visitor<QuantExt::AverageFXLinkedCashFlow>,
                          public QuantLib::Visitor<QuantExt::FloatingRateFXLinkedNotionalCoupon>,
                          public QuantLib::Visitor<QuantLib::InflationCoupon> {
private:
    vector<vector<string>> flowAnalysis_;
    static const QuantLib::Size numberOfColumns_ = 5;

public:
    AnalysisGenerator();
    void reset();
    void visit(QuantLib::CashFlow& c) override;
    void visit(QuantLib::IndexedCashFlow& c) override;
    void visit(QuantLib::Coupon& c) override;
    void visit(QuantLib::FloatingRateCoupon& c) override;
    void visit(QuantExt::AverageONIndexedCoupon& c) override;
    void visit(QuantLib::AverageBMACoupon& c) override;
    void visit(QuantExt::FXLinkedCashFlow& c) override;
    void visit(QuantExt::AverageFXLinkedCashFlow& c) override;
    void visit(QuantExt::FloatingRateFXLinkedNotionalCoupon& c) override;
    void visit(QuantLib::InflationCoupon& c) override;
    const vector<vector<string>>& analysis() const;
};

#define PAYMENT_DATE 0
#define ACCRUAL_START_DATE 1
#define ACCRUAL_END_DATE 2
#define FIXING_DATE 3
#define INDEX 4

AnalysisGenerator::AnalysisGenerator() { reset(); }

void AnalysisGenerator::reset() {
    flowAnalysis_.clear();

    vector<string> headings(numberOfColumns_);
    headings[PAYMENT_DATE] = string("Payment Date");
    headings[ACCRUAL_START_DATE] = string("Accrual Start Date");
    headings[ACCRUAL_END_DATE] = string("Accrual End Date");
    headings[FIXING_DATE] = string("Fixing Date");
    headings[INDEX] = string("Index");

    flowAnalysis_.push_back(headings);
}

void AnalysisGenerator::visit(QuantLib::CashFlow& c) {
    vector<string> cf(numberOfColumns_, "#N/A");
    cf[PAYMENT_DATE] = to_string(c.date());
    flowAnalysis_.push_back(cf);
}

void AnalysisGenerator::visit(QuantLib::Coupon& c) {
    visit(static_cast<QuantLib::CashFlow&>(c));
    flowAnalysis_.back()[ACCRUAL_START_DATE] = to_string(c.accrualStartDate());
    flowAnalysis_.back()[ACCRUAL_END_DATE] = to_string(c.accrualEndDate());
};

void AnalysisGenerator::visit(QuantLib::FloatingRateCoupon& c) {
    visit(static_cast<QuantLib::Coupon&>(c));
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fixingDate());
    flowAnalysis_.back()[INDEX] = c.index()->name();
}

void AnalysisGenerator::visit(QuantExt::AverageONIndexedCoupon& c) {
    for (auto& d : c.fixingDates()) {
        visit(static_cast<QuantLib::Coupon&>(c));
        flowAnalysis_.back()[FIXING_DATE] = to_string(d);
        flowAnalysis_.back()[INDEX] = c.index()->name();
    }
}

void AnalysisGenerator::visit(QuantLib::AverageBMACoupon& c) {
    for (auto& d : c.fixingDates()) {
        visit(static_cast<QuantLib::Coupon&>(c));
        flowAnalysis_.back()[FIXING_DATE] = to_string(d);
        flowAnalysis_.back()[INDEX] = c.index()->name();
    }
}

void AnalysisGenerator::visit(QuantExt::FXLinkedCashFlow& c) {
    visit(static_cast<QuantLib::CashFlow&>(c));
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fxFixingDate());
    flowAnalysis_.back()[INDEX] = c.fxIndex()->name();
}

void AnalysisGenerator::visit(QuantExt::AverageFXLinkedCashFlow& c) {
    visit(static_cast<QuantLib::CashFlow&>(c));
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fxFixingDates().back());
    flowAnalysis_.back()[INDEX] = c.fxIndex()->name();
}

void AnalysisGenerator::visit(QuantExt::FloatingRateFXLinkedNotionalCoupon& c) {
    // Ibor
    visit(static_cast<QuantLib::FloatingRateCoupon&>(c));
    // FX
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fxFixingDate());
    flowAnalysis_.back()[INDEX] = c.fxIndex()->name();
}

void AnalysisGenerator::visit(QuantLib::InflationCoupon& c) {
    visit(static_cast<QuantLib::Coupon&>(c));
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fixingDate());
    flowAnalysis_.back()[INDEX] = c.index()->name();
}

void AnalysisGenerator::visit(QuantLib::IndexedCashFlow& c) {
    visit(static_cast<QuantLib::CashFlow&>(c));
    flowAnalysis_.back()[FIXING_DATE] = to_string(c.fixingDate());
    flowAnalysis_.back()[INDEX] = c.index()->name();
}
const vector<vector<string>>& AnalysisGenerator::analysis() const { return flowAnalysis_; }

vector<vector<string>> flowAnalysis(const QuantLib::Leg& leg) {
    AnalysisGenerator generator;
    for (QuantLib::Size i = 0; i < leg.size(); ++i)
        leg[i]->accept(generator);
    return generator.analysis();
}
} // namespace data
} // namespace ore
