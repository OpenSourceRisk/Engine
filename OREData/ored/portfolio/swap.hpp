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

/*! \file portfolio/swap.hpp
    \brief Swap trade data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Swap, Single and Cross Currency
/*!
  \ingroup tradedata
*/
class Swap : public Trade {
public:
    //! Default constructor
    Swap(const string swapType = "Swap") : Trade(swapType) {}

    Swap(const Envelope& env, const string swapType = "Swap") : Trade(swapType, env) {}

    //! Constructor with vector of LegData
    Swap(const Envelope& env, const vector<LegData>& legData, const string swapType = "Swap",
         const std::string settlement = "Physical")
        : Trade(swapType, env), legData_(legData), settlement_(settlement) {}

    //! Constructor with two legs
    Swap(const Envelope& env, const LegData& leg0, const LegData& leg1, const string swapType = "Swap",
         const std::string settlement = "Physical")
        : Trade(swapType, env), legData_({leg0, leg1}), settlement_(settlement) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    virtual void setIsdaTaxonomyFields();
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

    //! Add underlying index names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! Settlement Type can be set to "Cash" for NDF. Default value is "Physical"
    const string& settlement() const { return settlement_; }

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const vector<LegData>& legData() const { return legData_; }
    //@}

    const std::map<std::string,boost::any>& additionalData() const override;

protected:
    virtual QuantLib::ext::shared_ptr<LegData> createLegData() const;

    vector<LegData> legData_;
    string settlement_;
    bool isXCCY_ = false;

private:
    bool isResetting_;
    Size notionalTakenFromLeg_;
};

std::string isdaSubProductSwap(const std::string& tradeId, const vector<LegData>& legData);

} // namespace data
} // namespace ore
