/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file lienarannuitymapping.hpp
    \brief lienar annuity mapping function f(S) = a*S+b
*/

#include <orepbase/qle/models/linearannuitymapping.hpp>

#include <ql/cashflows/coupon.hpp>

namespace QuantExt {

LinearAnnuityMapping::LinearAnnuityMapping(const Real a, const Real b) : a_(a), b_(b) {}

Real LinearAnnuityMapping::map(const Real S) const { return a_ * S + b_; }

Real LinearAnnuityMapping::mapPrime(const Real S) const { return a_; }

Real LinearAnnuityMapping::mapPrime2(const Real S) const { return 0.0; }

bool LinearAnnuityMapping::mapPrime2IsZero() const { return true; }

LinearAnnuityMappingBuilder::LinearAnnuityMappingBuilder(const Real a, const Real b) : a_(a), b_(b) {}
LinearAnnuityMappingBuilder::LinearAnnuityMappingBuilder(const Handle<Quote>& reversion) : reversion_(reversion) {
    registerWith(reversion_);
}

namespace {
Real GsrG(const Real yf, const Real reversion) {
    if (std::fabs(reversion) < 1.0E-4)
        return yf;
    else
        return (1.0 - std::exp(-reversion * yf)) / reversion;
}
} // namespace

boost::shared_ptr<AnnuityMapping> LinearAnnuityMappingBuilder::build(const Date& valuationDate, const Date& optionDate,
                                                                     const Date& paymentDate,
                                                                     const VanillaSwap& underlying,
                                                                     const Handle<YieldTermStructure>& discountCurve) {

    // no need for an acutal mapping, since the coupon amount is deterministic, i.e. model-independent

    if (optionDate <= valuationDate)
        return boost::make_shared<LinearAnnuityMapping>(0.0, 0.0);

    // build the mapping dependent on whether a, b or a reversion is given

    if (a_ != Null<Real>() && b_ != Null<Real>()) {
        return boost::make_shared<LinearAnnuityMapping>(a_, b_);
    } else if (!reversion_.empty()) {
        Real atmForward = underlying.fairRate();
        Real gx = 0.0, gy = 0.0;
        for (Size i = 0; i < underlying.fixedLeg().size(); i++) {
            boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(underlying.fixedLeg()[i]);
            Real yf = c->accrualPeriod();
            Real pv = yf * discountCurve->discount(c->date());
            gx += pv * GsrG(discountCurve->dayCounter().yearFraction(optionDate, c->date()), reversion_->value());
            gy += pv;
        }
        Real gamma = gx / gy;
        Date lastd = underlying.fixedLeg().back()->date();
        Real a =
            discountCurve->discount(paymentDate) *
            (gamma - GsrG(discountCurve->dayCounter().yearFraction(optionDate, paymentDate), reversion_->value())) /
            (discountCurve->discount(lastd) *
                 GsrG(discountCurve->dayCounter().yearFraction(optionDate, lastd), reversion_->value()) +
             atmForward * gy * gamma);
        Real b = discountCurve->discount(paymentDate) / gy - a * atmForward;
        return boost::make_shared<LinearAnnuityMapping>(a, b);
    } else {
        QL_FAIL("LinearAnnuityMapping::build(): failed, because neither a, b nor a reversion is given");
    }
}

} // namespace QuantExt
