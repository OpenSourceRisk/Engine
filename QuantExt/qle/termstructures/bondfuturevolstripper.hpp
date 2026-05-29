/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/termstructures/bondfuturevolstripper.hpp
    \brief strip bond future volatilities from bond future option prices.
*/

#pragma once
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltimeextrapolation.hpp>
#include <qle/utilities/solvers.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/types.hpp>
#include <ql/shared_ptr.hpp>
#include <map>

namespace QuantExt {

class BondFutureVolStripper : public QuantLib::LazyObject {
public:

    //! A simple struct to hold the call and put option price quotes for a given strike.
    struct OptionPrice {
        QuantLib::Real strike;
        QuantLib::Handle<QuantLib::Quote> callPrice;
        QuantLib::Handle<QuantLib::Quote> putPrice;

        bool operator<(const OptionPrice& other) const noexcept {
            return strike < other.strike;
        }
    };

    // Store quote surface by expiry date key.
    using QuoteSurface = std::map<QuantLib::Date, std::vector<OptionPrice>>;

    using TimeExtrapType = QuantLib::BlackVolTimeExtrapolation::Type;

    BondFutureVolStripper(QuantLib::Date referenceDate,
        QuantLib::Calendar calendar,
        QuantLib::BusinessDayConvention bdc,
        QuantLib::DayCounter dayCounter,
        QuantLib::Handle<QuantLib::Quote> futurePrice,
        QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        QuoteSurface quotes,
        Solver1DOptions solverOptions,
        QuantLib::Exercise::Type type = QuantLib::Exercise::European,
        bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true,
        TimeExtrapType timeExtrapolationType = TimeExtrapType::FlatVolatility,
        bool preferOutOfTheMoney = false);

    //! Return the stripped volatility structure.
    QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> volSurface();

    //! If any grid points fail to strip, this method provides the error messages.
    const std::vector<std::string>& errorMessages() const;

private:
    void performCalculations() const override;

    // Function object used in solving.
    class PriceError {
    public:
        PriceError(const QuantLib::VanillaOption& option,
            QuantLib::SimpleQuote& volatility,
            QuantLib::Real targetPrice);

        QuantLib::Real operator()(QuantLib::Volatility volatility) const;

    private:
        const QuantLib::VanillaOption& option_;
        QuantLib::SimpleQuote& volatility_;
        QuantLib::Real targetPrice_;
    };

    // Data needed for the creation of the volatility surface after the volatilities have been stripped.
    QuantLib::Date referenceDate_;
    QuantLib::Calendar calendar_;
    QuantLib::BusinessDayConvention bdc_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::Exercise::Type type_;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
    TimeExtrapType timeExtrapolationType_;

    // Data needed for the stripping process.
    QuantLib::Handle<QuantLib::Quote> futurePrice_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuoteSurface quotes_;
    Solver1DOptions solverOptions_;
    bool preferOutOfTheMoney_;

    // Store any error messages during the stripping process.
    mutable std::vector<std::string> errorMessages_;

    // Volatility structure built from the stripped volatilities.
    mutable QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> volSurface_;

    // Store the function that will be called each time to solve for volatility
    QuantLib::Brent brent_;
    std::function<QuantLib::Real(const PriceError&, QuantLib::Real)> solver_;

    // Set up solver according to the solver options.
    void setUpSolver();

    // For given prices, find starting position of the stripping.
    QuantLib::Size findStartPos(const std::vector<OptionPrice>& optionPrices) const;

    // Strip volatilities for the given prices and add to `expiries`, `strikes`, and `vols`.
    // Return the first stripped volatility value.
    QuantLib::ext::optional<QuantLib::Volatility> stripVols(const std::vector<OptionPrice>& prices,
        QuantLib::Size startPos, QuantLib::Size endPos, std::vector<QuantLib::Date>& expiries,
        std::vector<QuantLib::Real>& strikes, std::vector<QuantLib::Volatility>& vols, QuantLib::Real initialGuess,
        const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise,
        const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& engine,
        QuantLib::SimpleQuote& volQuote, bool asc) const;
};

} // namespace QuantExt
