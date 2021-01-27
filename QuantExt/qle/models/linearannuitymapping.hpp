/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file linearannuitymapping.hpp
    \brief linear annuity mapping function f(S) = a*S+b
*/

#pragma once

#include <orepbase/qle/models/annuitymapping.hpp>

namespace QuantExt {

/*! linear annuity mapping function f(S) = a*S+b */
class LinearAnnuityMapping : public AnnuityMapping {
public:
    LinearAnnuityMapping(const Real a, const Real b);

    Real map(const Real S) const override;
    Real mapPrime(const Real S) const override;
    Real mapPrime2(const Real S) const override;
    bool mapPrime2IsZero() const override;

    Real a() const { return a_; }
    Real b() const { return b_; }

private:
    Real a_, b_;
};

/*! linear annuity mapping builder */
class LinearAnnuityMappingBuilder : public AnnuityMappingBuilder {
public:
    LinearAnnuityMappingBuilder(const Real a, const Real b);
    LinearAnnuityMappingBuilder(const Handle<Quote>& reversion);
    boost::shared_ptr<AnnuityMapping> build(const Date& valuationDate, const Date& optionDate, const Date& paymentDate,
                                            const VanillaSwap& underlying,
                                            const Handle<YieldTermStructure>& discountCurve);

private:
    Real a_ = Null<Real>(), b_ = Null<Real>();
    Handle<Quote> reversion_;
};

} // namespace QuantExt
