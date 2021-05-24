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

/*! \file qle/pricingengines/blackcdsoptionengine.hpp
    \brief Black credit default swap option engine.
    \ingroup engines
*/

#ifndef quantext_black_cds_option_engine_hpp
#define quantext_black_cds_option_engine_hpp

#include <qle/instruments/cdsoption.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {

/*! Black single name CDS option engine

    Prices single name CDS option instruments quoted in terms of strike spread. It is assumed that the volatility 
    structure's strike dimension, if there is one, is in terms of spread also. This is the standard situation for 
    single name CDS options.

    The valuation follows the approach outlined in <em>Modelling Single-name and Multi-name Credit Derivatives, 
    Dominic O'Kane, 2008, Section 9.3.7</em>. This is also the approach in <em>A CDS Option Miscellany, Richard 
    J. Martin, 2019, Section 2.1 and 2.2</em>. If we need the approach in Section 2.4 of that paper, we would need 
    to make adjustments to the forward spread and RPV01 in our calculation which may in turn need access to the ISDA 
    supplied interest rate curve. We leave that as a possible future enhancement.
*/
class BlackCdsOptionEngine : public QuantExt::CdsOption::engine {
public:
    BlackCdsOptionEngine(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability,
        QuantLib::Real recovery,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volatility);

    //! \name Inspectors
    //@{
    const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability() const;
    QuantLib::Real recovery() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure> discount() const;
    const QuantLib::Handle<QuantLib::BlackVolTermStructure> volatility() const;
    //@}

    //! \name Instrument interface
    //@{
    void calculate() const override;
    //@}

private:
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> probability_;
    QuantLib::Real recovery_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discount_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volatility_;

};

}

#endif
