/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/reportconfig.hpp
    \brief md report and arbitrage check configuration
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

#include <boost/optional.hpp>

namespace ore {
namespace data {

class SmileDynamicsConfig : public XMLSerializable {
public:

    SmileDynamicsConfig(const std::string& swaption = "StickyStrike", const std::string& capFloor = "StickyStrike",
			const std::string& yield = "StickyStrike", const std::string& zeroInflationCapFloor = "StickyStrike",
			const std::string& yoyInflationCapFloor = "StickyStrike", const std::string& equity = "StickyStrike",
			const std::string& commodity = "StickyStrike", const std::string& fx = "StickyStrike",
			const std::string& cds = "StickyStrike")
      : swaption_(swaption), capFloor_(capFloor), yield_(yield), zeroInflationCapFloor_(zeroInflationCapFloor),
	yoyInflationCapFloor_(yoyInflationCapFloor), equity_(equity), commodity_(commodity), fx_(fx), cds_(cds) {
      validate();
    }

    const std::string& swaption() const { return swaption_; }
    const std::string& capFloor() const { return capFloor_; }
    const std::string& yield() const { return yield_; }
    const std::string& zeroInflationCapFloor() const { return zeroInflationCapFloor_; }
    const std::string& yoyInflationCapFloor() const { return yoyInflationCapFloor_; }
    const std::string& equity() const { return equity_; }
    const std::string& commodity() const { return commodity_; }
    const std::string& fx() const { return fx_; }
    const std::string& cds() const { return cds_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

private:
    void validate();
    std::string swaption_;
    std::string capFloor_;
    std::string yield_;
    std::string zeroInflationCapFloor_;
    std::string yoyInflationCapFloor_;
    std::string equity_;
    std::string commodity_;
    std::string fx_;
    std::string cds_;
};

} // namespace data
} // namespace ore
