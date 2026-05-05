/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/indexcdsoption.hpp
    \brief Index CDS option instrument
*/

#pragma once

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/instruments/cdsoption.hpp>
#include <qle/utilities/solvers.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantExt {

class IndexCdsOptionBaseEngine;

//! Index CDS option instrument
class IndexCdsOption : public QuantLib::Option {
public:
    class arguments;
    class results;
    class engine;

    IndexCdsOption(const QuantLib::ext::shared_ptr<IndexCreditDefaultSwap>& swap,
                   const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise,
                   QuantLib::Real strike,
                   CdsOption::StrikeType strikeType_ = CdsOption::Spread,
                   Settlement::Type settlementType = Settlement::Cash,
                   QuantLib::Real tradeDateNtl = QuantLib::Null<QuantLib::Real>(),
                   QuantLib::Real realisedFep = QuantLib::Null<QuantLib::Real>(),
                   const QuantLib::Period& indexTerm = 5 * Years);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments* args) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<IndexCreditDefaultSwap>& underlyingSwap() const { return swap_; }
    //@}

    //! \name Calculations
    //@{
    QuantLib::Real atmRate() const;
    QuantLib::Real riskyAnnuity() const;
    //! Return the front end protection (FEP) adjusted forward price.
    QuantLib::Real fepAdjustedForwardPrice() const;
    //! Return the front end protection (FEP) adjusted forward spread.
    QuantLib::Spread fepAdjustedForwardSpread() const;
    //! Calculate implied volatility using `BlackIndexCdsOptionEngine`.
    QuantLib::Volatility impliedVolatility(
        QuantLib::Real price,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureSwapCurrency,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureTradeCollateral,
        const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& dpts,
        QuantLib::Real recoveryRate,
        QuantLib::Real accuracy = 1.e-4,
        QuantLib::Size maxEvaluations = 100,
        QuantLib::Volatility minVol = 1.0e-7,
        QuantLib::Volatility maxVol = 4.0) const;

    //! Small struct to hold the results of the implied volatility calculation.
    struct ImpliedVolatilityResult {
        //! The final volatility result from the solver.
        QuantLib::Volatility volatility = QuantLib::Null<QuantLib::Real>();
        //! A flag indicating whether the implied volatility calculation was successful.
        bool success = true;
        //! The error of the implied volatility calculation.
        QuantLib::Real error = QuantLib::Null<QuantLib::Real>();
        //! The error message from the solver in the event of a failure.
        std::string errorMessage;
    };

    /** Calculate implied volatility using any index CDS option engine derived from `IndexCdsOptionBaseEngine`.
     *
     *  The first element in the result is the implied volatility and the second element is a boolean flag indicating
     *  whether the calculation was successful.
     *
     *  \note The volatility handle must be the handle used in the creation of `engine`.
     */
    ImpliedVolatilityResult impliedVolatility(
        QuantLib::Real price,
        QuantLib::ext::shared_ptr<IndexCdsOptionBaseEngine> engine,
        QuantLib::Handle<QuantLib::SimpleQuote> volatility,
        Solver1DOptions solverOptions) const;
    //@}

    /** Allow the strike to be updated.
     *  
     *  This allows `IndexCdsOption` to be used as a helper in the stripping of volatilities from prices.
     */
     void setStrike(QuantLib::Real strike);

private:
    QuantLib::ext::shared_ptr<IndexCreditDefaultSwap> swap_;
    mutable QuantLib::Real strike_;
    CdsOption::StrikeType strikeType_;
    Settlement::Type settlementType_;
    QuantLib::Real tradeDateNtl_;
    QuantLib::Real realisedFep_;
    QuantLib::Period indexTerm_;

    // Results
    mutable QuantLib::Real riskyAnnuity_;
    mutable QuantLib::Real fepAdjustedForwardPrice_;
    mutable QuantLib::Real fepAdjustedForwardSpread_;

    //! \name Instrument interface
    //@{
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results* results) const override;
    //@}
};

//! %Arguments for index CDS option calculation
class IndexCdsOption::arguments : public Option::arguments {
public:
    QuantLib::ext::shared_ptr<IndexCreditDefaultSwap> swap;
    QuantLib::Real strike = QuantLib::Null<QuantLib::Real>();
    CdsOption::StrikeType strikeType = CdsOption::Spread;
    Settlement::Type settlementType = Settlement::Cash;
    QuantLib::Real tradeDateNtl = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real realisedFep = QuantLib::Null<QuantLib::Real>();
    QuantLib::Period indexTerm;

    void validate() const override;
};

//! %Results from index CDS option calculation
class IndexCdsOption::results : public Option::results {
public:
    void reset() override;
    QuantLib::Real riskyAnnuity = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real fepAdjustedForwardPrice = QuantLib::Null<QuantLib::Real>();
    QuantLib::Spread fepAdjustedForwardSpread = QuantLib::Null<QuantLib::Spread>();
};

//! Base class for index CDS option engines
class IndexCdsOption::engine : public QuantLib::GenericEngine<IndexCdsOption::arguments, IndexCdsOption::results> {};

}
