/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/trade.hpp
    \brief base trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/tradeactions.hpp>
#include <ql/time/date.hpp>
#include <ql/instrument.hpp>
#include <ql/cashflow.hpp>

using std::string;
using ore::data::XMLSerializable;
;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using QuantLib::Date;

namespace ore {
namespace data {

//! Trade base class
/*! Instrument interface to pricing and risk applications
    Derived classes should
    - contain additional serializable data classes
    - implement a build() function that parses data and constructs QuantLib
      and QuantExt objects
*/
class Trade : public XMLSerializable {
public:
    //! Base class constructor
    Trade(const string& tradeType, const Envelope& env = Envelope(), const TradeActions& ta = TradeActions())
        : tradeType_(tradeType), envelope_(env), tradeActions_(ta) {}

    //! Default destructor
    virtual ~Trade() {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const boost::shared_ptr<EngineFactory>&) = 0;

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! Reset trade, clear all base class data
    void reset() {
        // This is now being done in the build() function.
        // instrument_ = boost::shared_ptr<QuantLib::Instrument>(); //TODO Wrapper
        legs_.clear();
        legCurrencies_.clear();
        legPayers_.clear();
        npvCurrency_ = "";
        maturity_ = Date();
        tradeActions_.clear();
    }

    //! Set the trade id
    string& id() { return id_; }

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }

    const string& tradeType() const { return tradeType_; }

    const Envelope& envelope() { return envelope_; }

    const TradeActions& tradeActions() { return tradeActions_; }

    const boost::shared_ptr<InstrumentWrapper>& instrument() { return instrument_; }

    const std::vector<QuantLib::Leg>& legs() { return legs_; }

    const std::vector<string>& legCurrencies() { return legCurrencies_; }

    const std::vector<bool>& legPayers() { return legPayers_; }

    const string& npvCurrency() { return npvCurrency_; }

    const Date& maturity() { return maturity_; }
    //@}

protected:
    // protected members, to be set by build functions of derived classes
    string tradeType_; // class name of the derived class
    boost::shared_ptr<InstrumentWrapper> instrument_;
    std::vector<QuantLib::Leg> legs_;
    std::vector<string> legCurrencies_;
    std::vector<bool> legPayers_;
    string npvCurrency_;
    Date maturity_;

private:
    string id_;
    Envelope envelope_;
    TradeActions tradeActions_;
};
}
}
