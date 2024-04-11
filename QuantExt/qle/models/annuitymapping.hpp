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

/*! \file annuitymapping.hpp
    \brief base class for annuity mapping functions used in TSR models
*/

#pragma once

#include <ql/instruments/vanillaswap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>

namespace QuantExt {

using namespace QuantLib;

/*! base class for annuity mapping functions used in TSR models */
class AnnuityMapping {
public:
    virtual ~AnnuityMapping() {}

    /* E^A( P(t,T) / A(t) | S(t) ) */
    virtual Real map(const Real S) const = 0;

    /* first derivative of map, provides finite difference default implementation */
    virtual Real mapPrime(const Real S) const;

    /* second derivative of map, provides finite difference default implementation */
    virtual Real mapPrime2(const Real S) const;

    /* returns true if mapPrime2 == 0 for all arguments */
    virtual bool mapPrime2IsZero() const = 0;

protected:
    // step size used for finite difference derivatives
    double h_ = 1E-6;
};

/*! base class for annuity mapping builders for use in actual pricers */
class AnnuityMappingBuilder : public Observable, public Observer {
public:
    virtual ~AnnuityMappingBuilder() {}
    virtual QuantLib::ext::shared_ptr<AnnuityMapping> build(const Date& valuationDate, const Date& optionDate,
                                                    const Date& paymentDate, const VanillaSwap& underlying,
                                                    const Handle<YieldTermStructure>& discountCurve) = 0;
    void update() override;
};

} // namespace QuantExt
