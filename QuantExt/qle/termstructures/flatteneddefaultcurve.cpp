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

#include <qle/instruments/creditdefaultswap.hpp>
#include <qle/pricingengines/midpointcdsengine.hpp>
#include <qle/termstructures/flatteneddefaultcurve.hpp>

#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {

FlattenedDefaultCurve::FlattenedDefaultCurve(const Handle<DefaultProbabilityTermStructure>& source,
                                             const Handle<Quote>& recovery, const Handle<YieldTermStructure>& discount,
                                             const Date& samplingDate)
    : SurvivalProbabilityStructure(source->dayCounter()), source_(source), recovery_(recovery), discount_(discount),
      samplingDate_(samplingDate) {
    registerWith(source_);
    registerWith(recovery_);
    registerWith(recovery_);
    registerWith(discount_);
}

void FlattenedDefaultCurve::update() { LazyObject::update(); }

void FlattenedDefaultCurve::performCalculations() const {

    QL_REQUIRE(source_->referenceDate() < samplingDate_,
               "FlattenedDefaultCurve: source curve ref date ("
                   << source_->referenceDate() << ") must be before sampling date (" << samplingDate_ << ")");

    // hardcoded standard cds conventions

    Schedule schedule = MakeSchedule()
                            .from(source_->referenceDate())
                            .to(samplingDate_)
                            .withCalendar(WeekendsOnly())
                            .withFrequency(Quarterly)
                            .withConvention(Following)
                            .withTerminationDateConvention(Unadjusted)
                            .withRule(DateGeneration::CDS2015);

    CreditDefaultSwap cds(Protection::Buyer, 1E8, 0.01, schedule, Following, Actual360(), true,
                          CreditDefaultSwap::ProtectionPaymentTime::atDefault, source_->referenceDate(),
                          boost::shared_ptr<Claim>(), Actual360(true), source_->referenceDate(), 3);

    auto engine = boost::make_shared<QuantExt::MidPointCdsEngine>(source_, 0.4, discount_);

    cds.setPricingEngine(engine);
    Real fairSpread = cds.fairSpread();
    std::cout << "flattened curve recalc, fair spread = " << fairSpread << std::endl;

    CreditDefaultSwap cdsf(Protection::Buyer, 1E8, fairSpread, schedule, Following, Actual360(), true,
                           CreditDefaultSwap::ProtectionPaymentTime::atDefault, source_->referenceDate(),
                           boost::shared_ptr<Claim>(), Actual360(true), source_->referenceDate(), 3);
    cdsf.setPricingEngine(engine);

    try {
        flatRate_ = cdsf.impliedHazardRate(0.0, discount_, source_->dayCounter(), 0.4, 1E-8);
    } catch (...) {
        // if the above should fail, fall back to a less accurate estimate
        flatRate_ = -std::log(source_->survivalProbability(samplingDate_)) / source_->timeFromReference(samplingDate_);
        std::cout << "USE FALLBACK RATE!" << std::endl;
    }

    std::cout << "flattened curve recalc, implied hazard = " << flatRate_ << std::endl;
}

Real FlattenedDefaultCurve::survivalProbabilityImpl(Time t) const {
    calculate();
    return std::exp(-flatRate_ * t);
}

} // namespace QuantExt
