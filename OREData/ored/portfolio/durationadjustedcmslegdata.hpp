/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file durationadjustedcmslegdata.hpp
    \brief leg data for duration adjusted cms
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

class DurationAdjustedCmsLegData : public ore::data::LegAdditionalData {

public:
    DurationAdjustedCmsLegData()
        : ore::data::LegAdditionalData("DurationAdjustedCMS"), fixingDays_(Null<Size>()), isInArrears_(true),
          nakedOption_(false) {}

    //! Detailed constructor
    DurationAdjustedCmsLegData(const std::string& swapIndex, Size duration, Size fixingDays, bool isInArrears,
                               const std::vector<double>& spreads,
                               const std::vector<std::string>& spreadDates = std::vector<std::string>(),
                               const std::vector<double>& caps = std::vector<double>(),
                               const std::vector<std::string>& capDates = std::vector<std::string>(),
                               const std::vector<double>& floors = std::vector<double>(),
                               const std::vector<std::string>& floorDates = std::vector<std::string>(),
                               const std::vector<double>& gearings = std::vector<double>(),
                               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
                               bool nakedOption = false)
        : LegAdditionalData("CMS"), swapIndex_(swapIndex), duration_(duration), fixingDays_(fixingDays),
          isInArrears_(isInArrears), spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates),
          floors_(floors), floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates),
          nakedOption_(nakedOption) {
        indices_.insert(swapIndex_);
    }

    //! \name Inspectors
    //@{
    const std::string& swapIndex() const { return swapIndex_; }
    Size duration() const { return duration_; }
    Size fixingDays() const { return fixingDays_; }
    bool isInArrears() const { return isInArrears_; }
    const std::vector<double>& spreads() const { return spreads_; }
    const std::vector<std::string>& spreadDates() const { return spreadDates_; }
    const std::vector<double>& caps() const { return caps_; }
    const std::vector<std::string>& capDates() const { return capDates_; }
    const std::vector<double>& floors() const { return floors_; }
    const std::vector<std::string>& floorDates() const { return floorDates_; }
    const std::vector<double>& gearings() const { return gearings_; }
    const std::vector<std::string>& gearingDates() const { return gearingDates_; }
    bool nakedOption() const { return nakedOption_; }
    //@}

    //! \name Modifiers
    //@{
    vector<double>& caps() { return caps_; }
    vector<string>& capDates() { return capDates_; }
    vector<double>& floors() { return floors_; }
    vector<string>& floorDates() { return floorDates_; }
    bool& nakedOption() { return nakedOption_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    std::string swapIndex_;
    Size duration_;
    Size fixingDays_;
    bool isInArrears_;
    std::vector<double> spreads_;
    std::vector<std::string> spreadDates_;
    std::vector<double> caps_;
    std::vector<std::string> capDates_;
    std::vector<double> floors_;
    std::vector<std::string> floorDates_;
    std::vector<double> gearings_;
    std::vector<std::string> gearingDates_;
    bool nakedOption_;
};

} // namespace data
} // namespace ore
