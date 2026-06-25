/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file ored/portfolio/intradaypowerforward.hpp
    \brief Intraday power forward representation

    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/powerloadprofiledata.hpp>
#include <ored/portfolio/trade.hpp>

#include <optional>

namespace ore {
namespace data {

//! Serializable Intraday power forward contract
//! \ingroup tradedata
class IntradayPowerForward : public Trade {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    IntradayPowerForward();

    //! Detailed constructor with explicit future expiry date.
    IntradayPowerForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                         const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                         const QuantLib::Date& deliveryDate, int deliveryStart, int deliveryEnd, bool isDstHour,
                         QuantLib::Real strike, const QuantLib::ext::optional<bool>& physicallySettled = true,
                         const QuantLib::Date& paymentDate = QuantLib::Date());
    //}

    //! \name Inspectors
    //@{
    std::string position() { return position_; }
    std::string commodityName() { return commodityName_; }
    std::string currency() { return currency_; }
    QuantLib::Real quantity() { return quantity_; }
    std::string maturityDate() { return maturityDate_; }
    QuantLib::Real strike() { return strike_; }
    const QuantLib::Date& deliveryDate() const { return deliveryDate_; }
    const std::optional<PowerLoadProfileData>& loadProfileData() const { return loadProfileData_; }
    const QuantLib::ext::optional<bool>& physicallySettled() const { return physicallySettled_; }
    const QuantLib::Date& paymentDate() const { return paymentDate_; }
    //@}

    //! \name Trade interface
    //@{
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real currentNotional() const;

    //! Add underlying Commodity names
    std::map<AssetClass, std::set<std::string>> underlyingIndices(
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string position_;
    std::string commodityName_;
    std::string currency_;
    QuantLib::Real quantity_;
    std::string maturityDate_;
    QuantLib::Real strike_;
    QuantLib::Date deliveryDate_;
    std::string powerLoadProfileReference_;
    std::optional<PowerLoadProfileData> loadProfileData_ = std::nullopt;
    std::optional<int> deliveryStart_ = std::nullopt;
    std::optional<int> deliveryEnd_ = std::nullopt;
    bool isDstHour_ = false;
    QuantLib::ext::optional<bool> physicallySettled_;
    QuantLib::Date paymentDate_;

    QuantLib::Date fixingDate_;
    std::string fxIndex_;
    std::string payCcy_;
};
} // namespace data
} // namespace ore
