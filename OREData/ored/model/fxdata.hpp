/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file model/fxdata.hpp
    \brief FX component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <vector>

namespace ore {
namespace data {

using namespace QuantLib;

//! FX Model Parameters
/*!  Base class for specification of a FX model component in the cross asset model.
  \ingroup models
*/
class FxData : public XMLSerializable {
public:
    FxData() = default;
    virtual ~FxData() = default;

    FxData(std::string foreignCcy, std::string domesticCcy, std::string name)
        : foreignCcy_(std::move(foreignCcy)), domesticCcy_(std::move(domesticCcy)), name_(std::move(name)) {}

    //! \name Setters/Getters
    //@{
    const std::string& foreignCcy() const { return foreignCcy_; }
    const std::string& domesticCcy() const { return domesticCcy_; }
    void setDomesticCcy(std::string ccy) { domesticCcy_ = std::move(ccy); }
    void setForeignCcy(std::string ccy) { foreignCcy_ = std::move(ccy); }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    bool operator==(const FxData& rhs) const = default;
    bool operator!=(const FxData& rhs) const = default;

    virtual QuantLib::ext::shared_ptr<FxData> clone(std::string foreignCcy) const = 0;

protected:
    std::string foreignCcy_;
    std::string domesticCcy_;
    std::string name_;
};

} // namespace data
} // namespace ore
