/*
 Copyright (C) 2026 AcadiaSoft Inc
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

#include <numeric>
#include <ql/utilities/vectors.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/cashflows/intradaypowercashflow.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::CashFlow;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Visitor;
using std::vector;

namespace QuantExt {

IntradayPowerCashFlow::IntradayPowerCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate,
                                             const QuantLib::Date& endDate, const QuantLib::Date& paymentDate,
                                             const ext::shared_ptr<IntradayPowerIndex>& index,
                                             const ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve,
                                             const QuantLib::Calendar& pricingCalendar, QuantLib::Real spread,
                                             QuantLib::Real gearing, bool includeStartDate, bool includeEndDate,
                                             const ext::shared_ptr<FxIndex>& fxIndex,
                                             std::optional<QuantLib::Natural> avgPricePrecision)
    : startDate_(startDate), endDate_(endDate), paymentDate_(paymentDate), loadCurve_(loadCurve),
      pricingCalendar_(pricingCalendar), spread_(spread), gearing_(gearing), includeStartDate_(includeStartDate),
      includeEndDate_(includeEndDate), fxIndex_(fxIndex), avgPricePrecision_(avgPricePrecision) {
    init(quantity, index);
}

void IntradayPowerCashFlow::init(const QuantLib::Real quantity, const ext::shared_ptr<IntradayPowerIndex>& index) {
    computePeriodQuantity(quantity);
    rolloutIndices(index);
    initWeights();
}

void IntradayPowerCashFlow::computePeriodQuantity(const QuantLib::Real quantity) { periodQuantity_ = quantity; }

void IntradayPowerCashFlow::rolloutIndices(const ext::shared_ptr<IntradayPowerIndex>& index) {
    auto deliveryDates =
        pricingDates(startDate_, endDate_, pricingCalendar_, !includeStartDate_, includeEndDate_, true);
    for (const auto& d : deliveryDates) {
        auto loadProfile = loadCurve_ != nullptr ? loadCurve_->loadProfile(d) : nullptr;
        indices_.push_back({d, index->clone(d, loadProfile)});
    }
}

void IntradayPowerCashFlow::initWeights() {
    double totalLoad = 0.0;
    for (const auto& kv : indices_) {
        auto loadProfile = loadCurve_ != nullptr ? loadCurve_->loadProfile(kv.first) : nullptr;
        // If we have a load profile the weight we dont assume equal weight for each day, so we need to weight it by
        // total period load
        if (loadProfile) {
            weights_[kv.first] += loadProfile->totalMWh();
            totalLoad += loadProfile->totalMWh();
        } else {
            weights_[kv.first] = 1.0;
            totalLoad += 1.0;
        }
    }
    for (auto& kv : weights_) {
        kv.second /= totalLoad == 0 ? 1.0 : totalLoad;
    }
}

void IntradayPowerCashFlow::performCalculations() const {

    // Calculate the average price
    averagePricePerMWh_ = 0.0;
    Real fxRate = 0.0;
    for (const auto& kv : indices_) {
        fxRate = (fxIndex_) ? this->fxIndex()->fixing(kv.first) : 1.0;
        averagePricePerMWh_ += fxRate * kv.second->fixing(kv.first) * weights_.at(kv.first);
    }

    if (avgPricePrecision_.has_value()) {
        // First, round to 8 decimal places to avoid floating point problems
        static const QuantLib::Natural preRoundPrecision = 8;
        QuantLib::ClosestRounding preRound(preRoundPrecision);
        averagePricePerMWh_ = preRound(averagePricePerMWh_);

        QuantLib::ClosestRounding round(*avgPricePrecision_);
        averagePricePerMWh_ = round(averagePricePerMWh_);
    }

    // Amount is just average price times quantity
    // In case of Foreign currency settlement, the spread must be expressed in Foreign currency units
    amount_ = periodQuantity_ * (gearing_ * averagePricePerMWh_ + spread_);
}

Real IntradayPowerCashFlow::amount() const {
    calculate();
    return amount_;
}

Real IntradayPowerCashFlow::fixing() const {
    calculate();
    return averagePricePerMWh_;
}

void IntradayPowerCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<IntradayPowerCashFlow>* v1 = dynamic_cast<Visitor<IntradayPowerCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

IntradayPowerLeg::IntradayPowerLeg(const Schedule& schedule, const ext::shared_ptr<IntradayPowerIndex>& index,
                                   const ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve)
    : schedule_(schedule), index_(index), loadCurve_(loadCurve), paymentLag_(0), paymentCalendar_(NullCalendar()),
      paymentConvention_(Unadjusted), pricingCalendar_(Calendar()), includeEndDate_(true), includeStartDate_(false) {}

IntradayPowerLeg& IntradayPowerLeg::withQuantities(Real quantity) {
    quantities_ = vector<Real>(1, quantity);
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withQuantities(const vector<Real>& quantities) {
    quantities_ = quantities;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withPaymentCalendar(const Calendar& paymentCalendar) {
    paymentCalendar_ = paymentCalendar;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withPaymentConvention(BusinessDayConvention paymentConvention) {
    paymentConvention_ = paymentConvention;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withPricingCalendar(const Calendar& pricingCalendar) {
    pricingCalendar_ = pricingCalendar;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withSpreads(Real spread) {
    spreads_ = vector<Real>(1, spread);
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withSpreads(const vector<Real>& spreads) {
    spreads_ = spreads;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withGearings(Real gearing) {
    gearings_ = vector<Real>(1, gearing);
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withGearings(const vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::includeEndDate(bool flag) {
    includeEndDate_ = flag;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::includeStartDate(bool flag) {
    includeStartDate_ = flag;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withPaymentDates(const vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex) {
    fxIndex_ = fxIndex;
    return *this;
}

IntradayPowerLeg& IntradayPowerLeg::withAvgPricePrecision(std::optional<QuantLib::Natural> precision) {
    avgPricePrecision_ = precision;
    return *this;
}

IntradayPowerLeg::operator Leg() const {

    // Number of cashflows
    bool singleDateSchedule = schedule_.size() == 1;
    Size numberCashflows = singleDateSchedule ? 1 : schedule_.size() - 1;

    // Initial consistency checks
    QL_REQUIRE(!quantities_.empty(), "No quantities given");
    QL_REQUIRE(quantities_.size() <= numberCashflows,
               "Too many quantities (" << quantities_.size() << "), only " << numberCashflows << " required");

    QL_REQUIRE(paymentDates_.empty() || paymentDates_.size() == numberCashflows,
               "Expected the number of explicit payment dates ("
                   << paymentDates_.size() << ") to equal the number of calculation periods (" << numberCashflows
                   << ")");

    // Leg to hold the result
    // We always include the schedule start and schedule termination date in the averaging so the first and last
    // coupon have special treatment here that overrides the includeEndDate_ and includeStartDate_ flags
    Leg leg;
    leg.reserve(numberCashflows);
    for (Size i = 0; i < numberCashflows; ++i) {

        Date start = schedule_.date(i);
        Date end = singleDateSchedule ? start : schedule_.date(i + 1);
        bool includeStart = i == 0 ? true : includeStartDate_;
        bool includeEnd = i == numberCashflows - 1 ? true : includeEndDate_;
        Real quantity = detail::get(quantities_, i, 1.0);
        Real spread = detail::get(spreads_, i, 0.0);
        Real gearing = detail::get(gearings_, i, 1.0);
        Date paymentDate = detail::get(paymentDates_, i, Date());

        if (paymentDate == Date()) {
            // Otherwise calculate payment date by applying lag and convention to the end date of the period
            paymentDate = paymentCalendar_.advance(end, paymentLag_ * Days, paymentConvention_);
        }

        leg.push_back(ext::make_shared<IntradayPowerCashFlow>(quantity, start, end, paymentDate, index_, loadCurve_,
                                                              pricingCalendar_, spread, gearing, includeStart,
                                                              includeEnd, fxIndex_, avgPricePrecision_));
    }

    return leg;
}

} // namespace QuantExt
