/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/equityoutperformanceoption.hpp
    \brief EQ Outperformance Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/equityderivative.hpp>
#include <ored/portfolio/barrierdata.hpp>

#include <ored/portfolio/optiondata.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable EQ Outperformance Option
/*!
  \ingroup tradedata
*/
class EquityOutperformanceOption : public ore::data::Trade {
public:
    //! Default constructor
    EquityOutperformanceOption() : Trade("EquityOutperformanceOption") {}
    //! Constructor
    EquityOutperformanceOption(Envelope& env, OptionData option, const string& currency, Real notional, 
        const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying1, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying2, 
        Real initialPrice1, Real initialPrice2, Real strike, const string& initialPriceCurrency1 = "", const string& initialPriceCurrency2 = "",
        Real knockInPrice = Null<Real>(), Real knockOutPrice = Null<Real>(), string fxIndex1 = "", string fxIndex2 = "" );

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const string& currency() const { return currency_; }
    const std::string& name1() const { return underlying1_->name(); }
    const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying1() const { return underlying1_; }
    const std::string& name2() const { return underlying2_->name(); }
    const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying2() const { return underlying2_; }
    
    const Real& initialPrice1() const { return initialPrice1_; }
    const Real& initialPrice2() const { return initialPrice2_; }

    const string& initialPriceCurrency1() const { return initialPriceCurrency1_; }
    const string& initialPriceCurrency2() const { return initialPriceCurrency2_; }
    
    const Real& strikeReturn() const { return strikeReturn_; }

    const Real& knockInPrice() const { return knockInPrice_; }
    const Real& knockOutPrice() const { return knockOutPrice_; }

    const string& fxIndex1() const { return fxIndex1_; }
    const string& fxIndex2() const { return fxIndex2_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    OptionData option_;
    string currency_;
    Real amount_;
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying1_;    
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying2_;    
    Real initialPrice1_;
    Real initialPrice2_;
    Real strikeReturn_;
    Real knockInPrice_;
    Real knockOutPrice_;
    string initialPriceCurrency1_ = "";
    string initialPriceCurrency2_ = "";
    string fxIndex1_ = "";
    string fxIndex2_ = "";
        
};
} // namespace data
} // namespace ore
