/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadedinflationcurve.hpp
    \brief spreaded inflation term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedZeroInflationCurve : public ZeroInflationTermStructure, public LazyObject {
public:
    //! times should be consistent with reference ts day counter
    SpreadedZeroInflationCurve(const Handle<ZeroInflationTermStructure>& referenceCurve, const std::vector<Time>& times,
                               const std::vector<Handle<Quote>>& quotes);

    Date maxDate() const override;
    void update() override;
    const Date& referenceDate() const override;

    Calendar calendar() const override;
    Natural settlementDays() const override;

    Date baseDate() const override;

protected:
    void performCalculations() const override;
    Rate zeroRateImpl(Time t) const override;

private:
    Handle<ZeroInflationTermStructure> referenceCurve_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> quotes_;
    mutable std::vector<Real> data_;
    QuantLib::ext::shared_ptr<Interpolation> interpolation_;
};

class SpreadedYoYInflationCurve : public YoYInflationTermStructure, public LazyObject {
public:
    //! times should be consistent with reference ts day counter
    SpreadedYoYInflationCurve(const Handle<YoYInflationTermStructure>& referenceCurve, const std::vector<Time>& times,
                              const std::vector<Handle<Quote>>& quotes);

    Date maxDate() const override;
    void update() override;
    const Date& referenceDate() const override;

    Calendar calendar() const override;
    Natural settlementDays() const override;

    Date baseDate() const override;

protected:
    void performCalculations() const override;
    Rate yoyRateImpl(Time t) const override;

private:
    Handle<YoYInflationTermStructure> referenceCurve_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> quotes_;
    mutable std::vector<Real> data_;
    QuantLib::ext::shared_ptr<Interpolation> interpolation_;
};

} // namespace QuantExt
