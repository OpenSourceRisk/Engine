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

/*! \file qle/termstructures/indexcdsvolstripper.hpp
    \brief strip index CDS volatilities from option prices.
*/

#pragma once

#include <qle/instruments/indexcdsoption.hpp>
#include <qle/termstructures/creditvolcurve.hpp>
#include <qle/termstructures/creditcurve.hpp>
#include <qle/pricingengines/indexcdsoptionbaseengine.hpp>
#include <qle/utilities/solvers.hpp>
#include <map>

namespace QuantExt {

class IndexCdsVolStripper : public QuantLib::LazyObject {
public:
    //! Enumeration to allow user to override default engine choice.
    enum class OptionEngine {
        None,
        Black,
        Numerical
    };

    //! Indicates what the premium quote should be multiplied by to give the actual premium amount.
    enum class QuoteDimension {
        // The premium quote should be applied to the full option notional scaled by the current index factor.
        BpsPerOutstandingNtl,
        // The premium quote should be applied to the full option notional.
        BpsPerOptionNtl
    };

    //! A struct to hold the data needed to build index CDS options used in the stripping process.
    struct TradeData {
        CreditCurve::RefData indexCdsData;
        CdsOption::StrikeType strikeType = CdsOption::Spread;
        QuantLib::Real indexFactor = 1.0;
        QuantLib::Real indexFactorStrike = 1.0;
        QuantLib::Real realisedFepFactor = 0.0;
        // If set to `None`, a default is chosen. Otherwise, it overrides the default.
        OptionEngine engineOverride = OptionEngine::None;
        QuoteDimension quoteDimension = QuoteDimension::BpsPerOutstandingNtl;
    };

    //! A simple struct to hold the payer and receiver option price quotes for a given strike.
    struct OptionPrice {
        QuantLib::Real strike;
        QuantLib::Handle<QuantLib::Quote> payerPrice;
        QuantLib::Handle<QuantLib::Quote> receiverPrice;

        bool operator<(const OptionPrice& other) const noexcept {
            return strike < other.strike;
        }
    };

    // Store quote surface by expiry date key.
    using QuoteSurface = std::map<QuantLib::Date, std::vector<OptionPrice>>;
    // For each underlying index CDS term, store a quote surface.
    using QuoteCube = std::map<QuantLib::Period, QuoteSurface>;

    IndexCdsVolStripper(QuantLib::Date referenceDate,
        QuantLib::Calendar calendar,
        QuantLib::BusinessDayConvention bdc,
        QuantLib::DayCounter dayCounter,
        const std::map<QuantLib::Period, QuantLib::Handle<CreditCurve>>& termCurves,
        std::map<QuantLib::Period, QuantLib::Date> termMaturities,
        QuoteCube quotes,
        TradeData tradeData,
        Solver1DOptions solverOptions);

    QuantLib::ext::shared_ptr<CreditVolCurve> creditVolCurve();

    //! If any grid points fail to strip, this method provides the error messages.
    const std::vector<std::string>& errorMessages() const;

private:
    void performCalculations() const override;

    static constexpr QuantLib::Real notional_ = 10000.0;

    // Data needed to create InterpolatingCreditVolCurve when volatility quotes have been stripped.
    QuantLib::Date referenceDate_;
    QuantLib::Calendar calendar_;
    QuantLib::BusinessDayConvention bdc_;
    QuantLib::DayCounter dayCounter_;
    std::map<QuantLib::Period, QuantLib::Date> termMaturities_;
    std::vector<QuantLib::Period> terms_;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves_;

    // Data needed for the stripping process.
    QuoteCube quotes_;
    TradeData tradeData_;
    Solver1DOptions solverOptions_;
    OptionEngine optionEngine_;

    // Store any error messages during the stripping process.
    mutable std::vector<std::string> errorMessages_;

    // For each term (outer Period key) and expiry (inner Date key), we need an index CDS option payer and receiver 
    // helper instrument.
    using OptionPtr = QuantLib::ext::shared_ptr<IndexCdsOption>;
    struct PayRecHelper {
        OptionPtr payerHelper;
        OptionPtr receiverHelper;
    };
    using Helpers = std::map<QuantLib::Period, std::map<QuantLib::Date, PayRecHelper>>;
    Helpers helpers_;

    // Each term may need a separate engine and associated volatility.
    using EnginePtr = QuantLib::ext::shared_ptr<IndexCdsOptionBaseEngine>;
    using VolHandle = QuantLib::Handle<QuantLib::SimpleQuote>;
    struct EngineVol {
        EnginePtr engine;
        VolHandle vol;
    };
    std::map<QuantLib::Period, EngineVol> termEngineVol_;

    // Credit volatility curve built from the stripped volatilities.
    mutable QuantLib::ext::shared_ptr<CreditVolCurve> creditVolCurve_;

    // Add an engine and associated volatility for each term.
    void populateEngineAndVols(const QuantLib::Period& term, const QuantLib::Handle<CreditCurve>& cch);

    // Add a payer and receiver option for the expiry date and term.
    void populateOptionHelpers(const QuantLib::Date& expiryDate, const QuantLib::Period& term,
        const QuantLib::Handle<CreditCurve>& cch);

    // Create option helper.
    QuantLib::ext::shared_ptr<QuantExt::IndexCdsOption> createIndexCdsOption(QuantLib::Protection::Side side,
        const QuantLib::Date& expiryDate, const QuantLib::Schedule& schedule, const QuantLib::Handle<CreditCurve>& cch,
        const QuantLib::Period& term);

    // For given prices, find starting position of the stripping.
    QuantLib::Size findStartPos(const std::vector<OptionPrice>& optionPrices) const;

    // Strip volatilities for the given prices and add them to the `volQuotes`.
    // Return the first stripped volatility value.
    using VolQuotesMap = InterpolatingCreditVolCurve::QuoteMap;
    QuantLib::ext::optional<QuantLib::Volatility> stripVols(const std::vector<OptionPrice>& prices,
        QuantLib::Size startPos, QuantLib::Size endPos, VolQuotesMap& volQuotes, const PayRecHelper& payRecHelper,
        const EngineVol& engineVol, Solver1DOptions& solverOptions, const QuantLib::Date& expiryDate,
        const QuantLib::Period& term, QuantLib::DiscountFactor discToPremPmt, bool asc) const;
};

} // namespace QuantExt
