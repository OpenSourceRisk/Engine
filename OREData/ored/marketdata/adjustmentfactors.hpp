/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/marketdata/inmemoryloader.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

//! Class to hold market data adjustment factors - for example equity stock splits
class AdjustmentFactors : public XMLSerializable {
public:
    AdjustmentFactors(QuantLib::Date asof) : asof_(asof){};

    //! Check if we have any adjustment factors for a name
    bool hasFactor(const std::string& name) const;
    //! Returns the adjustment factor for a name on a given date
    QuantLib::Real getFactor(const std::string& name, const QuantLib::Date& d) const;
    //! Add an adjustment factor
    void addFactor(std::string name, QuantLib::Date d, QuantLib::Real factor);

    //! \name Serilaisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! names with adjustment factors
    std::set<std::string> names() const;
    //! dates with contributions to an adjustment factor for a name
    std::set<QuantLib::Date> dates(const std::string& name) const;
    //! gets the contribution to an adjustment factor for a name on a given date
    QuantLib::Real getFactorContribution(const std::string& name, const QuantLib::Date& d) const;

private:
    //! Asof date - only apply adjustments before this date
    QuantLib::Date asof_;
    //! Map of names to adjustment factors
    std::map<std::string, std::vector<std::pair<QuantLib::Date, QuantLib::Real>>> data_;
};

} // namespace data
} // namespace ore
