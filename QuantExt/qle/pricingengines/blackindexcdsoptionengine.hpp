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

/*! \file qle/pricingengines/blackindexcdsoptionengine.hpp
    \brief Black index credit default swap option engine.
*/

#pragma once

#include <qle/instruments/indexcdsoption.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/creditvolcurve.hpp>

namespace QuantExt {

/*! Black index CDS option engine

    Prices index CDS option instruments quoted in terms of strike spread or strike price. If the strike is in terms
    of spread, it is assumed that the volatility structure's strike dimension, if there is one, is in terms of spread
    also. This is the standard quotation convention for investment grade index families like CDX IG and ITraxx Europe.
    If the strike is in terms of price, it is assumed that the volatility structure's strike dimension, if there is
    one, is in terms of price also. This is the standard quotation convention for high yield index families like
    CDX HY and CDX EM.

    The valuation of the index CDS options with strike price is a reasonably straightforward application of Black's
    formula. The approach is outlined for example in <em>Mark-to-market Credit Index Option Pricing and
    Credit Volatility Index, John Yang and Lukasz Dobrek, 23 June 2015, Section 1.1</em>. Here, we calculate the
    front end protection (FEP) adjusted forward price as opposed to deriving it from the market quotes of payer and
    receiver CDS options with the same strike.

    The valuation of the index CDS options with strike spread follows the approach outlined in <em>Modelling
    Single-name and Multi-name Credit Derivatives, Dominic O'Kane, 2008, Section 11.7</em>. This is also the approach
    outlined in <em>Credit Index Option, ICE, 2018</em>.
*/
class BlackIndexCdsOptionEngine : public QuantExt::IndexCdsOption::engine {
public:
    //! Constructor taking a default probability term structure bootstrapped from the index spreads.
    BlackIndexCdsOptionEngine(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability,
                              QuantLib::Real recovery, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                              const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility);

    /*! Constructor taking a vector of default probability term structures bootstrapped from the index constituent
        spread curves and a vector of associated recovery rates.
    */
    BlackIndexCdsOptionEngine(
        const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& probabilities,
        const std::vector<QuantLib::Real>& recoveries, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility,
        QuantLib::Real indexRecovery = QuantLib::Null<QuantLib::Real>());

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& probabilities() const;
    const std::vector<QuantLib::Real>& recoveries() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure> discount() const;
    const QuantLib::Handle<QuantExt::CreditVolCurve> volatility() const;
    //@}

    //! \name Instrument interface
    //@{
    void calculate() const override;
    //@}

private:
    std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> probabilities_;
    std::vector<QuantLib::Real> recoveries_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discount_;
    QuantLib::Handle<QuantExt::CreditVolCurve> volatility_;

    //! Assumed index recovery used in the flat strike spread curve calculation if provided.
    QuantLib::Real indexRecovery_;

    //! Store the underlying index CDS notional(s) during calculation.
    mutable std::vector<QuantLib::Real> notionals_;

    //! Calculate the discounted value of the front end protection.
    QuantLib::Real fep() const;

    //! Calculation for instrument with strike quoted as spread.
    void spreadStrikeCalculate(QuantLib::Real fep) const;

    //! Calculation for instrument with strike quoted as price.
    void priceStrikeCalculate(QuantLib::Real fep) const;

    //! Calculate the forward risky annuity based on the strike spread.
    QuantLib::Real forwardRiskyAnnuityStrike() const;

    //! Register with the market data
    void registerWithMarket();
};

} // namespace QuantExt
