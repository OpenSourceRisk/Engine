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
#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <map>
#include <tuple>

namespace QuantExt {

class CreditVolCurve : public QuantLib::VolatilityTermStructure, public QuantLib::LazyObject {
public:
    enum class Type { Price, Spread };
    CreditVolCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                   const std::vector<QuantLib::Period>& terms,
                   const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);
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
    virtual QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                                      const QuantLib::Real strike, const Type& targetType) const = 0;

    /* Interpolates between the volatility for d and d+1 where d is a date such that
       timeFromRef(d) <= exerciseTime <= timeFromRef(d+1). */
    QuantLib::Real volatility(const QuantLib::Real exerciseTime, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const;

    virtual const std::vector<QuantLib::Period>& terms() const;
    virtual const std::vector<QuantLib::Handle<CreditCurve>>& termCurves() const;
    const Type& type() const;

    QuantLib::Real atmStrike(const QuantLib::Date& expiry, const QuantLib::Period& term) const;
    QuantLib::Real atmStrike(const QuantLib::Date& expiry, const QuantLib::Real underlyingLength) const;

    QuantLib::Real minStrike() const override;
    QuantLib::Real maxStrike() const override;
    QuantLib::Date maxDate() const override;

protected:
    void init();
    void update() override { LazyObject::update(); }
    void performCalculations() const override;
    QuantLib::Real moneyness(const QuantLib::Real strike, const QuantLib::Real atmStrike) const;
    QuantLib::Real strike(const QuantLib::Real moneyness, const QuantLib::Real atmStrike) const;

    std::vector<QuantLib::Period> terms_;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves_;
    Type type_;

    mutable std::map<std::pair<QuantLib::Date, double>, double> atmStrikeCache_;
};

class InterpolatingCreditVolCurve : public CreditVolCurve {
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
    using Smile = std::pair<QuantLib::Real, QuantLib::ext::shared_ptr<QuantLib::Interpolation>>;
    using Key = std::pair<QuantLib::Date, QuantLib::Period>;

    void init();
    void performCalculations() const override;
    void createSmile(const QuantLib::Date& expiry, const QuantLib::Period& term, const QuantLib::Date& expiry_m,
                     const QuantLib::Date& expiry_p) const;

    std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes_;

    mutable std::vector<QuantLib::Period> smileTerms_;
    mutable std::vector<QuantLib::Date> smileExpiries_;
    mutable std::vector<QuantLib::Real> smileTermLengths_;
    mutable std::vector<QuantLib::Real> smileExpiryTimes_;

    mutable std::map<Key, std::vector<QuantLib::Real>> strikes_;
    mutable std::map<Key, std::vector<QuantLib::Real>> vols_;
    mutable std::map<Key, Smile> smiles_;
};

class ProxyCreditVolCurve : public CreditVolCurve {
public:
    // if no terms / termCurves are given, the source terms / termCurves are used for this surface
    ProxyCreditVolCurve(const QuantLib::Handle<CreditVolCurve>& source, const std::vector<QuantLib::Period>& terms = {},
                        const std::vector<QuantLib::Handle<CreditCurve>>& termCurves = {});
    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const override;

    const QuantLib::Date& referenceDate() const override;

private:
    QuantLib::Handle<CreditVolCurve> source_;
};

class SpreadedCreditVolCurve : public CreditVolCurve {
public:
    /* for stickyMoneyness = true and a base curve that is strike dependent, both the base curve and this curve must have
       terms and termCurves defined where the former do not react to spread movements while the latter represent the moving ATM level */
    SpreadedCreditVolCurve(const QuantLib::Handle<CreditVolCurve> baseCurve, const std::vector<QuantLib::Date> expiries,
                           const std::vector<QuantLib::Handle<QuantLib::Quote>> spreads, const bool stickyMoneyness,
                           const std::vector<QuantLib::Period>& terms = {},
                           const std::vector<QuantLib::Handle<CreditCurve>>& termCurves = {});

    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const override;

    const QuantLib::Date& referenceDate() const override;

private:
    void performCalculations() const override;

    QuantLib::Handle<CreditVolCurve> baseCurve_;
    std::vector<QuantLib::Date> expiries_;
    std::vector<QuantLib::Handle<QuantLib::Quote>> spreads_;
    const bool stickyMoneyness_;

    mutable std::vector<QuantLib::Real> times_;
    mutable std::vector<QuantLib::Real> spreadValues_;
    mutable QuantLib::ext::shared_ptr<QuantLib::Interpolation> interpolatedSpreads_;
};

class CreditVolCurveWrapper : public CreditVolCurve {
public:
    explicit CreditVolCurveWrapper(const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol);

    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike, const Type& targetType) const override;

    const QuantLib::Date& referenceDate() const override;

private:
    QuantLib::Handle<QuantLib::BlackVolTermStructure> vol_;
};

class BlackVolFromCreditVolWrapper : public QuantLib::BlackVolatilityTermStructure {
public:
    explicit BlackVolFromCreditVolWrapper(const QuantLib::Handle<QuantExt::CreditVolCurve>& vol,
                                          const QuantLib::Real underlyingLength);

    const QuantLib::Date& referenceDate() const override;
    QuantLib::Real minStrike() const override;
    QuantLib::Real maxStrike() const override;
    QuantLib::Date maxDate() const override;

private:
    QuantLib::Real blackVolImpl(QuantLib::Real t, QuantLib::Real strike) const override;
    QuantLib::Handle<QuantExt::CreditVolCurve> vol_;
    QuantLib::Real underlyingLength_;
};

} // namespace QuantExt
