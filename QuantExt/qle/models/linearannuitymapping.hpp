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

/*! \file linearannuitymapping.hpp
    \brief linear annuity mapping function f(S) = a*S+b
*/

#pragma once

#include <qle/models/annuitymapping.hpp>

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
    QuantLib::ext::shared_ptr<AnnuityMapping> build(const Date& valuationDate, const Date& optionDate, const Date& paymentDate,
                                            const VanillaSwap& underlying,
                                            const Handle<YieldTermStructure>& discountCurve) override;

private:
    Real a_ = Null<Real>(), b_ = Null<Real>();
    Handle<Quote> reversion_;
};

} // namespace QuantExt
