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

/*! \file portfolio/fxdoublebarrieroption.hpp
   \brief FX Double Barrier Option data model and serialization
   \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ql/instruments/barriertype.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable FX KIKO Barrier Option
/*!
  \ingroup tradedata
*/
class FxKIKOBarrierOption : public FxSingleAssetDerivative {
public:
    //! Default constructor
    FxKIKOBarrierOption()
        : ore::data::Trade("FxKIKOBarrierOption"), FxSingleAssetDerivative(""),
          boughtAmount_(0.0), soldAmount_(0.0) {}
    //! Constructor
    FxKIKOBarrierOption(Envelope& env, OptionData option, vector<BarrierData> barriers, string boughtCurrency,
                        double boughtAmount, string soldCurrency, double soldAmount, string startDate = "",
                        string calendar = "", string fxIndex = "")
        : ore::data::Trade("FxKIKOBarrierOption", env),
          FxSingleAssetDerivative("", env, boughtCurrency, soldCurrency), option_(option),
          barriers_(barriers), startDate_(startDate), calendar_(calendar), fxIndex_(fxIndex),
          boughtAmount_(boughtAmount), soldAmount_(soldAmount) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const vector<BarrierData>& barriers() const { return barriers_; }
    double boughtAmount() const { return boughtAmount_; }
    double soldAmount() const { return soldAmount_; }
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
    bool checkBarrier(Real spot, Barrier::Type type, Real level);
    OptionData option_;
    vector<BarrierData> barriers_;
    string startDate_;
    string calendar_;
    string fxIndex_;
    double boughtAmount_;
    double soldAmount_;
};
} // namespace data
} // namespace oreplus
