/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/model/inflation/infdkdata.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/utilities/parsers.hpp>

using std::vector;

namespace ore {
namespace data {

InfDkData::InfDkData() {}

InfDkData::InfDkData(CalibrationType calibrationType,
    const vector<CalibrationBasket>& calibrationBaskets,
    const std::string& currency,
    const std::string& index,
    const ReversionParameter& reversion,
    const VolatilityParameter& volatility,
    const LgmReversionTransformation& reversionTransformation,
    const bool ignoreDuplicateCalibrationExpiryTimes)
    : InflationModelData(calibrationType, calibrationBaskets, currency, index, ignoreDuplicateCalibrationExpiryTimes),
      reversion_(reversion),
      volatility_(volatility),
      reversionTransformation_(reversionTransformation) {}

const ReversionParameter& InfDkData::reversion() const {
    return reversion_;
}

const VolatilityParameter& InfDkData::volatility() const {
    return volatility_;
}

const LgmReversionTransformation& InfDkData::reversionTransformation() const {
    return reversionTransformation_;
}

void InfDkData::fromXML(XMLNode* node) {
    
    // Check the node is not null and that name is LGM or DodgsonKainth. LGM is for backward compatibility.
    QL_REQUIRE(node, "XML Node should not be null");
    QL_REQUIRE(XMLUtils::getNodeName(node) == "LGM" || XMLUtils::getNodeName(node) == "DodgsonKainth", "Expected " <<
        "node name to be either LGM or DodgsonKainth");

    InflationModelData::fromXML(node);
    reversion_.fromXML(XMLUtils::getChildNode(node, "Reversion"));
    volatility_.fromXML(XMLUtils::getChildNode(node, "Volatility"));

    // Read in calibration instruments to create calibration baskets. We support the legacy XML interface which was 
    // a single CalibrationCapFloors node and the new interface which is a vector of serialisable CalibrationBasket 
    // objects.
    if (XMLNode* n = XMLUtils::getChildNode(node, "CalibrationCapFloors")) {
        // Check that we have not already populated the calibration baskets.
        QL_REQUIRE(calibrationBaskets_.empty(), "Calibration baskets have already been populated.");
        // Legacy interface.
        populateCalibrationBaskets(n);
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "ParameterTransformation")) {
        reversionTransformation_.fromXML(n);
    }
}

XMLNode* InfDkData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("DodgsonKainth");
    InflationModelData::append(doc, node);
    XMLUtils::appendNode(node, reversion_.toXML(doc));
    XMLUtils::appendNode(node, volatility_.toXML(doc));
    XMLUtils::appendNode(node, reversionTransformation_.toXML(doc));

    return node;
}

void InfDkData::populateCalibrationBaskets(XMLNode* node) {

    // Get the values from the XML
    auto type = parseCapFloorType(XMLUtils::getChildValue(node, "CapFloor", true));
    auto maturities = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
    auto strikes = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", true);

    if (!strikes.empty()) {
        
        QL_REQUIRE(strikes.size() == maturities.size(), "Number of maturities and strikes for inflation index " <<
            index() << " should match but got " << strikes.size() << " strikes and " << maturities.size() <<
            " maturities.");
        
        // If any of the strikes are ATM, relabel to ATM/AtmFwd so that parseBaseStrike works below.
        for (string& s : strikes) {
            if (s == "ATM")
                s = "ATM/AtmFwd";
        }
    } else {
        // Default to ATM if no strikes provided.
        strikes.resize(maturities.size(), "ATM/AtmFwd");
    }

    // Create a vector of CPI cap floor calibration instruments.
    vector<QuantLib::ext::shared_ptr<CalibrationInstrument>> instruments;
    for (Size i = 0; i < maturities.size(); ++i) {
        auto p = parseDateOrPeriod(maturities[i]);
        QuantLib::ext::shared_ptr<BaseStrike> s = parseBaseStrike(strikes[i]);
        instruments.push_back(QuantLib::ext::make_shared<CpiCapFloor>(type, p, s));
    }

    // Populate the calibrationBaskets_ member.
    calibrationBaskets_ = { CalibrationBasket(instruments) };
}

}
}
