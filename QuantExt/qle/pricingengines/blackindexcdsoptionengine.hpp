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

#include <qle/pricingengines/indexcdsoptionbaseengine.hpp>

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
class BlackIndexCdsOptionEngine : public QuantExt::IndexCdsOptionBaseEngine {
public:
    using IndexCdsOptionBaseEngine::IndexCdsOptionBaseEngine;

private:
    void doCalc() const override;

    void spreadStrikeCalculate(QuantLib::Real fep) const;
    void priceStrikeCalculate(QuantLib::Real fep) const;
    QuantLib::Real forwardRiskyAnnuityStrike() const;
};

} // namespace QuantExt
