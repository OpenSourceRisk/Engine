/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/commodityforward.hpp
    \brief Instrument representing a commodity forward contract
    
    \ingroup instruments
*/

#ifndef quantext_commodity_forward_hpp
#define quantext_commodity_forward_hpp

#include <ql/currency.hpp>
#include <ql/instrument.hpp>
#include <ql/position.hpp>

namespace QuantExt {

/*! Instrument representing a commodity forward contract.
    
    \ingroup instruments
*/
class CommodityForward : public QuantLib::Instrument {
public:
    class arguments;
    class engine;
    
    //! \name Constructors
    //@{
    /*! \param name         The commodity name.
        \param currency     The currency of the commodity trade.
        \param position     Long (Short) for buying (selling) commodity forward
        \param quantity     Number of commodity units being exchanged
        \param maturityDate Maturity date of forward
        \param strike       The agreed forward price
    */
    CommodityForward(const std::string& name, 
        const QuantLib::Currency& currency,
        QuantLib::Position::Type position,
        QuantLib::Real quantity,
        const QuantLib::Date& maturityDate,
        QuantLib::Real strike);
    //@}

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(QuantLib::PricingEngine::arguments*) const;
    //@}

    //! \name Inspectors
    //@{
    const std::string& name() const { return name_; }
    const QuantLib::Currency& currency() const { return currency_; }
    QuantLib::Position::Type position() const { return position_; }
    QuantLib::Real quantity() const { return quantity_; }
    const QuantLib::Date& maturityDate() const { return maturityDate_; }
    QuantLib::Real strike() const { return strike_; }
    //@}

private:
    std::string name_;
    QuantLib::Currency currency_;
    QuantLib::Position::Type position_;
    QuantLib::Real quantity_;
    QuantLib::Date maturityDate_;
    QuantLib::Real strike_;
};

//! \ingroup instruments
class CommodityForward::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    std::string name;
    QuantLib::Currency currency;
    QuantLib::Position::Type position;
    QuantLib::Real quantity;
    QuantLib::Date maturityDate;
    QuantLib::Real strike;
    void validate() const;
};

//! \ingroup instruments
class CommodityForward::engine : public QuantLib::GenericEngine<CommodityForward::arguments, Instrument::results> {};
}

#endif
