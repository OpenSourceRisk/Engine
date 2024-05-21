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

/*! \file ored/portfolio/equitydoubletouchoption.hpp
    \brief EQ Double One-Touch/No-Touch Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/equityderivative.hpp>

namespace ore {
namespace data {
using std::string;

//! SerializableEQ Double One-Touch/No-Touch Option
/*!
  \ingroup tradedata
*/
class EquityDoubleTouchOption : public EquitySingleAssetDerivative {
public:
    //! Default constructor
    EquityDoubleTouchOption()
        : ore::data::Trade("EquityDoubleTouchOption"), EquitySingleAssetDerivative("") {}
    //! Constructor
    EquityDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier,
                            const EquityUnderlying& equityUnderlying, string payoffCurrency, double payoffAmount,
                            string startDate = "", string calendar = "");

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const BarrierData& barrier() const { return barrier_; }
    double payoffAmount() const { return payoffAmount_; }
    const string& type() const { return type_; }
    const string& payoffCurrency() const { return payoffCurrency_; }
    const string& startDate() const { return startDate_; }
    const string& calendar() const { return calendar_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    bool checkBarrier(Real spot, Barrier::Type type, Real level);

    OptionData option_;
    BarrierData barrier_;
    string startDate_;
    string calendar_;
    Real payoffAmount_;
    string type_;
    string payoffCurrency_;

    // Store some parsed variables needed in the fixings method
    QuantLib::Date start_;
    QuantLib::Calendar cal_;
};
} // namespace data
} // namespace oreplus
