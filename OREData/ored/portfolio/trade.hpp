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

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/tradeactions.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflow.hpp>
#include <ql/instrument.hpp>
#include <ql/time/date.hpp>

using std::string;
using ore::data::XMLSerializable;

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
        : tradeType_(tradeType), envelope_(env), tradeActions_(ta) {
        reset();
    }

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
        notional_ = 0.0;
        maturity_ = Date();
        tradeActions_.clear();
    }

    //! Set the trade id
    string& id() { return id_; }

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }

    const string& tradeType() const { return tradeType_; }

    const Envelope& envelope() const { return envelope_; }

    const set<string>& portfolioIds() const { return envelope().portfolioIds(); }

    const TradeActions& tradeActions() { return tradeActions_; }

    const boost::shared_ptr<InstrumentWrapper>& instrument() { return instrument_; }

    const std::vector<QuantLib::Leg>& legs() { return legs_; }

    const std::vector<string>& legCurrencies() { return legCurrencies_; }

    const std::vector<bool>& legPayers() { return legPayers_; }

    const string& npvCurrency() { return npvCurrency_; }

    //! Return the current notional in npvCurrency. See individual sub-classes for the precise definition
    // of notional, for exotic trades this may not be what you expect.
    QuantLib::Real notional() { return notional_; }

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
    QuantLib::Real notional_;
    Date maturity_;

    // Utility to add a single (fee, option premium, etc.) payment such that it is taken into account in pricing and
    // cash flow projection. For example, an option premium flow is not covered by the underlying option instrument in
    // QuantLib and needs to be represented separately. This is done by inserting it as an additional instrument
    // into the InstrumentWrapper. This utility creates the additional instrument. The actual insertion into the
    // instrument wrapper is done in the individual trade builders when they instantiate the InstrumentWrapper.
    void addPayment(std::vector<boost::shared_ptr<Instrument>>& instruments, std::vector<Real>& multipliers,
                    const Date& paymentDate, const Real& paymentAmount, const Currency& paymentCurrency,
                    const Currency& tradeCurrency, const boost::shared_ptr<EngineFactory>& factory,
                    const string& configuration);

private:
    string id_;
    Envelope envelope_;
    TradeActions tradeActions_;
};
} // namespace data
} // namespace ore
