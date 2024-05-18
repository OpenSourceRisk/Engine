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

/*! \file portfolio/fxforward.hpp
    \brief FX Forward data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

namespace ore {
namespace data {

//! Serializable FX Forward
/*!
  \ingroup tradedata
*/
class FxForward : public Trade {
public:
    //! Default constructor
    FxForward() : Trade("FxForward"), boughtAmount_(0.0), soldAmount_(0.0) {}
    //! Constructor
    FxForward(const Envelope& env, const string& maturityDate, const string& boughtCurrency, double boughtAmount,
              const string& soldCurrency, double soldAmount, const string& settlement = "Physical",
              const string& fxIndex = "", const string& payDate = "")
        : Trade("FxForward", env), maturityDate_(maturityDate), boughtCurrency_(boughtCurrency),
          boughtAmount_(boughtAmount), soldCurrency_(soldCurrency), soldAmount_(soldAmount), settlement_(settlement),
          fxIndex_(fxIndex), payDate_(payDate) {}

    bool isExpired(const Date& d) override;
    
    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

    //! \name Inspectors
    //@{
    const string& maturityDate() const { return maturityDate_; }
    const string& boughtCurrency() const { return boughtCurrency_; }
    double boughtAmount() const { return boughtAmount_; }
    const string& soldCurrency() const { return soldCurrency_; }
    double soldAmount() const { return soldAmount_; }
    //! Settlement Type can be set to "Cash" for NDF. Default value is "Physical"
    const string& settlement() const { return settlement_; }
    const string& fxIndex() const { return fxIndex_; }
    const string& paymentDate() const { return payDate_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    string maturityDate_;
    string boughtCurrency_;
    double boughtAmount_;
    string soldCurrency_;
    double soldAmount_;
    string settlement_;
    string payCurrency_;
    string fxIndex_;
    string payDate_;
    string payLag_;
    string payCalendar_;
    string payConvention_;
    bool includeSettlementDateFlows_;
};
} // namespace data
} // namespace ore
