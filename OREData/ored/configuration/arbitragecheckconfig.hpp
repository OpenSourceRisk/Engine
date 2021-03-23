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

/*! \file ored/configuration/arbitragecheckconfig.hpp
    \brief arbitrage check configuration
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

class ArbitrageCheckConfig : public XMLSerializable {
public:
    ArbitrageCheckConfig();
    ArbitrageCheckConfig(const std::vector<Period>& tenors, const std::vector<Real>& moneyness)
        : tenors_(tenors), moneyness_(moneyness) {}

    const std::vector<Period>& tenors() const { return tenors_; }
    const std::vector<Real>& moneyness() const { return moneyness_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

private:
    std::vector<Period> tenors_;
    std::vector<Real> moneyness_;
};

} // namespace data
} // namespace ore
