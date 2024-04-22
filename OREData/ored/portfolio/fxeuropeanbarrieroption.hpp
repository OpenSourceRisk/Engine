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

/*! \file ored/portfolio/fxeuropeanbarrieroption.hpp
   \brief FX European Barrier Option data model and serialization
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

//! Serializable FX European Barrier Option
/*!
  \ingroup tradedata
*/
class FxEuropeanBarrierOption : public FxSingleAssetDerivative {
public:
    //! Default constructor
    FxEuropeanBarrierOption()
        : ore::data::Trade("FxEuropeanBarrierOption"), FxSingleAssetDerivative("FxEuropeanBarrierOption"),
          boughtAmount_(0.0), soldAmount_(0.0) {}
    //! Constructor
    FxEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier, string boughtCurrency,
                            double boughtAmount, string soldCurrency, double soldAmount, string startDate = "",
                            string calendar = "", string fxIndex = "")
        : ore::data::Trade("FxEuropeanBarrierOption", env),
          FxSingleAssetDerivative("FxEuropeanBarrierOption", env, boughtCurrency, soldCurrency), option_(option),
          barrier_(barrier), boughtAmount_(boughtAmount), soldAmount_(soldAmount), fxIndex_(fxIndex) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const BarrierData& barrier() const { return barrier_; }
    double boughtAmount() const { return boughtAmount_; }
    double soldAmount() const { return soldAmount_; }
    const std::string& fxIndex() const { return fxIndex_; }
    Real strike() const;
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
    double boughtAmount_;
    double soldAmount_;
    //! If the option has automatic exercise (i.e. cash settled after maturity), need an FX index for settlement.
    std::string fxIndex_;
};
} // namespace data
} // namespace oreplus
