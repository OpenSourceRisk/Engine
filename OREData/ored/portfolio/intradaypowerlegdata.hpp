/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file ored/portfolio/intradaypowerlegdata.hpp
    \brief leg data for intraday power floating leg
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/powerloadprofiledata.hpp>

namespace ore {
namespace data {

class IntradayPowerLegData : public ore::data::LegAdditionalData {

public:
    //! Default constructor
    IntradayPowerLegData();

    //! Constructor
    IntradayPowerLegData(const std::string& name, const std::vector<QuantLib::Real>& quantities,
                         const std::vector<std::string>& quantityDates = {},
                         const std::vector<QuantLib::Real>& spreads = {},
                         const std::vector<std::string>& spreadDates = {},
                         const std::vector<QuantLib::Real>& gearings = {},
                         const std::vector<std::string>& gearingDates = {},
                         const std::string& pricingCalendar = std::string(), bool includePeriodStart = true,
                         bool includePeriodEnd = false, const PowerLoadProfileData& loadProfileData = PowerLoadProfileData(),
                         const std::string& fxIndex = std::string(),
                         QuantLib::Natural avgPricePrecision = QuantLib::Null<QuantLib::Natural>());

    //! \name Inspectors
    //@{
    const std::string& name() const { return name_; }
    const std::vector<QuantLib::Real>& quantities() const { return quantities_; }
    const std::vector<std::string>& quantityDates() const { return quantityDates_; }
    const std::vector<QuantLib::Real>& spreads() const { return spreads_; }
    const std::vector<std::string>& spreadDates() const { return spreadDates_; }
    const std::vector<QuantLib::Real>& gearings() const { return gearings_; }
    const std::vector<std::string>& gearingDates() const { return gearingDates_; }
    const std::string& pricingCalendar() const { return pricingCalendar_; }
    bool includePeriodStart() const { return includePeriodStart_; }
    bool includePeriodEnd() const { return includePeriodEnd_; }
    const PowerLoadProfileData& loadProfileData() const { return loadProfileData_; }
    const std::string& fxIndex() const { return fxIndex_; }
    QuantLib::Natural avgPricePrecision() const { return avgPricePrecision_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string name_;
    std::vector<QuantLib::Real> quantities_;
    std::vector<std::string> quantityDates_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<std::string> spreadDates_;
    std::vector<QuantLib::Real> gearings_;
    std::vector<std::string> gearingDates_;
    std::string pricingCalendar_;
    bool includePeriodStart_;
    bool includePeriodEnd_;
    PowerLoadProfileData loadProfileData_;
    std::string fxIndex_;
    QuantLib::Natural avgPricePrecision_;
};

} // namespace data
} // namespace ore
