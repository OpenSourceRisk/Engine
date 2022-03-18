/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file creditvolcurve.hpp
    \brief credit vol curve
*/

#pragma once

#include <qle/termstructures/creditcurve.hpp>

#include <ql/handle.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/voltermstructure.hpp>

#include <tuple>

namespace QuantExt {

class CreditVolCurve : public QuantLib::VolatilityTermStructure {
public:
    enum class Type { Price, Spread };
    CreditVolCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc);
    CreditVolCurve(const QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                   QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                   const std::vector<QuantLib::Period>& terms,
                   const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);
    CreditVolCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                   QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                   const std::vector<QuantLib::Period>& terms,
                   const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);

    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Period& underlyingTerm,
                              const QuantLib::Real strike, const Type& targetType) const;
    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const = 0;

    virtual const std::vector<QuantLib::Period>& terms() const;
    virtual const std::vector<QuantLib::Handle<CreditCurve>>& termCurves() const;
    const Type& type() const;

    Real atmStrike(const QuantLib::Date& expiry, const QuantLib::Period& term) const;
    Real atmStrike(const QuantLib::Date& expiry, const Real underlyingLength) const;

protected:
    Real moneyness(const Real strike, const Real atmStrike) const;
    Real strike(const Real moneyness, const Real atmStrike) const;
    void init() const;
    QuantLib::Date maxDate() const override;
    QuantLib::Real minStrike() const override;
    QuantLib::Real maxStrike() const override;

    std::vector<QuantLib::Period> terms_;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves_;
    Type type_;
};

class InterpolatingCreditVolCurve : public CreditVolCurve, public QuantLib::LazyObject {
public:
    InterpolatingCreditVolCurve(const QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                                QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                                const std::vector<QuantLib::Period>& terms,
                                const std::vector<QuantLib::Handle<CreditCurve>>& termCurves,
                                const std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>,
                                               QuantLib::Handle<QuantLib::Quote>>& quotes,
                                const Type& type);
    InterpolatingCreditVolCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                                QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                                const std::vector<QuantLib::Period>& terms,
                                const std::vector<QuantLib::Handle<CreditCurve>>& termCurves,
                                const std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>,
                                               QuantLib::Handle<QuantLib::Quote>>& quotes,
                                const Type& type);

    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const override;

private:
    void init() const;
    void update() override { LazyObject::update(); }
    void performCalculations() const override;
    void createSmile(const Date& expiry, const Period& term, const Date& expiry_m, const Smile& smile_m,
                     const Date& expiry_p, const Smile& smile_p);

    std::map<std::tuple<QuantLib::Period, QuantLib::Date, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes_;

    mutable std::vector<QuantLib::Period> smileTerms_;
    mutable std::vector<QuantLib::Date> smileExpiries_;
    mutable std::vector<QuantLib::Real> smileTermLengths_;
    mutable std::vector<QuantLib::Real> smileExpiryTimes_;

    using Key = std::pair<QuantLib::Date, QuantLib::Period>;
    using Smile = std::pair<Real, boost::shared_ptr<QuantLib::Interpolation>>;
    mutable std::map<Key, std::vector<Real>> strikes_;
    mutable std::map<Key, std::vector<Real>> vols_;
    mutable std::map<Key, Smile> smiles_;
};

class SpreadedCreditVolCurve : public CreditVolCurve {
public:
    SpreadedCreditVolCurve(const QuantLib::Handle<CreditVolCurve> baseCurve, const std::vector<QuantLib::Date> expiries,
                           const std::vector<QuantLib::Handle<QuantLib::Quote>> spreads, const bool stickyMoneyness);

    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const override;

    const std::vector<QuantLib::Period>& terms() const override;
    const std::vector<QuantLib::Handle<CreditCurve>>& termCurves() const override;
    const QuantLib::Date& referenceDate() const override;

private:
    QuantLib::Handle<CreditVolCurve> baseCurve_;
    std::vector<QuantLib::Date> expiries_;
    std::vector<QuantLib::Handle<QuantLib::Quote>> spreads_;
    const bool stickyMoneyness_;
};

} // namespace QuantExt
