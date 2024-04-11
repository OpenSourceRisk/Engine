/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/eqcommoptionsurfacestripper.hpp
    \brief Imply equity or commodity volatility surface from put/call price surfaces
    \ingroup termstructures
*/

#ifndef quantext_eq_comm_option_surface_stripper_hpp
#define quantext_eq_comm_option_surface_stripper_hpp

#include <qle/indexes/equityindex.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/termstructures/optionpricesurface.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

/*! A simple struct to group together options used by a Solver1D instance.

    This declaration may be moved to a common header if useful for other classes.
*/
struct Solver1DOptions {
    //! The maximum number of evaluations. Default used if not set.
    QuantLib::Size maxEvaluations = QuantLib::Null<QuantLib::Size>();
    //! The accuracy for the search.
    QuantLib::Real accuracy = QuantLib::Null<QuantLib::Real>();
    //! The initial guess for the search.
    QuantLib::Real initialGuess = QuantLib::Null<QuantLib::Real>();
    //! Set the minimum and maximum search.
    std::pair<QuantLib::Real, QuantLib::Real> minMax =
        std::make_pair(QuantLib::Null<QuantLib::Real>(), QuantLib::Null<QuantLib::Real>());
    //! Set the step size for the search.
    QuantLib::Real step = QuantLib::Null<QuantLib::Real>();
    //! The lower bound of the search domain. A \c Null<Real>() indicates that the bound should not be set.
    QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>();
    //! The upper bound of the search domain. A \c Null<Real>() indicates that the bound should not be set.
    QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>();
};

//! Abstract base class for the option stripper
class OptionSurfaceStripper : public QuantLib::LazyObject {

public:
    OptionSurfaceStripper(const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
                          const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
                          const QuantLib::Calendar& calendar,
                          const QuantLib::DayCounter& dayCounter,
                          QuantLib::Exercise::Type type = QuantLib::Exercise::European,
                          bool lowerStrikeConstExtrap = true,
                          bool upperStrikeConstExtrap = true,
                          bool timeFlatExtrapolation = false,
                          bool preferOutOfTheMoney = false,
                          Solver1DOptions solverOptions = {});

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! Return the stripped volatility structure.
    QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> volSurface();

protected:
    //! Generate the relevant Black Scholes process for the underlying.
    virtual QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> process(
        const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& volatilityQuote) const = 0;

    //! Return the forward price at a given date.
    virtual QuantLib::Real forward(const QuantLib::Date& date) const = 0;

    QuantLib::ext::shared_ptr<OptionInterpolatorBase> callSurface_;
    QuantLib::ext::shared_ptr<OptionInterpolatorBase> putSurface_;
    const QuantLib::Calendar& calendar_;
    const QuantLib::DayCounter& dayCounter_;
    QuantLib::Exercise::Type type_;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
    bool timeFlatExtrapolation_;
    bool preferOutOfTheMoney_;

private:
    //! Function object used in solving.
    class PriceError {
    public:
        PriceError(const QuantLib::VanillaOption& option, QuantLib::SimpleQuote& volatility,
            QuantLib::Real targetPrice);

        QuantLib::Real operator()(QuantLib::Volatility volatility) const;

    private:
        const QuantLib::VanillaOption& option_;
        QuantLib::SimpleQuote& volatility_;
        QuantLib::Real targetPrice_;
    };

    //! Retrieve the vector of strikes at a given expiry date.
    std::vector<QuantLib::Real> strikes(const QuantLib::Date& expiry, bool isCall) const;

    /*! Imply the volatility at a given \p expiry and \p strike for the given option \p type. The exercise type is 
        indicated by the member variable \c type_ and the target price is read off the relevant price surface i.e. 
        either \c callSurface_ or \c putSurface_.

        If the root finding fails, a \c Null<Real>() is returned.
    */
    QuantLib::Real implyVol(QuantLib::Date expiry,
        QuantLib::Real strike,
        QuantLib::Option::Type type,
        QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engine,
        QuantLib::SimpleQuote& volQuote) const;

    // Apply the solver options.
    void setUpSolver();

    mutable QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> volSurface_;

    //! Solver used when implying volatility from price.
    Brent brent_;
    Solver1DOptions solverOptions_;

    //! Set to \c true if we must strip volatilities from prices.
    bool havePrices_;

    //! Store the function that will be called each time to solve for volatility
    std::function<Real(const PriceError&)> solver_;
};

class EquityOptionSurfaceStripper : public OptionSurfaceStripper {

public:
    EquityOptionSurfaceStripper(const QuantLib::Handle<QuantExt::EquityIndex2>& equityIndex,
        const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
        const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
        const QuantLib::Calendar& calendar,
        const QuantLib::DayCounter& dayCounter,
        QuantLib::Exercise::Type type = QuantLib::Exercise::European,
        bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true,
        bool timeFlatExtrapolation = false,
        bool preferOutOfTheMoney = false,
        Solver1DOptions solverOptions = {});

protected:
    //! \name OptionSurfaceStripper interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> process(
        const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& volatilityQuote) const override;

    QuantLib::Real forward(const QuantLib::Date& date) const override;
    //@}

private:
    QuantLib::Handle<QuantExt::EquityIndex2> equityIndex_;
};

class CommodityOptionSurfaceStripper : public OptionSurfaceStripper {

public:
    CommodityOptionSurfaceStripper(
        const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
        const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
        const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
        const QuantLib::Calendar& calendar,
        const QuantLib::DayCounter& dayCounter,
        QuantLib::Exercise::Type type = QuantLib::Exercise::European,
        bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true,
        bool timeFlatExtrapolation = false,
        bool preferOutOfTheMoney = false,
        Solver1DOptions solverOptions = {});

protected:
    //! \name OptionSurfaceStripper interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> process(
        const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& volatilityQuote) const override;

    QuantLib::Real forward(const QuantLib::Date& date) const override;
    //@}

private:
    QuantLib::Handle<QuantExt::PriceTermStructure> priceCurve_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
};

}
#endif
