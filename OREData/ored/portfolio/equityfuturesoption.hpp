/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
 */

/*! \file portfolio/equityfuturesoption.hpp
    \brief EQ Futures Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/vanillaoption.hpp>

namespace ore {
namespace data {
using std::string;
using namespace ore::data;

//! Serializable EQ Futures Option
/*!
  \ingroup tradedata
*/
class EquityFutureOption : public VanillaOptionTrade {
public:

    //! Default constructor
    EquityFutureOption() : VanillaOptionTrade(AssetClass::EQ) { tradeType_ = "EquityFutureOption"; }
    //! Constructor
    EquityFutureOption(Envelope& env, OptionData option, const string& currency, Real quantity, 
        const boost::shared_ptr<ore::data::Underlying>& underlying, Real strike, QuantLib::Date forwardDate,
        const boost::shared_ptr<QuantLib::Index>& index = nullptr, const std::string& indexName = "");

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const std::string& name() const { return underlying_->name(); }
    const boost::shared_ptr<ore::data::Underlying>& underlying() const { return underlying_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

private:
    boost::shared_ptr<ore::data::Underlying> underlying_;    
        
};
} // namespace data
} // namespace ore
