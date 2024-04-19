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

/*! \file ored/portfolio/fxbarrieroption.hpp
    \brief FX Barrier Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/barrierdata.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable FX Digital Barrier Option
/*!
  \ingroup tradedata
*/
class FxDigitalBarrierOption : public FxSingleAssetDerivative {
public:
    //! Default constructor
    FxDigitalBarrierOption()
        : ore::data::Trade("FxDigitalBarrierOption"), FxSingleAssetDerivative("") {}
    //! Constructor
    FxDigitalBarrierOption(Envelope& env, OptionData option, BarrierData barrier, double strike, double payoffAmount,
                           const string& foreignCurrency, const string& domesticCurrency, const string& startDate = "",
                           const string& calendar = "", const string& fxIndex = "", const string& payoffCurrency = "")
        : ore::data::Trade("FxDigitalBarrierOption", env),
          FxSingleAssetDerivative("", env, foreignCurrency, domesticCurrency), option_(option),
          barrier_(barrier), startDate_(startDate), calendar_(calendar), fxIndex_(fxIndex), strike_(strike),
          payoffAmount_(payoffAmount), payoffCurrency_(payoffCurrency) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const BarrierData& barrier() const { return barrier_; }
    double strike() const { return strike_; }
    double payoffAmount() const { return payoffAmount_; }
    const string& payoffCurrency() const { return payoffCurrency_; }
    const string& startDate() const { return startDate_; }
    const string& calendar() const { return calendar_; }
    const string& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    OptionData option_;
    BarrierData barrier_;
    string startDate_;
    string calendar_;
    string fxIndex_;
    Real strike_;
    Real payoffAmount_;
    string payoffCurrency_;
};
} // namespace data
} // namespace oreplus
